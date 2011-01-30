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

#ifndef CAT_SPHYNX_SERVER_HPP
#define CAT_SPHYNX_SERVER_HPP

#include <cat/sphynx/Transport.hpp>
#include <cat/threads/RWLock.hpp>
#include <cat/threads/Thread.hpp>
#include <cat/threads/WaitableFlag.hpp>
#include <cat/sphynx/Collexion.hpp>
#include <cat/crypt/cookie/CookieJar.hpp>
#include <cat/crypt/tunnel/KeyAgreementResponder.hpp>
#include <cat/threads/RefObject.hpp>

namespace cat {


namespace sphynx {


// Server port
class Server : public UDPEndpoint
{
	virtual void OnShutdownRequest();
	virtual void OnZeroReferences();

public:
	Server();
	virtual ~Server();

	bool StartServer(ThreadPoolLocalStorage *tls, Port port, u8 *public_key, int public_bytes, u8 *private_key, int private_bytes, const char *session_key);

	u32 GetTotalPopulation();

	static bool GenerateKeyPair(ThreadPoolLocalStorage *tls, const char *public_key_file,
								const char *private_key_file, u8 *public_key,
								int public_bytes, u8 *private_key, int private_bytes);

private:
	static const int SESSION_KEY_BYTES = 32;
	char _session_key[SESSION_KEY_BYTES];

	Port _server_port;

	ConnexionMap _conn_map;

	CookieJar _cookie_jar;

	KeyAgreementResponder _key_agreement_responder;
	u8 _public_key[PUBLIC_KEY_BYTES];

private:
	ServerWorker *FindLeastPopulatedPort();

	void OnRead(ThreadPoolLocalStorage *tls, const NetAddr &src, u8 *data, u32 bytes);
	void OnClose();

	void PostConnectionCookie(const NetAddr &dest);
	void PostConnectionError(const NetAddr &dest, HandshakeError err);

protected:
	// Must return a new instance of your Connexion derivation
	virtual Connexion *NewConnexion() = 0;

	// IP address filter: Return true to allow the connection to be made
	virtual bool AcceptNewConnexion(const NetAddr &src) = 0;

	// Lookup client by key
	Connexion *Lookup(u32 key);
};


} // namespace sphynx


} // namespace cat

#endif // CAT_SPHYNX_SERVER_HPP
