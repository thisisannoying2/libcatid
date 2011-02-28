/*
	Copyright (c) 2009-2011 Christopher A. Taylor.  All rights reserved.

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

#ifndef CAT_SPHYNX_CONNEXION_MAP_HPP
#define CAT_SPHYNX_CONNEXION_MAP_HPP

#include <cat/net/Sockets.hpp>
#include <cat/sphynx/Connexion.hpp>
#include <cat/threads/RWLock.hpp>

// TODO: Implement a generic growing Dictionary<> class

namespace cat {


namespace sphynx {


// Maps remote address to connected clients
class ConnexionMap
{
public:
	static const u32 INVALID_KEY = ~(u32)0;
	static const int HASH_TABLE_SIZE = 32768; // Power-of-2
	static const int HASH_TABLE_MASK = HASH_TABLE_SIZE - 1;
	static const int MAX_POPULATION = HASH_TABLE_SIZE / 2;
	static const int CONNECTION_FLOOD_THRESHOLD = 10;

	// (multiplier-1) divisible by all prime factors of table size
	// (multiplier-1) is a multiple of 4 if table size is a multiple of 4
	// These constants are from Press, Teukolsky, Vetterling and Flannery's
	// "Numerical Recipes in FORTRAN: The Art of Scientific Computing"
	static const u32 COLLISION_MULTIPLIER = 71*5861 * 4 + 1;
	static const u32 COLLISION_INCREMENTER = 1013904223;

	// If multiplier changes, this needs to be recalculated (multiplicative inverse of above)
	static const u32 COLLISION_MULTINVERSE = 4276115653;
	static const u32 COLLISION_INCRINVERSE = 0 - COLLISION_INCREMENTER;

	struct Slot
	{
		Connexion *conn;
		bool collision;
	};

protected:
	u32 _flood_salt, _ip_salt, _port_salt;
	Slot _map_table[HASH_TABLE_SIZE];
	u8 _flood_table[HASH_TABLE_SIZE];
	RWLock _table_lock;

public:
	ConnexionMap();
	virtual ~ConnexionMap();

	// Initialize the hash salt
	void Initialize(FortunaOutput *csprng);

	// Lookup client by address
	// Returns true if flood guard triggered
	bool LookupCheckFlood(Connexion * &connexion, const NetAddr &addr);

	// Lookup client by key
	Connexion *Lookup(u32 key);

	// May return false if network address in Connexion is already in the map.
	// This averts a potential race condition but should never happen during
	// normal operation, so the Connexion allocation by caller won't be wasted
	bool Insert(Connexion *conn);

	// Remove Connexion object from the lookup table
	void Remove(Connexion *conn);

	// Invoke ->RequestShutdown() on all Connexion objects
	void ShutdownAll();
};


} // namespace sphynx


} // namespace cat

#endif // CAT_SPHYNX_CONNEXION_MAP_HPP
