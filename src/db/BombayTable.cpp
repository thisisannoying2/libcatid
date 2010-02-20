/*
	Copyright (c) 2009-2010 Christopher A. Taylor.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of LibCat nor the names of its contributors may be used
	  to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include <cat/db/BombayTable.hpp>
#include <cat/port/AlignedAlloc.hpp>
#include <cat/io/Logging.hpp>
using namespace cat;
using namespace bombay;

// Hash 64-bit file offset to a 32-bit hash table key
static CAT_INLINE u32 GetHash64(u64 key)
{
	key = (~key) + (key << 18);
	key = key ^ (key >> 31);
	key = key * 21;
	key = key ^ (key >> 11);
	key = key + (key << 6);
	key = key ^ (key >> 22);
	return (u32)key;
}

Table::Table(const char *file_path, u32 record_bytes, u32 cache_bytes, ShutdownObserver *shutdown_observer)
	: AsyncFile(REFOBJ_PRIO_0+16)
{
	_shutdown_observer = shutdown_observer;
	if (shutdown_observer)
		shutdown_observer->AddRef();

	CAT_STRNCPY(_file_path, file_path, sizeof(_file_path));
	_record_bytes = record_bytes;

	// Set cache bytes to a multiple of record bytes with cache node header
	u32 node_bytes = sizeof(CacheNode) + record_bytes;
	_index_read_size = MAX_INDEX_READ_SIZE - (MAX_INDEX_READ_SIZE % node_bytes);
	_cache_bytes = cache_bytes - (cache_bytes % node_bytes);

	// Calculate hash table size
	u32 node_count = _cache_bytes / node_bytes;
	u32 hash_table_size = node_count / TARGET_TREE_SIZE;
	if (hash_table_size < MIN_TABLE_SIZE)
		_hash_table_size = MIN_TABLE_SIZE;
	else
	{
		// Round hash table size up to the next highest power-of-2
		hash_table_size |= hash_table_size >> 1;
		hash_table_size |= hash_table_size >> 2;
		hash_table_size |= hash_table_size >> 4;
		hash_table_size |= hash_table_size >> 8;
		hash_table_size |= hash_table_size >> 16;
		_hash_table_size = hash_table_size + 1;
	}

	_cache_hash_table = 0;
	_cache = 0;
	_cache_full = false;
	_next_record = 0;
	_next_cache_slot = 0;

	_head_index_waiting = 0;
	_head_index_update = 0;
	_head_index = 0;
	_head_index_unique = 0;
}

Table::~Table()
{
	// Release all indices
	for (TableIndex *index = _head_index, *next; index; index = next)
	{
		next = index->_next;
		index->Save();
		index->ReleaseRef();
	}

	FreeCache();

	if (_shutdown_observer)
		_shutdown_observer->ReleaseRef();
}

bool Table::AllocateCache()
{
	FreeCache();

	INFO("Table") << "Allocating a " << _hash_table_size << "-element hash table for " << _file_path << ", and " << _cache_bytes << " bytes of cache memory";

	// Allocate hash table memory
	_cache_hash_table = new CacheNode*[_hash_table_size];
	if (!_cache_hash_table) return false;

	// Clear hash table memory
	memset(_cache_hash_table, 0, _hash_table_size * sizeof(CacheNode*));

	// Allocate cache memory
	_cache = (u8*)LargeAligned::Acquire(_cache_bytes);
	return _cache != 0;
}

void Table::FreeCache()
{
	if (_cache_hash_table)
	{
		delete []_cache_hash_table;
		_cache_hash_table = 0;
	}

	if (_cache)
	{
		LargeAligned::Release(_cache);
		_cache = 0;
	}
}

CacheNode *Table::FindNode(u64 offset)
{
	// Get root node for this offset
	u32 key = GetHash64(offset) & (_hash_table_size - 1);
	CacheNode *node = _cache_hash_table[key];

	// Search for node or approximation of insertion point
	while (node)
	{
		// If node was found in cache (yay!),
		if (node->offset == offset)
			return node;
		else if (offset > node->offset)
			node = node->higher;
		else
			node = node->lower;
	}

	return 0;
}

void Table::UnlinkNode(CacheNode *node)
{
	CacheNode *parent = node->parent;
	CacheNode *lower = node->lower;
	CacheNode *higher = node->higher;

	// If the node is not at the root,
	if (parent)
	{
		// If the node is the lower branch of its parent,
		if (parent->lower == node)
		{
			if (!lower)
			{
				// Re-link the higher child tree to its parent
				parent->lower = higher;
				if (higher) higher->parent = parent;
			}
			else if (!higher)
			{
				// Re-link the lower child tree to its parent
				parent->lower = lower;
				if (lower) lower->parent = parent;
			}
			else
			{
				// Re-link the higher child tree to its parent
				parent->lower = higher;
				higher->parent = parent;

				// Move the lower child tree to the far left in the higher child tree
				CacheNode *leftmost = higher;
				while (leftmost->lower) leftmost = leftmost->lower;
				leftmost->lower = lower;
				lower->parent = leftmost;
			}
		}
		else
		{
			if (!lower)
			{
				// Re-link the higher child tree to its parent
				parent->higher = higher;
				if (higher) higher->parent = parent;
			}
			else if (!higher)
			{
				// Re-link the lower child tree to its parent
				parent->higher = lower;
				if (lower) lower->parent = parent;
			}
			else
			{
				// Re-link the lower child tree to its parent
				parent->higher = lower;
				lower->parent = parent;

				// Move the lower child tree to the far left in the higher child tree
				CacheNode *rightmost = lower;
				while (rightmost->higher) rightmost = rightmost->higher;
				rightmost->higher = higher;
				higher->parent = rightmost;
			}
		}
	}
	else // At root (no parent):
	{
		u32 old_key = GetHash64(node->offset) & (_hash_table_size - 1);

		if (!lower)
		{
			// Re-link the higher child tree to its parent
			_cache_hash_table[old_key] = higher;
			if (higher) higher->parent = 0;
		}
		else if (!higher)
		{
			// Re-link the lower child tree to its parent
			_cache_hash_table[old_key] = lower;
			if (lower) lower->parent = 0;
		}
		else
		{
			// Re-link the higher child tree to its parent
			_cache_hash_table[old_key] = higher;
			higher->parent = 0;

			// Move the lower child tree to the far left in the higher child tree
			CacheNode *leftmost = higher;
			while (leftmost->lower) leftmost = leftmost->lower;
			leftmost->lower = lower;
			lower->parent = leftmost;
		}
	}
}

void Table::InsertNode(u64 offset, u32 key, CacheNode *hint, CacheNode *node)
{
	node->lower = 0;
	node->higher = 0;
	node->offset = offset;

	// If insertion point got to root,
	if (!hint)
	{
		// Look up root
		hint = _cache_hash_table[key];

		// If root is not set,
		if (!hint)
		{
			_cache_hash_table[key] = node;

			node->parent = 0;

			return;
		}
	}

	// Look starting from a node, knowing it won't already be inserted
	for (;;)
	{
		if (offset > hint->offset)
		{
			// Search right for empty leaf
			CacheNode *higher = hint->higher;
			if (!higher)
			{
				hint->higher = node;
				break;
			}
			hint = higher;
		}
		else
		{
			// Search left for empty leaf
			CacheNode *lower = hint->lower;
			if (!lower)
			{
				hint->lower = node;
				break;
			}
			hint = lower;
		}
	}

	node->parent = hint;
}

u8 *Table::SetOffset(u64 offset)
{
	// Get root node for this offset
	u32 key = GetHash64(offset) & (_hash_table_size - 1);
	CacheNode *insert = _cache_hash_table[key];

	// Search for node or approximation of insertion point
	while (insert)
	{
		// If node was found in cache (yay!),
		if (insert->offset == offset)
			return GetTrailingBytes(insert);
		else if (offset > insert->offset)
		{
			CacheNode *higher = insert->higher;
			if (!higher) break;
			insert = higher;
		}
		else
		{
			CacheNode *lower = insert->lower;
			if (!lower) break;
			insert = lower;
		}
	}

	// We will need to insert a new node:

	// Grab memory for the node
	u32 next = _next_cache_slot;
	CacheNode *node = reinterpret_cast<CacheNode*>( &_cache[next] );

	// If node is already used, unlink it!
	if (node->offset != INVALID_RECORD_INDEX && _cache_full)
	{
		// If the parent happens to be the one we're replacing, use parent instead
		if (insert == node) insert = insert->parent;

		UnlinkNode(node);
	}

	// Mark next cache slot
	next += sizeof(CacheNode) + _record_bytes;
	if (next >= _cache_bytes) { next = 0; _cache_full = true; }
	_next_cache_slot = next;

	InsertNode(offset, key, insert, node);

	return GetTrailingBytes(node);
}

u8 *Table::InsertOffset(u64 offset)
{
	// We will need to insert a new node:

	// Grab memory for the node
	u32 next = _next_cache_slot;
	CacheNode *node = reinterpret_cast<CacheNode*>( &_cache[next] );

	// If node is already used, unlink it!
	if (_cache_full && node->offset != INVALID_RECORD_INDEX)
	{
		UnlinkNode(node);
	}

	// Mark next cache slot
	next += sizeof(CacheNode) + _record_bytes;
	if (next >= _cache_bytes) { next = 0; _cache_full = true; }
	_next_cache_slot = next;

	// Get root node for this offset
	u32 key = GetHash64(offset) & (_hash_table_size - 1);
	CacheNode *insert = _cache_hash_table[key];

	InsertNode(offset, key, insert, node);

	return GetTrailingBytes(node);
}

u8 *Table::PeekOffset(u64 offset)
{
	CacheNode *node = FindNode(offset);
	if (!node) return 0;

	return GetTrailingBytes(node);
}

bool Table::RemoveOffset(u64 offset)
{
	CacheNode *node = FindNode(offset);
	if (!node) return false;

	UnlinkNode(node);

	// Mark cache node as invalid
	node->offset = INVALID_RECORD_INDEX;

	return true;
}

TableIndex *Table::MakeIndex(const char *index_file_path, IHash *hash_function, bool unique)
{
	if (!hash_function) return 0;

	TableIndex *index = new TableIndex(this, index_file_path, hash_function, _shutdown_observer);
	if (!index) return 0;

	if (!index->Initialize())
	{
		index->ReleaseRef();
		return 0;
	}

	// Add to index list
	index->_next = _head_index;
	_head_index = index;

	// Add to unique list
	index->_next_unique = _head_index_unique;
	_head_index_unique = index;

	return index;
}

bool Table::Initialize()
{
	if (!AllocateCache())
	{
		WARN("Table") << "Out of memory: Unable to allocate table cache for " << _file_path;
		return false;
	}

	if (!Open(_file_path, ASYNCFILE_READ | ASYNCFILE_WRITE | ASYNCFILE_RANDOM))
	{
		WARN("Table") << "Unable to open database file " << _file_path;
		return false;
	}

	// NOTE: Does not support files larger than 4 GB
	_next_record = GetSize();

	OnIndexingDone();

	return true;
}

u32 Table::GetCacheBytes()
{
	return _cache_bytes;
}

u32 Table::GetRecordBytes()
{
	return _record_bytes;
}


//// Callback on read completion

void Table::OnRead(ThreadPoolLocalStorage *tls, ReadFileCallback callback, u64 offset, u8 *data, u32 bytes)
{
	// On failure,
	if (bytes != _record_bytes)
	{
		// Invoke callback
		callback(tls, offset, data, bytes);
	}
	else
	{
		// Update cache immediately
		AutoWriteLock lock(_lock);
		memcpy(SetOffset(offset), data, bytes);
		lock.Release();

		callback(tls, offset, data, bytes);
	}
}


//// Indexing

bool Table::StartIndexingRead()
{
	void *buffer = LargeAligned::Acquire(_index_read_size);
	if (!buffer)
	{
		WARN("Table") << "Out of memory: Unable to acquire read buffer for indexing database " << _file_path;
		return false;
	}

	if (!BeginBulkRead(_index_read_offset, _index_read_size, buffer))
	{
		WARN("Table") << "Read failure while indexing database " << _file_path;
		LargeAligned::Release(buffer);
		return false;
	}

	_index_read_offset += _index_read_size;

	return true;
}

bool Table::StartIndexing()
{
	_index_database_size = GetSize();

	// If database is empty, just return true (no work to do)
	if (_index_database_size <= 0)
	{
		INFO("Table") << "Database table " << _file_path << " is empty so aborting index job";

		OnIndexingDone();
		return true;
	}

	INFO("Table") << "Starting indexing of database table " << _file_path;

	_index_read_completed = 0;
	_index_read_offset = 0;

	for (int ii = 0; ii < NUM_PARALLEL_INDEX_READS; ++ii)
	{
		if (!StartIndexingRead())
			return false;
	}

	return true;
}

void Table::OnIndexingDone()
{
	// Unlink loaded indices
	for (TableIndex *index = _head_index_update, *next; index; index = next)
	{
		INFO("Table") << "Table indexing has completed for " << index->GetFilePath();
		next = index->_next_loading;
		index->_next_loading = 0;
	}

	// Shift waiting into update slot
	_head_index_update = _head_index_waiting;
	_head_index_waiting = 0;

	// If the indexing should be restarted,
	if (_head_index_update)
		StartIndexing();
}

bool Table::RequestIndexRebuild(TableIndex *index)
{
	AutoWriteLock lock(_lock);

	// If already requested,
	if (index->_next_loading ||
		_head_index_waiting == index ||
		_head_index_update == index)
	{
		return true;
	}

	// If not initialized or update in progress,
	if (!_cache || _head_index_update)
	{
		// Insert in waiting list
		index->_next_loading = _head_index_waiting;
		_head_index_waiting = index;

		return true;
	}

	// Insert in update list
	index->_next_loading = _head_index_update;
	_head_index_update = index;

	lock.Release();

	return StartIndexing();
}

void Table::OnReadBulk(ThreadPoolLocalStorage *tls, u64 offset, u8 *data, u32 bytes)
{
	if (!bytes)
	{
		// Probably read beyond end of data (this is normal)
		LargeAligned::Release(data);
	}
	else
	{
		u32 step = _record_bytes;
		u32 records = bytes / step;

		// For each record from the file,
		while (records--)
		{
			// For each table index,
			for (TableIndex *index = _head_index_update; index; index = index->_next_loading)
			{
				// Insert record into index
				index->InsertComplete(data, offset);
			}

			data += step;
			offset += step;
		}

		AutoWriteLock lock(_lock);

			u64 completed = _index_read_completed + bytes;
			_index_read_completed = completed;

			u64 next_read_offset = _index_read_offset + _index_read_size;
			_index_read_offset = next_read_offset;

		lock.Release();

		// If reading has completed,
		if (completed >= _index_database_size)
		{
			INANE("Table") << "Done(1): Releasing indexing buffer for " << _file_path;
			LargeAligned::Release(data);

			OnIndexingDone();
		}
		else
		{
			if (next_read_offset + _index_read_size >= _index_database_size)
			{
				if (next_read_offset >= _index_database_size)
				{
					INANE("Table") << "Done(2): Releasing indexing buffer for " << _file_path;
					LargeAligned::Release(data);
				}
				else
				{
					INANE("Table") << "Reading final page indexing " << _file_path;
					BeginBulkRead(next_read_offset, (u32)(_index_database_size - next_read_offset), data);
				}
			}
			else
			{
				INANE("Table") << "Reading another page indexing " << _file_path;
				BeginBulkRead(next_read_offset, _index_read_size, data);
			}
		}
	}
}

u64 Table::UniqueIndexLookup(const void *data)
{
	// For each table index that uniquely identifies records,
	for (TableIndex *index = _head_index_unique; index; index = index->_next_unique)
	{
		u64 offset = index->LookupComplete(data);

		// If unique identifier is already in the index,
		if (INVALID_RECORD_INDEX != offset)
		{
			// It exists already
			return offset;
		}
	}

	// Was not found
	return INVALID_RECORD_INDEX;
}


//// User Interface

u8 *Table::GetBuffer()
{
	return GetPostBuffer(_record_bytes);
}

u64 Table::Insert(void *data)
{
	// Update cache immediately
	AutoWriteLock lock(_lock);

		// If it already exists,
		if (INVALID_RECORD_INDEX != UniqueIndexLookup(data))
			return INVALID_RECORD_INDEX;

		// Get next offset
		u64 offset = _next_record;
		_next_record = offset + _record_bytes;

		INANE("Table") << "Insert " << offset << " in " << _file_path;

		u8 *cache = InsertOffset(offset);
		memcpy(cache, data, _record_bytes);

		// Insert into indexes
		for (TableIndex *index = _head_index; index; index = index->_next)
			index->InsertComplete(data, offset);

	lock.Release();

	// Queue a disk write
	if (!BeginWrite(offset, data, _record_bytes))
	{
		WARN("Table") << "Disk write failure on insertion for " << _file_path;
		return INVALID_RECORD_INDEX;
	}

	return offset;
}

bool Table::Replace(u64 offset, void *data)
{
	INANE("Table") << "Replace " << offset << " in " << _file_path;

	// Update cache immediately
	AutoWriteLock lock(_lock);

		memcpy(SetOffset(offset), data, _record_bytes);

	lock.Release();

	// Queue a disk write
	return BeginWrite(offset, data, _record_bytes);
}

bool Table::Query(ThreadPoolLocalStorage *tls, u64 offset, ReadFileCallback callback)
{
	INANE("Table") << "Query " << offset << " in " << _file_path;

	// Check cache first
	AutoReadLock lock(_lock);

		u8 *cache = PeekOffset(offset);

		if (cache)
		{
			u8 *copy = (u8*)alloca(_record_bytes);
			memcpy(copy, cache, _record_bytes);
			lock.Release();

			callback(tls, offset, copy, _record_bytes);
			return true;
		}

	lock.Release();

	// Queue a disk read if not in cache
	return BeginRead(offset, _record_bytes, callback);
}

bool Table::Remove(void *data)
{
	// Update cache immediately
	AutoWriteLock lock(_lock);

		u64 offset = UniqueIndexLookup(data);

		INANE("Table") << "Remove " << offset << " from " << _file_path;

		// If it does not exist,
		if (INVALID_RECORD_INDEX == offset)
			return false;

		RemoveOffset(offset);

		// Remove from indexes
		for (TableIndex *index = _head_index; index; index = index->_next)
			index->RemoveComplete(data);

	lock.Release();

	// Zero entry on disk
	u8 *buffer = GetPostBuffer(_record_bytes);
	memset(buffer, 0, _record_bytes);

	// Queue a disk write
	return BeginWrite(offset, buffer, _record_bytes);
}
