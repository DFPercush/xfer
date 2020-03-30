
// Default port, Must be an integer between 1 and 65535, recommend higher than 10000
// Can be overridden by the -p option at run time
#define DEFAULT_PORT 50420

#include <sys/types.h>
#ifdef _WIN32
#include <Ws2tcpip.h>
#else
#include <socket.h>
#include <netdb.h>
#endif
#include <list>
#include <string>
#include <mutex>
#include <sstream>

#include "SocketStreamBuffer.h"
#include "secureSocketStream.h"

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

const char* xferVersionStr = "xfer_v0.0.2";
const off_t FileChunkSize = 0x400;

class ProgramOptions
{
public:
	unsigned short port;

	int listen : 1;
	int serve : 1;
	int useFileList : 1;
	int verbose : 1;

	std::list<std::string> files;
	std::string myName;
	std::string destFilename;
	addrinfo* remote;
	std::string connectionString;
	ProgramOptions()
	{
		listen = 0;
		serve = 0;
		useFileList = 0;
		verbose = 0;
		remote = nullptr;
		port = DEFAULT_PORT;
	}
};

std::string filenameOnly(const std::string& fullpath)
{
	size_t start = 0;
	size_t f = fullpath.find_last_of('/');
	size_t b = fullpath.find_last_of('\\');
	if (f == b && b == std::string::npos) { return fullpath; }
	start = (f > b) ? f : b;
	start++;
	return fullpath.substr(start, fullpath.length() - start);
}

bool isIntString(const char* s)
{
	char testOut[30];
	int test = atoi(s);
	sprintf_s<30>(testOut, "%d", test);
	if (!strcmp(s, testOut)) { return true; }
	else { return false; }
}

char usageString[] = 
"Usage: xfer [options] [files]\n"
"Options:\n"
"-- Stop parsing options\n"
"-c [host] Connect. Makes an outgoing tcp connection to the specified host address.\n"
"-l Listen for incoming tcp connections.\n"
"-p [port] Use a custom port number.\n"
"-s Send files. This has no bearing on how the connection is established.\n"
"   The absence of this option means act as the receiver.\n"
"-f [files] Give a list of files to send. Any text following this option,\n"
"   that is not itself an option, will be treated as a file name.\n"
;

void usage()
{
	printf("%s", usageString);
}

ProgramOptions parseCmd(int argc, char** argv)
{
	ProgramOptions op;
	int lastProcessedArg = 0;
	for (int i = 1; i < argc; i++)
	{
		lastProcessedArg = i;
		if (!strcmp(argv[i], "--")) 
		{
			lastProcessedArg++;
			break; 
		}
		else if (!strcmp(argv[i], "-s"))
		{
			op.serve = 1;
		}
		else if (!strcmp(argv[i], "-l"))
		{
			op.listen = 1;
		}
		else if (!strcmp(argv[i], "-f"))
		{
			op.useFileList = 1;
		}
		else if (!strcmp(argv[i], "-n"))
		{
			i++;
			if (i < argc)
			{
				op.myName = argv[i];
			}
			else
			{
				throw "-n requires a name, this is your name for this session";
			}
		}
		else if (!strcmp(argv[i], "-p")) // port
		{
			i++;
			if (i < argc)
			{
				unsigned short testShort;
				testShort = atoi(argv[i]);
				char testStr[30];
				sprintf_s<30>(testStr, "%d", testShort);
				if (strcmp(testStr, argv[i]))
				{
					std::string emsg = "Not a valid TCP port number for -p: ";
					emsg += argv[i];
					throw emsg.c_str();
				}
				op.port = testShort;
				if (op.remote)
				{
					((sockaddr_in*)op.remote->ai_addr)->sin_port = htons(op.port);
				}
			}
			else 
			{
				throw  "-p requires a TCP port number";
			}
		}
		else if (!strcmp(argv[i], "-c")) // connect
		{
			i++;
			if (i < argc)
			{
				op.connectionString = argv[i];
				if (0 != getaddrinfo(argv[i], nullptr, nullptr, &op.remote))
				{
					std::string emsg = "Could not look up remote address ";
					emsg += argv[i];
					throw emsg.c_str();
				}
				((sockaddr_in*)op.remote->ai_addr)->sin_port = htons(op.port);
			}
			else
			{
				throw "-c requires an address to connect";
			}
		}
		else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
		{
			op.verbose = true;
		}
		///////////////////////////////////////////////////////////////////////////////
		// Insert new commands here (above plz)
		else if (argv[i][0] == '-')
		{
			fprintf(stderr, "Warning: %s looks like an option directive, but is not recognized. Treating as file name.\n", argv[i]);
			op.files.push_back(argv[i]);
		}
		else
		{
			op.files.push_back(argv[i]);
		}
		lastProcessedArg = i + 1;
	}
	if (op.useFileList)
	{
		for (int fi = lastProcessedArg; fi < argc; fi++)
		{
			op.files.push_back(argv[fi]);
		}
	}
	return op;
}


bool x2_sendFiles(SOCKET s, const ProgramOptions& op)
{
	char buf[0x1000];
	struct stat fs;
	for (auto filename : op.files)
	{
		if (filename.find("..") != std::string::npos)
		{
			fprintf(stderr, "Illegal file name, .. not allowed: %s\n", filename.c_str());
			continue;
		}
		if (stat(filename.c_str(), &fs))
		{
			fprintf(stderr, "Error getting file info for %s\n", filename.c_str());
		}
		FILE* f = fopen(filename.c_str(), "rb");
		if (!f)
		{
			fprintf(stderr, "Error opening %s\n", filename.c_str());
			continue;
		}

		// Send null terminated length + file name
		char numbuf[40];
		sprintf_s<40>(numbuf, "%lld ", (long long)fs.st_size);
		send(s, numbuf, (int)strlen(numbuf), 0);
		send(s, filename.c_str(), (int)filename.length() + 1, 0);
		while (!feof(f))
		{
			int nread = (int)fread(buf, 1, sizeof(buf), f);
			send(s, buf, nread, 0);
		}
	}
	return false;
}

bool x_sendFiles(SOCKET s, const ProgramOptions& op)
{
	SocketStreamBuffer b(s);
	char filebuf[0x1000];
	if (op.useFileList)
	{
		for (auto fn : op.files)
		{
			std::stringstream ss;
			std::string numstr;
			struct stat fs;
			FILE* f;
			stat(fn.c_str(), &fs);
			fopen_s(&f, fn.c_str(), "rb");
			if (!f)
			{
				fprintf(stderr, "File not found: %s\n", fn.c_str());
				continue;
			}


			std::string fileHeader;

			fileHeader += "Filename:" + filenameOnly(fn);
			fileHeader += (char)0x0A;
			fileHeader += "Size:";
			ss << fs.st_size;
			ss >> numstr;
			fileHeader += numstr;
			fileHeader += (char)0x0A;
			fileHeader += (char)0x0A;

			int nr;
			off_t sent = 0;
			if (b.write(fileHeader.c_str(), fileHeader.length()) < fileHeader.length())
			{
				fprintf(stderr, "Error: Connection closed early.\n");
				return false;
			}
			if (op.verbose)
			{
				printf("%s", fileHeader.c_str());
			}
			int toRead;
			while ((!feof(f)) && (sent < fs.st_size))
			{
				toRead = (fs.st_size - sent);
				if (toRead > 0x1000) { toRead = 0x1000; }
				nr = (int)fread(filebuf, 1, toRead, f);
				if (b.write(filebuf, nr) < (unsigned)nr)
				{
					fprintf(stderr, "Error: Connection closed early.\n");
					return false;
				}
				sent += nr;
			}
			fclose(f);
		}
	}
	b.write("(end)\n", 6);
	if (op.verbose)
	{
		printf("Finished sending.\n");
	}
	return true;
}

bool x2_receiveFiles(SOCKET s, const ProgramOptions& op)
{
	char rbuf[0x1000];
	int nread = 1;
	std::string filename;
	std::stringstream nameAndSizeLine;
	off_t fileSize = 0;
	off_t filePos = 0;
	FILE* fout = nullptr;
	bool readingFileContents = false;
	while (nread > 0)
	{
		nread = recv(s, rbuf, 0x1000, 0);
		off_t nullCharPos = 0;
		bool foundNullChar = false;
		if (readingFileContents)
		{
			if (filePos + nread > fileSize)
			{
				fwrite(rbuf, 1, fileSize - (filePos), fout);
				nameAndSizeLine.clear();
				nameAndSizeLine.write(rbuf + (fileSize - filePos), nread - (fileSize - filePos));
				//nameAndSizeLine.rdbuf()->in_avail();
				//nameAndSizeLine.str();
				fclose(fout);
				fout = nullptr;
				filePos = 0;
				fileSize = 0;
				readingFileContents = false;
			}
			else
			{
				fwrite(rbuf, 1, nread, fout);
				filePos += nread;
			}
			filePos += nread;
		}
		
		if (!readingFileContents)
		{
			for (off_t pos = 0; pos < nread; pos++)
			{
				if (rbuf[pos] == 0)
				{
					foundNullChar = true;
					nullCharPos = pos;
					break;
				}
			}
			if (foundNullChar)
			{
				nameAndSizeLine.write(rbuf, nullCharPos);
				nameAndSizeLine >> fileSize;
				filename = nameAndSizeLine.str();
				if (filename.find("..") != std::string::npos)
				{
					fprintf(stderr, "Security warning: Remote is trying to traverse parent directory, terminating.\n");
					#ifdef WIN32
					closesocket(s);
					#else
					close(s);
					#endif
					return 1;
				}
				nameAndSizeLine.clear();
				fout = fopen(filename.c_str(), "wb");
				if (!fout)
				{
					fprintf(stderr, "Error opening %s", filename.c_str());
					// Continue trying for any other files.
				}
				//fwrite(rbuf + nullCharPos, 1, nread - nullCharPos, fout);
				//filePos += 
			}
			else
			{
				nameAndSizeLine.write(rbuf, nread);
			}
		}
	}
	return false;
}

bool x_receiveFiles(SOCKET s, const ProgramOptions& op)
{
	std::string line, k, v;
	size_t colonPos;
	SocketStreamBuffer b(s);
	std::string fname;
	off_t fsize;
	std::stringstream ss;
	FILE* f;
	char buf[0x1000];
	off_t nx;

	fsize = 0;
	fname = "";
	size_t ll = 0;
	bool conOpen = true;
	int nr;
	while (conOpen)
	{
		ll = b.getline(line);
		if (ll == 0)
		{
			if (fname.length() > 0 && fsize > 0)
			{
				f = nullptr;
				fopen_s(&f, fname.c_str(), "wb");
				if (!f)
				{
					fprintf(stderr, "Could not open file for writing: %s", fname.c_str());
					return false;
				}
				else
				{
					for (nx = 0; nx + 0x1000 < fsize; nx += 0x1000)
					{
						nr = (int)b.read(buf, 0x1000);
						if (nr != 0x1000)
						{
							fprintf(stderr, "Warning: Connection closed early\n");
							conOpen = false;
						}
						fwrite(buf, 1, nr, f);
					}
					nr = (int)b.read(buf, (int)(fsize - nx));
					if (nr != fsize - nx)
					{
						fprintf(stderr, "Warning: Connection closed early\n");
						conOpen = false;
					}
					fwrite(buf, 1, nr, f);
				}
				fclose(f);
			}
			else
			{
				fprintf(stderr, "Error, missing required file header.");
				return false;
			}
		}
		else if (line == "(end)")
		{
			conOpen = false;
			if (op.verbose)
			{
				printf("End of stream OK, closing.\n");
			}
		}
		else
		{
			if (op.verbose) { printf("%s\n", line.c_str()); }
			colonPos = line.find(':');
			if (colonPos > 0)
			{
				k = line.substr(0, colonPos);
				v = line.substr(colonPos + 1, line.length() - colonPos - 1);
				if (k == "Filename")
				{
					fname = filenameOnly(v);
				}
				else if (k == "Size")
				{
					ss.clear();
					ss << v;
					ss >> fsize;
					ss.clear();
				}
			}
		}
	}
	return true;
}

bool sendFiles(SOCKET sock, ProgramOptions op)
{
	SecureSocketStream s(sock, op.listen);
	if (!s.valid()) { return false; }
	char fileBuf[FileChunkSize];
	unsigned char shamd[SHA256_DIGEST_LENGTH];

	SHA256_CTX shacx;

	s << xferVersionStr << "\n";
	struct stat st;
	FILE* f;
	for (auto filename : op.files)
	{
		if (stat(filename.c_str(), &st))
		{
			fprintf(stderr, "Failed to get file information for %s\n", filename.c_str());
			return false;
		}
		f = fopen(filename.c_str(), "rb");
		if (!f) 
		{
			fprintf(stderr, "Could not open %s\n", filename.c_str());
			return false;
		}

		SHA256_Init(&shacx);

		s << st.st_size << filename << "\n";
		off_t toSend;
		off_t nreadBuf;
		for (off_t fpos = 0; fpos < st.st_size; fpos += nreadBuf)
		{
			toSend = FileChunkSize;
			if (toSend > st.st_size - fpos) { toSend = st.st_size - fpos; }
			nreadBuf = (off_t) fread(fileBuf, 1, FileChunkSize, f);
			if (nreadBuf <= 0)
			{
				fprintf(stderr, "File read error: %s", filename.c_str());
				return false;
			}
			SHA256_Update(&shacx, fileBuf, nreadBuf);
			if (s.write(fileBuf, nreadBuf) <= 0)
			{
				fprintf(stderr, "Socket write error\n");
				return false;
			}
		}

		SHA256_Final(shamd, &shacx);
		if (s.write(shamd, SHA256_DIGEST_LENGTH) <= 0)
		{
			fprintf(stderr, "Socket write error\n");
			return false;
		}
	}
	s << "0 <END>\n";
	s.close();
	return true;
}

bool receiveFiles(SOCKET sock, ProgramOptions op)
{
	SecureSocketStream s(sock, op.listen);
	if (!s.valid()) { return false; }

	std::string verStr;
	s.getline(verStr);
	if (verStr != xferVersionStr)
	{
		fprintf(stderr, "Error: Transfer protocol version mismatch.\n");
		return false;
	}

	std::string filename;
	off_t fileSize;
	SHA256_CTX shacx;
	FILE* f;
	unsigned char shamdHere[SHA256_DIGEST_LENGTH];
	unsigned char shamdThere[SHA256_DIGEST_LENGTH];
	unsigned char fileBuf[FileChunkSize];

	while (!s.eos())
	{
		s >> fileSize;
		if (!s.getline(filename))
		{
			fprintf(stderr, "Failed to read file name.\n");
			return false;
		}
		if (filename == "<END>" && fileSize == 0)
		{
			s.close();
			printf("All done!\n");
			return true;
		}

		off_t nreadCum = 0;
		int nread;
		int toRead;
		f = fopen(filename.c_str(), "wb");
		if (!f)
		{
			s.close();
			fprintf(stderr, "Could not open %s\n", filename.c_str());
			return false;
		}
		SHA256_Init(&shacx);
		while (nreadCum < fileSize && !s.eos())
		{
			if (fileSize - nreadCum < FileChunkSize) { toRead = fileSize - nreadCum; }
			else { toRead = FileChunkSize; }
			nread = s.readAnySize(fileBuf, toRead);
			if (nread <= 0)
			{
				break;
			}
			SHA256_Update(&shacx, fileBuf, nread);

			nreadCum += nread;
		}
		if (s.eos()) { break; }
		fclose(f);
		s.read(shamdThere, SHA256_DIGEST_LENGTH);
		SHA256_Final(shamdHere, &shacx);
		if (memcmp(shamdHere, shamdThere, SHA256_DIGEST_LENGTH))
		{
			fprintf(stderr, "Warning, data integrity violation, bad hash: %s\n", filename.c_str());
			fprintf(stderr, "    ...Adding .err to file name\n");
			rename(filename.c_str(), (filename + ".err").c_str());
		}
	}
	fprintf(stderr, "Error: Reached end of stream without an END marker from sender.\n");
	return false;
}

int submain(int argc, char** argv)
{

	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();

	/*****************************************************
	// Set up a Diffie-Hellman key exchange
	auto bignumContext = BN_CTX_new();
	auto publicBase = BN_new();
	auto publicMod = BN_new();
	auto mySecret = BN_new();
	auto myPreKey = BN_new();
	auto remoteExchangeKey = BN_new();

	// Only one party can generate the public exchange numbers.
	BN_generate_prime_ex2(publicMod, 1024, 1, nullptr, nullptr, nullptr, bignumContext);
	BN_rand(publicBase, 64, 1, 1);

	BN_mod_exp(myPreKey, publicBase, mySecret, publicMod, bignumContext);

	unsigned long myExchangeKeySize = BN_num_bytes(myPreKey);
	unsigned char* myExchangeKeyBuffer = new unsigned char[myExchangeKeySize];
	BN_bn2bin(myPreKey, myExchangeKeyBuffer);

	delete[] myExchangeKeyBuffer;
	BN_free(publicBase);
	BN_free(publicMod);
	BN_free(myPreKey);
	BN_free(mySecret);
	BN_free(remoteExchangeKey);

	***************************************************************/

	if (argc < 2)
	{
		usage();
		return 7;
	}
#ifdef WIN32
	WSADATA winsockStartupInfo;
	WSAStartup(0x0101, &winsockStartupInfo);
#endif

	ProgramOptions op;
	try
	{
		op = parseCmd(argc, argv);
	}
	catch (const char* s)
	{
		fprintf(stderr, "%s\n", s);
		return 1;
	}

#ifdef WIN32	
	if (op.verbose)
	{
		char curDir[MAX_PATH];
		GetCurrentDirectoryA(MAX_PATH, curDir);
		printf("Current directory is %s\n", curDir);
	}
#endif

	SecureSocketStream ss;
	if (op.listen)
	{
		sockaddr_in listenAddr;
		listenAddr.sin_family = AF_INET;
		listenAddr.sin_addr.S_un.S_addr = 0;
		memset(listenAddr.sin_zero, 0, sizeof(listenAddr.sin_zero));
		listenAddr.sin_port = htons(op.port);
		auto listenSock = socket(AF_INET, SOCK_STREAM, 0);
		u_long temp = 1;
		bind(listenSock, (sockaddr*)&listenAddr, sizeof(listenAddr));
		listen(listenSock, 1);
		sockaddr_in clientAddr;
		int clientAddrSize = sizeof(clientAddr);
		auto con = accept(listenSock, (sockaddr*)&clientAddr, &clientAddrSize);
		closesocket(listenSock);
		if (con == -1)
		{
			fprintf(stderr, "No connections accepted.");
			return 6;
		}
		sockaddr_in peer;
		int peerSize = sizeof(peer);
		memset(&peer, 0, sizeof(peer));
		getpeername(con, (sockaddr*)&peer, &peerSize);
		
		printf("Connection accepted from %d.%d.%d.%d\n", 
			peer.sin_addr.S_un.S_un_b.s_b1, 
			peer.sin_addr.S_un.S_un_b.s_b2,
			peer.sin_addr.S_un.S_un_b.s_b3,
			peer.sin_addr.S_un.S_un_b.s_b4
			);

		
		//SecureSocketStream sockServer(con, true);
		ss.begin(con, true);
		if (ss.valid())
		{
			printf("Connection established.\n");
		}
		else
		{
			printf("Connection failed.\n");
		}
		return 0;

		if (op.serve)
		{
			sendFiles(con, op);
		}
		else
		{
			receiveFiles(con, op);
		}
	}
	else // outgoing connection
	{
		//boost::asio::io_service ios;
		//tcp::resolver resolver(ios);
		//tcp::socket outSock(ios);
		//std::stringstream ssPort;
		//ssPort << op.port;
		//std::string portStr;
		//ssPort >> portStr;
		//tcp::resolver::query query(op.connectionString, portStr);
		////for (tcp::resolver::iterator endpoint_iterator = resolver.resolve(query); endpoint_iterator != resolver
		//tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		//boost::asio::connect(outSock, endpoint_iterator);
		//outSock.send

		if (!op.remote)
		{
			fprintf(stderr, "No remote address specified.\n");
			return 2;
		}
		auto s = ::socket(AF_INET, SOCK_STREAM, 0);
		if (!s) 
		{
			fprintf(stderr, "socket() returned null\n");
			return 3;
		}
		if (0 != connect(s, op.remote->ai_addr, sizeof(*op.remote->ai_addr)))
		{
#ifdef WIN32
			int e = WSAGetLastError();
			char emsg[200];
			FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, e, 0, emsg, 200, nullptr);
			fprintf(stderr, "Could not open a connection to the remote. Winsock error %d - %s\n", e, emsg);
#else
			fprintf(stderr, "Could not open a connection to the remote.\n");
#endif
			return 4;
		}

		ss.begin(s, false);
		if (ss.valid())
		{
			printf("Connection established.\n");
		}
		else
		{
			printf("Connection failed.\n");
		}
		return 0;


		if (op.serve)
		{
			sendFiles(s, op);
		}
		else
		{
			receiveFiles(s, op);
		}
	}
	return 0;
}

int main(int argc, char** argv)
{
	bool pauseAnyway = false;
	int ret = submain(argc, argv);
#ifdef WIN32
	if ((ret != 0) || (pauseAnyway)) { system("pause"); }
#endif
	return ret;
}

