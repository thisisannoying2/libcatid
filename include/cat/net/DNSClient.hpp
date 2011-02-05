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

// TODO: React to timeouts from DNS server by switching to a backup
// TODO: Use TTL from DNS record instead of fixed constant
// TODO: If the DNS resolution load is high, it would make sense to put
//       multiple requests in the same DNS packet
// TODO: Retransmissions could also be grouped into the same DNS packets
// TODO: The locks held in DNSClient are fairly coarse and could be broken up

#ifndef CAT_DNS_CLIENT_HPP
#define CAT_DNS_CLIENT_HPP

#include <cat/io/IOLayer.hpp>
#include <cat/threads/WaitableFlag.hpp>
#include <cat/port/FastDelegate.h>
#include <cat/crypt/rand/Fortuna.hpp>

namespace cat {


static const int HOSTNAME_MAXLEN = 63; // Max characters in a hostname request
static const int DNSREQ_TIMEOUT = 3000; // DNS request timeout interval
static const int DNSREQ_REPOST_TIME = 300; // Number of milliseconds between retries
static const int DNSREQ_MAX_SIMUL = 2048; // Maximum number of simultaneous DNS requests
static const int DNSCACHE_MAX_REQS = 8; // Maximum number of requests to cache
static const int DNSCACHE_MAX_RESP = 8; // Maximum number of responses to cache
static const int DNSCACHE_TIMEOUT = 60000; // Time until a cached response is dropped

static const int DNS_THREAD_KILL_TIMEOUT = 10000; // 10 seconds

// Prototype: bool MyResultCallback(const char *, const NetAddr*, int);
typedef fastdelegate::FastDelegate3<const char *, const NetAddr*, int, bool> DNSResultCallback;


//// DNSRequest

struct DNSCallback
{
	DNSCallback *next;

	DNSResultCallback cb;
	RefObject *ref;
};

struct DNSRequest
{
	DNSRequest *last, *next;

	u32 first_post_time; // Timestamp for first post, for timeout
	u32 last_post_time; // Timestamp for last post, for retries and cache

	// Our copy of the hostname string
	char hostname[HOSTNAME_MAXLEN+1];
	u16 id; // Random request ID
	DNSCallback callback_head;

	// For caching purposes
	NetAddr responses[DNSCACHE_MAX_RESP];
	int num_responses;
};


//// DNSClient

class DNSClient : public UDPEndpoint, public WorkerCallbacks
{
	NetAddr _server_addr;
	bool _initialized;

	FortunaOutput *_csprng;

	u32 _worker_id;

	Mutex _request_lock;
	DNSRequest *_request_head;
	DNSRequest *_request_tail;
	int _request_queue_size;

	Mutex _cache_lock;
	DNSRequest *_cache_head;
	DNSRequest *_cache_tail;
	int _cache_size;

	bool Initialize(IOLayer *iolayer);

	bool GetUnusedID(u16 &id); // not thread-safe, caller must lock
	bool IsValidHostname(const char *hostname);
	DNSRequest *PullRequest(u16 id); // not thread-safe, caller must lock

	// These functions do not lock, caller must lock:
	void CacheAdd(DNSRequest *req); // Assumes not already in cache
	DNSRequest *CacheGet(const char *hostname); // Case-insensitive
	void CacheKill(DNSRequest *req); // Assumes already in cache

	bool GetServerAddr();
	bool BindToRandomPort(IOLayer *iolayer, bool ignoreUnreachable);
	bool PostDNSPacket(DNSRequest *req, u32 now);
	bool PerformLookup(DNSRequest *req); // not thread-safe, caller must lock

	void ProcessDNSResponse(DNSRequest *req, int qdcount, int ancount, u8 *data, u32 bytes);
	void NotifyRequesters(DNSRequest *req);

public:
	DNSClient();
	CAT_INLINE virtual ~DNSClient() {}

	/*
		If hostname is numeric or in the cache, the callback function will be invoked
		immediately from the requesting thread, rather than from another thread.

		First attempts numerical resolve of hostname, then queries the DNS server.

		Hostname string length limited to HOSTNAME_MAXLEN characters.
		Caches the most recent DNSCACHE_MAX_REQS requests.
		Returns up to DNSCACHE_MAX_RESP addresses per resolve request.
		Performs DNS lookup on a cached request after DNSCACHE_TIMEOUT.
		Gives up on DNS lookup after DNSREQ_TIMEOUT.

		If holdRef is valid, the reference will be held until the callback completes.

		If no results were found, array_length == 0.
		If the callback returns false, the result will not be entered into the cache.

		The resolved addresses may need to be promoted to IPv6.

		If Resolve() returns false, no callback will be generated.
	*/
	bool Resolve(IOLayer *iolayer, const char *hostname, DNSResultCallback, RefObject *holdRef = 0);

protected:
	virtual void OnShutdownRequest();
	virtual bool OnZeroReferences();

	virtual void OnReadRouting(const BatchSet &buffers);
	virtual void OnUnreachable(const NetAddr &src);

	virtual void OnWorkerRead(IWorkerTLS *tls, const BatchSet &buffers);
	virtual void OnWorkerTick(IWorkerTLS *tls, u32 now);
};


} // namespace cat

#endif // CAT_DNS_CLIENT_HPP
