#include <cat/AllFramework.hpp>
#include <conio.h> // kbhit()
using namespace cat;
using namespace sphynx;

#include <fstream>
using namespace std;


class GameClient : public Client
{
public:
	virtual void OnClose()
	{
		WARN("Client") << "-- CLOSED";
	}

	virtual void OnConnectFail(HandshakeError err)
	{
		WARN("Client") << "-- CONNECT FAIL ERROR " << GetHandshakeErrorString(err);
	}

	virtual void OnConnect(ThreadPoolLocalStorage *tls)
	{
		WARN("Client") << "-- CONNECTED";
	}

	virtual void OnMessage(ThreadPoolLocalStorage *tls, BufferStream msg, u32 bytes)
	{
		switch (msg[0])
		{
		case 0:
			{
				WARN("Client") << "Got request for transmit";

				static char STR[4000];

				for (int ii = 0; ii < sizeof(STR); ++ii)
					STR[ii] = (char)ii;

				for (int jj = 0; jj < 1000; ++jj)
					WriteReliable(STREAM_1, 1, STR, sizeof(STR));

				WriteReliable(STREAM_3, 0, STR, sizeof(STR));
			}
			break;
		default:
			WARN("Client") << "Got message with " << bytes << " bytes";
		}
	}

	virtual void OnDisconnect(u8 reason)
	{
		WARN("Client") << "-- DISCONNECTED REASON " << (int)reason;
	}
	virtual void OnTick(ThreadPoolLocalStorage *tls, u32 now)
	{
		//WARN("Client") << "-- TICK " << now;
	}
};

int main()
{
	if (!InitializeFramework("ChatClient.txt"))
	{
		FatalStop("Unable to initialize framework!");
	}

	INFO("Client") << "Secure Chat Client 1.1";

	{
		ThreadPoolLocalStorage tls;

		ifstream keyfile("PublicKeyFile.txt");

		if (!keyfile)
		{
			FATAL("Client") << "Unable to open public key file";
		}

		u8 server_public_key[PUBLIC_KEY_BYTES];

		string key_str;
		keyfile >> key_str;

		if (ReadBase64(key_str.c_str(), (int)key_str.length(), server_public_key, sizeof(server_public_key)) != sizeof(server_public_key))
		{
			FATAL("Client") << "Public key from file is wrong length";
		}

		for (int ii = 0; ii < 1; ++ii)
		{
			GameClient *client = new GameClient;

			const char *SessionKey = "Chat";

			if (!client->SetServerKey(&tls, server_public_key, sizeof(server_public_key), SessionKey))
			{
				FATAL("Client") << "Provided server key invalid";
			}

			// linux: 10.1.1.146
			// netbook: 10.1.1.110
			// coldfront: 68.84.166.22
			if (!client->Connect("68.84.166.22", 22000))
			{
				FATAL("Client") << "Unable to connect to server";
			}
		}

		while (!kbhit())
		{
			Clock::sleep(100);
		}
	}

	ShutdownFramework(true);

	return 0;
}
