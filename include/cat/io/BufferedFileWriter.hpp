/*
	Copyright (c) 2011 Christopher A. Taylor.  All rights reserved.

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

#ifndef CAT_BUFFERED_FILE_WRITER_HPP
#define CAT_BUFFERED_FILE_WRITER_HPP

#include <cat/io/Buffers.hpp>

/*
	BufferedFileWriter

	Batches data into large buffers sized to be a multiple of the sector size.
*/

namespace cat {


// See above for rationale of these choices
static const u32 OPTIMAL_FILE_WRITE_CHUNK_SIZE = 32768;
static const u32 OPTIMAL_FILE_WRITE_MODE = ASYNCFILE_WRITE | ASYNCFILE_SEQUENTIAL | ASYNCFILE_NOBUFFER;

class BufferedFileWriter : public AsyncFile
{
	u8 *_cache;
	u32 _cache_size, _cache_used;
	u64 _offset;
	u64 _file_size;

public:
	BufferedFileWriter();
	virtual ~BufferedFileWriter();

	CAT_INLINE u64 Offset() { return _offset; }
	CAT_INLINE u64 Size() { return _file_size; }
	CAT_INLINE u64 Remaining()
	{
		s64 remaining = (s64)(_file_size - _offset);
		return remaining > 0 ? remaining : 0;
	}

	bool Open(const char *file_path, u32 worker_id);

	bool Write(const u8 *buffer, u32 bytes);
};


} // namespace cat

#endif // CAT_BUFFERED_FILE_WRITER_HPP
