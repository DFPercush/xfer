
#include "secureSocketStream.h"


void SecureSocketStream::init()
{
	bignumContext = BN_CTX_new();
	publicBase = BN_new();
	publicMod = BN_new();
	mySecret = BN_new();
	myPreKey = BN_new();
	remoteExchangeKey = BN_new();
	skey = BN_new();
	_valid = false;
	_eos = false;
	sendCipher = nullptr;
	recvCipher = nullptr;
	sendBuffer = nullptr;
	recvBuffer = nullptr;
}

bool SecureSocketStream::eos()
{
	return _eos;
}

bool SecureSocketStream::begin(SOCKET s, bool iAmServer)
{
	init();
	sock = s;
	_valid = (iAmServer ? handshakeServer() : handshakeClient());
	if (!_valid) { return false; }
	recvBuffer = new unsigned char[SecureSocketStream_SocketBufferSize];
	if (!recvBuffer) { _valid = false; return false; }
	sendBuffer = new unsigned char[SecureSocketStream_SocketBufferSize];
	if (!sendBuffer) { _valid = false; return false; }
	return _valid;
}

SecureSocketStream::SecureSocketStream()
{
	init();
}

SecureSocketStream::SecureSocketStream(SOCKET s, bool iAmServer)
{
	init();
	begin(s, iAmServer);
}

//SecureSocketStream::SecureSocketStream(const char* host)
//{
//
//}
//
//SecureSocketStream::SecureSocketStream(const char* host, int port)
//{
//
//}
//
//SecureSocketStream::SecureSocketStream(const std::string& host)
//{
//	SecureSocketStream(host.c_str());
//}
//
//SecureSocketStream::SecureSocketStream(const std::string& host, int port)
//{
//	SecureSocketStream(host.c_str(), port);
//}
//
//SecureSocketStream::SecureSocketStream(ListenFlagType L, int port)
//{
//
//}


SecureSocketStream::~SecureSocketStream()
{
	BN_clear_free(publicBase);
	BN_clear_free(publicMod);
	BN_clear_free(mySecret);
	BN_clear_free(myPreKey);
	BN_clear_free(remoteExchangeKey);
	BN_clear_free(skey);
	BN_CTX_free(bignumContext);
	if (recvBuffer) { delete [] recvBuffer; }
	if (sendBuffer) { delete [] sendBuffer; }
	close();
}

void SecureSocketStream::close()
{
	if (sock != INVALID_SOCKET)
	{
#ifdef WIN32
		closesocket(sock);
#else
		close(sock);
#endif
	}
	sock = INVALID_SOCKET;
}

bool SecureSocketStream::recvFixedBuffer(SOCKET sock, void* dest, int len)
{
	int nread = 0;
	int nreadcum = 0;
	//char rbuf[0x1000];
	while (nreadcum < len)
	{
		nread = recv(sock, (char*)dest + nreadcum, len - nreadcum, 0);
		if (nread <= 0)
		{
			// Connection terminated.
			_eos = true;
			if (nread < 0)
			{
				_valid = false;
			}
			return false;
		}
		nreadcum += nread;
	}
	return true;
}

bool SecureSocketStream::initCipher(BIGNUM* key, unsigned char* iv)
{
	sendCipher = EVP_CIPHER_CTX_new();
	recvCipher = EVP_CIPHER_CTX_new();

	unsigned char chachaSeedKey[SHA256_DIGEST_LENGTH];
	int skeyBinSize = BN_num_bytes(skey);
	unsigned char* skeyBin = new unsigned char[skeyBinSize];
	BN_bn2bin(skey, skeyBin);
	SHA256(skeyBin, skeyBinSize, chachaSeedKey);
	delete[] skeyBin;
	skeyBinSize = 0;

	bool ret = true;
	if (!EVP_EncryptInit_ex(sendCipher, EVP_chacha20(), nullptr, chachaSeedKey, iv)) { ret = false; }
	if (!EVP_DecryptInit_ex(recvCipher, EVP_chacha20(), nullptr, chachaSeedKey, iv)) { ret = false; }
	return ret;
}

bool SecureSocketStream::handshakeServer()
{
	HandshakeHeader hh;
	hh.version = SecureSocketStream_HandshakeVersion;
	hh.sizeOfThisStruct = sizeof(hh);

	BN_generate_prime_ex2(publicMod, SecureSocketStream_DHPrimeModBits, 1, nullptr, nullptr, nullptr, bignumContext);
	BN_rand(publicBase, 64, 1, 1);
	BN_rand(mySecret, SecureSocketStream_DHPrimeModBits - 2, 1, 1);
	auto chachaIV = BN_new();
	BN_mod_exp(myPreKey, publicBase, mySecret, publicMod, bignumContext);

	unsigned long myExchangeKeySize = BN_num_bytes(myPreKey);
	unsigned char* myExchangeKeyBuffer = new unsigned char[myExchangeKeySize];
	BN_bn2bin(myPreKey, myExchangeKeyBuffer);

	hh.publicModSizeOctets = BN_num_bytes(publicMod);
	hh.publicBaseSizeOctets = BN_num_bytes(publicBase);
	hh.preKeySizeOctets = BN_num_bytes(myPreKey);
	if (hh.publicBaseSizeOctets > sizeof(hh.publicBase)
		|| hh.publicModSizeOctets > sizeof(hh.publicMod)
		|| hh.preKeySizeOctets > sizeof(hh.preKey))
	{
		return fail();
	}

	BN_rand(chachaIV, 64, 0, 0);
	BN_bn2bin(chachaIV, hh.chachaIV);
	BN_free(chachaIV);

	BN_bn2bin(publicMod, hh.publicMod);
	BN_bn2bin(publicBase, hh.publicBase);
	BN_bn2bin(myPreKey, hh.preKey);

	printf("Sending handshake header, %d bytes ... ", (int)sizeof(hh));
	//::send(sock, (char*)&hh, sizeof(hh), 0);
	sendLoop(&hh, sizeof(hh));
	printf("ok.\n");

	HandshakeHeader hhClient;
	printf("Waiting to receive client handshake header, %d bytes ... ", (int)sizeof(hhClient));
	if (!recvFixedBuffer(sock, &hhClient, sizeof(hhClient))) { return fail(); }
	printf("ok.\n");

	if (hhClient.publicBaseSizeOctets != hh.publicBaseSizeOctets) { return fail(); }
	if (hhClient.publicModSizeOctets != hh.publicModSizeOctets) { return fail(); }
	if (memcmp(hh.publicBase, hhClient.publicBase, hh.publicBaseSizeOctets)) { return fail(); }
	if (memcmp(hh.publicMod, hhClient.publicMod, hh.publicModSizeOctets)) { return fail(); }

	BN_bin2bn(hhClient.preKey, hh.preKeySizeOctets, remoteExchangeKey);
	BN_mod_exp(skey, remoteExchangeKey, mySecret, publicMod, bignumContext);
	// Secret key established.
	
	printf("Sending GOOD signal ... ");
	//::send(sock, "GOOD", 4, 0);
	sendLoop("GOOD");
	printf("ok.\n");

	//auto decStr = BN_bn2dec(skey);
	//printf("skey = %s\n", decStr);
	//OPENSSL_free(decStr);

	printf("Handshake complete.\n");

	initCipher(skey, hh.chachaIV);
	return true;
}

bool SecureSocketStream::valid()
{
	return _valid;
}

bool SecureSocketStream::handshakeClient()
{
	HandshakeHeader hhServer;
	int nreadcum = 0;
	int nread = 0;

	printf("Waiting to receive server handshake header, %d bytes ... ", (int)sizeof(hhServer));
	if (!recvFixedBuffer(sock, &hhServer, sizeof(hhServer)))
	{
		printf("Error: Did not receive all the expected data.\n");
		return fail();
	}
	printf("ok.\n");

	if (hhServer.sizeOfThisStruct != sizeof(hhServer) || hhServer.version != SecureSocketStream_HandshakeVersion)
	{	
		printf("Error: Version mismatch.\n");
		return fail();
	}

	BN_bin2bn(hhServer.publicBase, hhServer.publicBaseSizeOctets, publicBase);
	BN_bin2bn(hhServer.publicMod, hhServer.publicModSizeOctets, publicMod);
	BN_bin2bn(hhServer.preKey, hhServer.preKeySizeOctets, remoteExchangeKey);

	BN_rand(mySecret, SecureSocketStream_DHPrimeModBits - 2, 1, 1);
	BN_mod_exp(myPreKey, publicBase, mySecret, publicMod, bignumContext);

	HandshakeHeader hhme; // me, my, mine
	memcpy(&hhme, &hhServer, sizeof(HandshakeHeader));
	hhme.preKeySizeOctets = BN_num_bytes(myPreKey);
	if (hhme.preKeySizeOctets > sizeof(hhme.preKey))
	{
		printf("Error: public exchange key is too large to fit in network buffer.\n");
		return fail();
	}
	BN_bn2bin(myPreKey, hhme.preKey);
	BN_mod_exp(skey, remoteExchangeKey, mySecret, publicMod, bignumContext);
	// Secret key established (symkey), now send server our info	
	printf("Sending handshake header, %d bytes ... ", (int)sizeof(hhme));
	//::send(sock, (char*)&hhme, sizeof(hhme), 0);
	sendLoop(&hhme, sizeof(hhme));
	printf("ok.\n");

	char gbuf[5];
	memset(gbuf, 0, 5);
	printf("Waiting for GOOD signal ... ");
	recvFixedBuffer(sock, gbuf, 4);
	gbuf[4] = 0;
	if (strcmp(gbuf, "GOOD"))
	{
		printf("Error: server did not acknowledge good status.\n");
		return fail();
	}
	printf("ok.\n");

	//auto decStr = BN_bn2dec(skey);
	//printf("skey = %s\n", decStr);
	//OPENSSL_free(decStr);

	//EVP_aes_256_cbc();
	printf("Handshake complete.\n");

	initCipher(skey, hhServer.chachaIV);
	return true;
}

bool SecureSocketStream::fail()
{
	close();
	_valid = false;
	return false;
}


int SecureSocketStream::readAnySize(void* buffer, int maxSize)
{
	if (!_valid) { return -1; }

	//ss.seekg(0, std::ios::end);
	//auto ssLen = (int)ss.tellg();
	//ss.seekg(0, std::ios::beg);
	//std::string ssLeftover(ss.str());
	//int ssLeftoverLength = (int)ssLeftover.length();
	
	auto ssLen = (int)ss.tellp();
	if (ssLen > 0)
	{
		if (ssLen > maxSize)
		{
			ss.read((char*)buffer, maxSize);
			return maxSize;
		}
		else
		{
			ss.read((char*)buffer, ssLen);
			return ssLen;
		}
	}
	int sizeToRead = SecureSocketStream_SocketBufferSize;
	if (sizeToRead > maxSize) { sizeToRead = maxSize; }
	int nread = recv(sock, (char*)recvBuffer, sizeToRead, 0);
	if (nread <= 0)
	{
		_eos = true;
		return nread;
	}
	int outl;
	if (!EVP_DecryptUpdate(recvCipher, (unsigned char*)buffer, &outl, recvBuffer, nread))
	{
		_valid = false;
		return -1;
	}
	return nread;
}

int SecureSocketStream::readFixedSize(void* buffer, int size)
{
	if (!_valid) { return -1; }
	//ss.seekg(0, std::ios::end);
	//auto ssLen = (int)ss.tellg();
	//ss.seekg(0, std::ios::beg);
	//std::string ssLeftover(ss.str());
	//int ssLeftoverLength = (int)ssLeftover.length();

	int nreadCum = 0;
	auto ssLen = (int)ss.tellp();
	if (ssLen > 0)
	{
		if (ssLen > size)
		{
			ss.read((char*)buffer, size);
			nreadCum += size;
		}
		else
		{
			ss.read((char*)buffer, ssLen);
			nreadCum += ssLen;
		}
	}

	int nread = 0;
	int toRead;
	int outl;
	while (nreadCum < size)
	{
		toRead = SecureSocketStream_SocketBufferSize;
		if (toRead > size - nreadCum) { toRead = size - nreadCum; }
		nread = recv(sock, (char*)recvBuffer, toRead, 0);
		if (nread <= 0)
		{
			_eos = true;
			return nreadCum;
		}
		if (!EVP_DecryptUpdate(recvCipher, (unsigned char*)buffer + nreadCum, &outl, recvBuffer, nread))
		{
			fail();
			return -1;
		}
		nreadCum += outl;
	}

	return nreadCum;
}

int SecureSocketStream::write(const void* buffer, int size)
{
	if (!_valid) { return -1; }
	int pos;
	int outl;
	for (pos = 0; pos + SecureSocketStream_SocketBufferSize < size; pos += SecureSocketStream_SocketBufferSize)
	{
		if (!EVP_EncryptUpdate(sendCipher, sendBuffer, &outl, (unsigned char*)buffer + pos, SecureSocketStream_SocketBufferSize))
		{
			fail();
			return -1;
		}
		//send(sock, (char*)sendBuffer, outl, 0);
		sendLoop(sendBuffer, outl);
	}
	if (size - pos > 0)
	{
		if (!EVP_EncryptUpdate(sendCipher, sendBuffer, &outl, (unsigned char*)buffer + pos, size - pos))
		{
			fail();
			return -1;
		}
		//send(sock, (char*)sendBuffer, outl, 0);
		sendLoop(sendBuffer, outl);
	}
	return size;
}

bool SecureSocketStream::getline(std::string& outstr)
{
	auto ssLen = ss.tellp();
	std::string s3;
	char rbuf[SecureSocketStream_SocketBufferSize];
	int nread;
	while (true)
	{
		ssLen = ss.tellp();
		if (ssLen > 0)
		{
			s3 = ss.str();
			if (s3.find_first_of("\r\n") != std::string::npos)
			{
				std::getline(ss, outstr);
				return true;
			}
		}
		nread = readAnySize(rbuf, SecureSocketStream_SocketBufferSize);
		if (nread <= 0)
		{
			return fail();
		}
		ss.write(rbuf, nread);
	}
	return false;
}

int SecureSocketStream::sendLoop(const void* buffer, int size)
{
	if (!_valid) { return -1; }
	int nsentCum = 0;
	int nsent;
	while (nsentCum < size)
	{
		nsent = ::send(sock, (char*)buffer + nsentCum, size - nsentCum, 0);
		if (nsent <= 0)
		{
			fail();
			return nsentCum;
		}
		nsentCum += nsent;
	}
	return nsentCum;
}

int SecureSocketStream::sendLoop(const char* c_str)
{
	return sendLoop(c_str, (int)strlen(c_str));
}

