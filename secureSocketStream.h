#pragma once

// This is NOT a standard TLS implementation, it's a custom format that has some similarities, but
// does not respect any standardized format. Connections must use this class on both ends.
// A https server, for example, won't be able to talk to this.
// Uses a Diffie-Hellman key exchange handshake to obtain a symmetric cipher key.

#include <string.h>
#include <memory.h>
#include <sys/types.h>
#ifdef _WIN32
#include <Ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define closesocket(s) ::close(s)
#define SOCKADDR_IN sockaddr_in
#define SOCKADDR sockaddr
#endif

#include <sstream>
#include <openssl/ssl.h>
#include <openssl/err.h>
//#include <openssl/bio.h>


#define SecureSocketStream_DHPrimeModBits 1024
#define SecureSocketStream_HandshakeVersion 2
#define SecureSocketStream_SocketBufferSize 0X1000



union EndianTest_t
{
	short s;
	struct
	{
		char ofs0;
		char ofs1;
	};
};

constexpr bool BigEndianSystem()
{
	EndianTest_t u{1};
	return (u.ofs1 != 0);
}

constexpr bool LittleEndianSystem()
{
	EndianTest_t u{1};
	return (u.ofs0 != 0);
}

class nshort
{
public:
	nshort& operator=(short n) { val = htons(n); return *this; }
	operator short() { return ntohs(val); }
private:
	short val;
};

class unshort
{
public:
	unshort& operator=(unsigned short n) { val = htons(n); return *this; }
	operator unsigned short() { return ntohs(val); }
private:
	unsigned short val;
};

class nlong
{
public:
	nlong& operator=(long n) { val = htonl(n); return *this; }
	operator long() { return ntohl(val); }
private:
	long val;
};

class unlong
{
public:
	unlong& operator=(unsigned long n) { val = htonl(n); return *this; }
	operator unsigned long() { return ntohl(val); }
private:
	unsigned long val;
};


class SecureSocketStream
{
public:
	struct ListenFlagType {};
	static ListenFlagType LISTEN;

	struct HandshakeHeader
	{
		unlong sizeOfThisStruct;
		unlong version;
		unlong publicModSizeOctets;
		unlong publicBaseSizeOctets;
		unlong preKeySizeOctets;
		unsigned char chachaIV[8];
		// The BIGNUM buffers should be longer than necessary
		// For 1024-bit numbers only 128 bytes is necessary, but to allow for future expansion and such,
		// 0x400 = 1024 bytes will allow 8192-bit numbers.
		// The number is stored at the beginning of the buffer and runs for the number of bytes/octets
		// indicated in the *SizeOctets members.
		unsigned char publicMod[0x400];
		unsigned char publicBase[0x400];
		unsigned char preKey[0x400];
	};

	SecureSocketStream(bool verbose = false);
	SecureSocketStream(SOCKET s, bool iAmServer, bool verbose = false);
	//SecureSocketStream(const char* host);
	//SecureSocketStream(const char* host, int port);
	//SecureSocketStream(const std::string& host);
	//SecureSocketStream(const std::string& host, int port);
	//SecureSocketStream(ListenFlagType L, int port);
	~SecureSocketStream();

	bool begin(SOCKET s, bool iAmServer);
	bool valid();

	template<typename T> SecureSocketStream& operator << (const T& thingToSend);
	template<typename T> SecureSocketStream& operator >> (T& thingToRecv);

	int readAnySize(void* buffer, int maxSize);
	int readFixedSize(void* buffer, int size);
	int read(void* buffer, int size) { return readFixedSize(buffer, size); }
	int write(const void* buffer, int size);

	bool getline(std::string& outstr);
	// TODO: getline()
	void close();
	bool eos();
	bool eof() { return eos(); }

	bool verbose;

private:

	void init();
	bool handshakeServer();
	bool handshakeClient();
	bool fail();
	bool initCipher(BIGNUM* key, unsigned char* iv);
	int sendLoop(const void* buffer, int size);
	int sendLoop(const char* c_str); // Does not send null terminator
	int recvFixedBuffer(void* dest, int len);
	int recvDecrypt(void* dest, int maxSize);
	void ssCheckClear();

	// Members vars
	bool _valid;
	bool _eos;
	SOCKET sock;
	std::stringstream ssRecv;
	unsigned char* recvBuffer;
	unsigned char* sendBuffer;

	// Diffie-Hellman params
	BN_CTX* bignumContext;
	BIGNUM* publicBase;
	BIGNUM* publicMod;
	BIGNUM* mySecret;
	BIGNUM* myPreKey;
	BIGNUM* remoteExchangeKey;
	BIGNUM* skey;

	// Chacha20 stream cipher objects
	EVP_CIPHER_CTX* sendCipher;
	EVP_CIPHER_CTX* recvCipher;
};

template<typename T> SecureSocketStream& SecureSocketStream::operator << (const T& thingToSend)
{
	std::stringstream lss;
	lss << thingToSend;
	std::string s(lss.str());
	write(s.c_str(), (int)s.length());
	//lss.str("");
	//lss.clear();
	return *this;
}

template<typename T> SecureSocketStream& SecureSocketStream::operator >> (T& thingToRecv)
{
	// Need to receive enough to encounter whitespace after item

	decltype(ssRecv.tellp() - ssRecv.tellg()) ssLen;
	std::string s3;
	char rbuf[SecureSocketStream_SocketBufferSize];
	int nread;
	int w1, w2, nw;
	const char* whitespace = " \t\r\n\0";
	while (true)
	{
		ssLen = ssRecv.tellp() - ssRecv.tellg();
		if (ssLen > 0)
		{
			//s3 = ssRecv.str();
			s3 = ssRecv.str().substr((int)ssRecv.tellg());
			w1 = (int)s3.find_first_of(whitespace);
			if (w1 == std::string::npos) { goto readMore; } // yeah, I know
			nw = (int)s3.find_first_not_of(whitespace);
			if (nw == std::string::npos) { goto readMore; }
			if (w1 < nw)
			{
				w2 = (int)s3.find_first_of(whitespace, nw);
				if (w2 == std::string::npos) { goto readMore; }
			}
			ssRecv >> thingToRecv;
			return *this;
		}
	readMore:
		//nread = readAnySize(rbuf, SecureSocketStream_SocketBufferSize);
		nread = recvDecrypt(rbuf, SecureSocketStream_SocketBufferSize);
		if (nread <= 0)
		{
			//fail();
			_eos = true;
			ssRecv >> thingToRecv;
			return *this;
		}
		ssRecv.write(rbuf, nread);
	}
	return *this;
}

