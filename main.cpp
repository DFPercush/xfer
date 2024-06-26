// xfer - secure file transfer

// Default port, Must be an integer between 1 and 65535, recommend higher than 10000
// Can be overridden by the -p option at run time
#define DEFAULT_PORT 12121


#if defined(WIN32) || defined(_WIN32) || defined(_WINDOWS_)

#include <Ws2tcpip.h>
#include <Windows.h>
#define cFilePathSeparator '\\'
#define sFilePathSeparator "\\"

#ifndef _WINDOWS_
#define _WINDOWS_
#endif

#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#define cFilePathSeparator '/'
#define sFilePathSeparator "/"
#define SOCKADDR_IN sockaddr_in
#define SOCKADDR sockaddr
#define closesocket(s) ::close(s)
#define INVALID_SOCKET -1
#endif

#define longbyte(v, b) ((v & (0xFF << (8 * b))) >> (8 * b))


#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <memory.h>
#include <inttypes.h>

#include <list>
#include <string>
//#include <mutex>
//#include <sstream>
#include <filesystem>
#include <iostream>

#include "secureSocketStream.h"

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

const char* xferVersionStr = "xfer_v0.0.2";
const off_t FileChunkSize = 0x400;

std::string htonPath(std::string p)
{
	for (size_t i = 0; i < p.length(); i++)
	{
		if (p[i] == cFilePathSeparator) { p[i] = '/'; }
	}
	return p;
}

std::string ntohPath(std::string p)
{
	for (size_t i = 0; i < p.length(); i++)
	{
		if (p[i] == '/') { p[i] = cFilePathSeparator; }
	}
	return p;
}

void clearLine()
{
#if defined(WIN32) || defined(_WIN32) || defined(_WINDOWS_)
	static bool initialized = false;
	if (!initialized)
	{
		// Set output mode to handle virtual terminal sequences
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE)
		{
			return;
		}

		DWORD dwMode = 0;
		if (!GetConsoleMode(hOut, &dwMode))
		{
			return;
		}

		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		if (!SetConsoleMode(hOut, dwMode))
		{
			return;
		}
		initialized = true;
		//return;
	}
	//printf("\033K");
	printf("\x1b[K");
	printf("\x1b[G");
#else
	printf("\033[2K\r");
#endif
}


bool isIntString(const char* s)
{
	char testOut[30];
	int test = atoi(s);
#ifdef _MSC_VER
	sprintf_s<30>(testOut, "%d", test);
#else
	sprintf(testOut, "%d", test);
#endif
	if (!strcmp(s, testOut)) { return true; }
	else { return false; }
}


class ProgramOptions
{
public:
	unsigned short port;

	unsigned int listen : 1;
	unsigned int serve : 1;
	unsigned int useFileList : 1;
	unsigned int verbose : 1;
	unsigned int quiet : 1;
	unsigned int showConnectedIp : 1;

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
		quiet = 0;
		showConnectedIp = 0;
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
	start = (b == std::string::npos) ? f : b;
	start++;
	return fullpath.substr(start, fullpath.length() - start);
}

std::string pathOnly(const std::string& fullPathAndFilename)
{
	auto lastSlash = fullPathAndFilename.find_last_of("/\\");
	if (lastSlash == std::string::npos) { return "."; }
	else { return fullPathAndFilename.substr(0, lastSlash); }
}

char usageString[] = 
"Usage: xfer [options] [files]\n"
"Options:\n"
"-c [host]\n"
"    Connect. Makes an outgoing tcp connection to the specified host address.\n"
"\n"
"-l\n"
"    Listen for incoming tcp connections.\n"
"\n"
"-p [port]\n"
"    Use a custom port number.\n"
"\n"
"-s\n"
"    Send files. This has no bearing on how the connection is established.\n"
"    The absence of this option means act as the receiver.\n"
"\n"
"-f [files]\n"
"    Give a list of files to send. Any text following this option,\n"
"    that is not itself an option, will be treated as a file name.\n"
"    If no -f is given on sender, reads file names one per line\n"
"    from stdin. Wild cards are not expanded here, use another tool \n"
"    like find to pipe in multiple file names, one per line.\n"
"\n"
"-v\n"
"    Verbose, show extra status information.\n"
"\n"
"-q\n"
"    Quiet, suppress all output (except maybe...)\n"
"\n"
"--showip\n"
"    Show the IP address of incoming connection even if -q is on.\n"
"\n"
"--\n"
"    Stop parsing options\n"
"\n"
"Examples:\n"
"  xfer -l -s -f myPhoto.jpg\n"
"    Listen as host and send file myPhoto.jpg\n"
"  xfer -l\n"
"    Listen as host and receive files.\n"
"  xfer -c 123.45.67.89\n"
"    Connect to IP address and receive files.\n"
"  xfer -c my.website.com -s -f myNotes.txt myLogo.png \n"
"    Connect to a host with a domain and upload files.\n"
"  find . 2> /dev/null | xfer -l -s\n"
"    (linux) Listen as host and send entire current directory.\n"
"\n"
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
			op.serve = 1u;
		}
		else if (!strcmp(argv[i], "-l"))
		{
			op.listen = 1u;
		}
		else if (!strcmp(argv[i], "-f"))
		{
			op.useFileList = 1u;
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
				//unsigned short testShort;
				//testShort = atoi(argv[i]);
				//char testStr[30];
				//sprintf_s<30>(testStr, "%d", testShort);
				//if (strcmp(testStr, argv[i]))
				if (!isIntString(argv[i]))
				{
					std::string emsg = "Not a valid TCP port number for -p: ";
					emsg += argv[i];
					throw emsg.c_str();
				}
				op.port = atoi(argv[i]);
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
			op.verbose = 1;
		}
		else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet"))
		{
			op.quiet = 1;
		}
		else if (!strcmp(argv[i], "--showip"))
		{
			op.showConnectedIp = true;
		}
		///////////////////////////////////////////////////////////////////////////////
		// Insert new commands here (above plz)
		else if (argv[i][0] == '-')
		{
			if (!op.quiet) fprintf(stderr, "Warning: %s looks like an option directive, but is not recognized. Treating as file name.\n", argv[i]);
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

bool sendFiles(SOCKET sock, ProgramOptions op)
{
	SecureSocketStream s(sock, op.listen, op.verbose);
	if (!s.valid())
	{
		if (!op.quiet) fprintf(stderr, "Encryption handshake error.\n");
		return false;
	}

	char fileBuf[FileChunkSize];
	unsigned char shamd[SHA256_DIGEST_LENGTH];

	auto sslcx = OSSL_LIB_CTX_new();
	//SHA256_CTX shacx;
	EVP_MD_CTX* shacx = EVP_MD_CTX_new();
	EVP_MD* shaimpl = EVP_MD_fetch(sslcx, "SHA256", "");


	s << xferVersionStr << "\n";

#ifdef _WINDOWS_
	struct _stat64 st;
#else
	struct stat st;
#endif

	time_t lastStatusTime, now;
	time(&lastStatusTime);
	
	if (!op.quiet) printf("Sending files...\n\n");
	
	FILE* f;
	decltype(op.files.begin()) itFilename;
	if (op.useFileList)
	{
		itFilename = op.files.begin();
	}
	
	//for (auto filename : op.files)
	std::string filename;
	bool firstFile = true;
	while (true)
	{
		// Emulate a for loop depending on which mode
		if ((!firstFile) && (op.useFileList)) { itFilename++; }
		if (op.useFileList)
		{
			if (itFilename == op.files.end()) { break; }
			filename = *itFilename;
		}
		else
		{
			if (std::cin.eof()) { break; }
			std::getline(std::cin, filename);
			if (std::cin.eof() && filename.length() == 0) { break; }
		}
		firstFile = false;
		// End for loop emulation

		if (!op.quiet) printf("%s\n", filename.c_str());
		//if (stat(filename.c_str(), &st))
#ifdef _WINDOWS_
		if (_stat64(filename.c_str(), &st))
#else
		if (stat(filename.c_str(), &st))
#endif
		{
			if (!op.quiet) fprintf(stderr, "  Warning: Read failed: %s\n", filename.c_str());
			//return false;
			continue;
		}
		f = fopen(filename.c_str(), "rb");
		if (!f) 
		{
			if (!op.quiet) fprintf(stderr, "Could not open %s\n", filename.c_str());
			return false;
		}

		//SHA256_Init(&shacx);
		//auto shaeng = ENGINE
		EVP_DigestInit_ex(shacx, shaimpl, nullptr);

		if (filename[0] == cFilePathSeparator)
		{
			filename = filename.substr(1);
		}
		else if (filename.substr(1, 2) == std::string(":") + sFilePathSeparator)
		{
			filename = filename.substr(2);
		}

		filename = htonPath(filename);
		s << st.st_size << " " << filename << "\n";
		

		off_t toSend;
		off_t nreadBuf;
		for (int64_t fpos = 0; fpos < st.st_size; fpos += nreadBuf)
		{
			if (!op.quiet)
			{
				time(&now);
				if (difftime(now, lastStatusTime) > 0.1)
				{
					// Don't bottleneck on console system calls.
					// It's been long enough.
					clearLine();
					printf("%" PRIu64 " / %" PRIu64, (uint64_t)fpos, (uint64_t)st.st_size);
					lastStatusTime = now;
				}
			}
			toSend = FileChunkSize;
			if (toSend > st.st_size - fpos) { toSend = (off_t)(st.st_size - fpos); }
			nreadBuf = (off_t) fread(fileBuf, 1, FileChunkSize, f);
			if (nreadBuf <= 0)
			{
				if (!op.quiet) fprintf(stderr, "File read error: %s", filename.c_str());
				return false;
			}
			//SHA256_Update(&shacx, fileBuf, nreadBuf);
			EVP_DigestUpdate(shacx, fileBuf, nreadBuf); 
			if (s.write(fileBuf, nreadBuf) <= 0)
			{
				if (!op.quiet) fprintf(stderr, "Socket write error\n");
				return false;
			}
		}

		if (!op.quiet)
		{
			clearLine();
			//fflush(stdout);
		}

		//SHA256_Final(shamd, &shacx);
		EVP_DigestFinal_ex(shacx, shamd, nullptr);
		if (s.write(shamd, SHA256_DIGEST_LENGTH) <= 0)
		{
			if (!op.quiet) fprintf(stderr, "Socket write error\n");
			return false;
		}
		firstFile = false;
	}
	s << "0 <END>\n";
	s.close();
	if (!op.quiet)
	{
		clearLine();
		printf("\nAll done!\n\n");
	}
	EVP_MD_free(shaimpl);
	EVP_MD_CTX_free(shacx);
	OSSL_LIB_CTX_free(sslcx);
	return true;
}

bool receiveFiles(SOCKET sock, ProgramOptions op)
{
	SecureSocketStream s(sock, op.listen, op.verbose);
	if (!s.valid()) 
	{
		if (!op.quiet) fprintf(stderr, "Encyption handshake error.\n");
		return false; 
	}

	std::string verStr;
	s.getline(verStr);
	if (op.verbose)
	{
		if (!op.quiet) printf("    My version: [%s]\nRemote version: [%s]\n", xferVersionStr, verStr.c_str());
		//fflush(stdout);
	}
	if (verStr != xferVersionStr)
	{
		if (!op.quiet) fprintf(stderr, "Error: Transfer protocol version mismatch.\n");
		return false;
	}

	std::string filename;
	long long fileSize;
	//SHA256_CTX shacx;
	unsigned char shamdHere[SHA256_DIGEST_LENGTH];
	unsigned char shamdThere[SHA256_DIGEST_LENGTH];
	unsigned char fileBuf[FileChunkSize];

	auto sslcx = OSSL_LIB_CTX_new();
	//SHA256_CTX shacx;
	EVP_MD_CTX* shacx = EVP_MD_CTX_new();
	EVP_MD* shaimpl = EVP_MD_fetch(sslcx, "SHA256", "");

	time_t lastStatusTime, now;
	time(&lastStatusTime);

	if (!op.quiet) printf("Receiving files...\n\n");
	//fflush(stdout);

	while (!s.eos())
	{
		s >> fileSize;
		s.read(fileBuf, 1);  // Skip space
		if (!s.getline(filename))
		{
			if (!op.quiet) fprintf(stderr, "Failed to read file name.\n");
			return false;
		}

		filename = ntohPath(filename);

		// Sanitize root paths.
		if (filename[0] == cFilePathSeparator)
		{
			// "/root/something"
			filename = filename.substr(1);
		}
		else if (filename.substr(1, 2) == std::string(":") + sFilePathSeparator)
		{
			// "C:\something"
			filename = filename.substr(2);
		}

		if (filename == "<END>" && fileSize == 0)
		{
			s.close();
			clearLine();
			if (!op.quiet) printf("\nAll done!\n\n");
			return true;
		}

		//if (!op.quiet) printf("%lld %s\n", fileSize, filename.c_str());
		if (!op.quiet) printf("%s\n", filename.c_str());
		//fflush(stdout);

		std::string parentDirToken = "..";
		parentDirToken += sFilePathSeparator;

		if (filename.find(parentDirToken) != std::string::npos)
		{
			fprintf(stderr, "Security warning: Destination path traverses parent directory. Aborting.\n");
			return false;
		}

		std::string logfilename = pathOnly(filename);

		// Create the directory if it doesn't exist.
		std::filesystem::create_directories(logfilename); // the variable 'logfilename' is somewhat confusing, it's just the path at this point.
		
		logfilename += sFilePathSeparator;
		logfilename += "xfer.log";
		time_t lognow;
		FILE* log = fopen(logfilename.c_str(), "a+");
		if (!log)
		{
			if (!op.quiet)
			{
				fprintf(stderr, "Error: Can not open log file %s\nAborting.", logfilename.c_str());
			}
			return false;
		}


		std::string fileRename = "";
		struct stat st, str;
		if (stat(filename.c_str(), &st) == 0)
		{
			// File already exists.
			int renameNumber = 0;
			char renameNumberBuf[16];
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif
			do
			{
				sprintf(renameNumberBuf, ".%.3d", renameNumber);
				fileRename = filename + renameNumberBuf;
				renameNumber++;
			} while (stat(fileRename.c_str(), &str) == 0);
			if (rename(filename.c_str(), fileRename.c_str()))
			{
				if (!op.quiet) { fprintf(stderr, "Can not rename old file. Aborting.\n"); }
				return false;
			}
			if (!op.quiet)
			{
				printf("  * Renamed old file: %s\n", fileRename.c_str());
			}
		}
		
		// TODO: Log file
		FILE* f = fopen(filename.c_str(), "wb");
		if (!f)
		{
			s.close();
			if (!op.quiet) fprintf(stderr, "Could not open %s\n", filename.c_str());
			return false;
		}




		time(&lognow);
		tm* lt = localtime(&lognow);
		SOCKADDR_IN sa;
		socklen_t salen = sizeof(sa);
		int sockerr;
		if (getpeername(sock, (SOCKADDR*)&sa, &salen))
		{
#ifdef _WINDOWS_
			sockerr = WSAGetLastError();
#else
			sockerr = -1;
#endif
			if (!op.quiet)
			{
				fprintf(stderr, "Could not get peer name, error code %d.\n", sockerr);
			}
			memset(&sa, 0, sizeof(sa));
		}

#ifdef _WINDOWS_
		fprintf(log, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d %d.%d.%d.%d => %s\n",
			lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
			lt->tm_hour, lt->tm_min, lt->tm_sec,
			sa.sin_addr.S_un.S_un_b.s_b1,
			sa.sin_addr.S_un.S_un_b.s_b2,
			sa.sin_addr.S_un.S_un_b.s_b3,
			sa.sin_addr.S_un.S_un_b.s_b4,
			filenameOnly(filename).c_str());
#else
		fprintf(log, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d %d.%d.%d.%d => %s\n",
			lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
			lt->tm_hour, lt->tm_min, lt->tm_sec,
			longbyte(sa.sin_addr.s_addr, 0),
			longbyte(sa.sin_addr.s_addr, 1),
			longbyte(sa.sin_addr.s_addr, 2),
			longbyte(sa.sin_addr.s_addr, 3),
			filenameOnly(filename).c_str());
#endif
		if (fileRename.length() > 0)
		{
			fprintf(log, "  Renamed old file: %s\n", filenameOnly(fileRename).c_str());
		}





		long long nreadCum = 0;
		int nread;
		int toRead;

		//SHA256_Init(&shacx);
		EVP_DigestInit_ex(shacx, shaimpl, nullptr);

		while (nreadCum < fileSize && !s.eos())
		{
			if (fileSize - nreadCum < FileChunkSize) { toRead = (int)(fileSize - nreadCum); }
			else { toRead = FileChunkSize; }
			nread = s.readAnySize(fileBuf, toRead);
			if (nread <= 0)
			{
				break;
			}
			if ((int)fwrite(fileBuf, 1, nread, f) < nread)
			{
				if (!op.quiet) fprintf(stderr, "File write error: %s\n", filename.c_str());
				return false;
			}
			//SHA256_Update(&shacx, fileBuf, nread);
			EVP_DigestUpdate(shacx, fileBuf, nread);

			if (!op.quiet)
			{
				time(&now);
				if (difftime(now, lastStatusTime) > 0.1)
				{
					// Don't bottleneck on console system calls.
					// It's been long enough.
					clearLine();
					printf("%lld / %lld", nreadCum, fileSize);
					lastStatusTime = now;
					//fflush(stdout);
				}
			}

			nreadCum += nread;
		}
		if (!op.quiet)
		{
			clearLine();
			//fflush(stdout);
		}
		if (s.eos()) { break; }
		fclose(f);
		f = nullptr;
		s.read(shamdThere, SHA256_DIGEST_LENGTH);
		//SHA256_Final(shamdHere, &shacx);
		EVP_DigestFinal_ex(shacx, shamdHere, nullptr);
		char shamdHereStr[65];
		char shamdThereStr[65];
		const char* hexMap = "0123456789abcdef";
		for (int si = 0; si < 64; si += 2)
		{
			shamdHereStr[si] = hexMap[(shamdHere[si >> 1] & 0xF0) >> 4];
			shamdHereStr[si + 1] = hexMap[shamdThere[si >> 1] & 0xF];
		}
		shamdThereStr[64] = 0;
		shamdHereStr[64] = 0;
		if (memcmp(shamdHere, shamdThere, SHA256_DIGEST_LENGTH))
		{
			fprintf(log, "  Bad hash, here %s, there %s \n", shamdHereStr, shamdThereStr);
			if (!op.quiet) fprintf(stderr, "Warning, data integrity violation, bad hash: %s\n", filename.c_str());
			if (!op.quiet) fprintf(stderr, "    ...Adding .err to file name\n");
			rename(filename.c_str(), (filename + ".err").c_str());
		}
		else
		{
			fprintf(log, "  SHA-256 = %s\n", shamdHereStr);
		}
		fclose(log);
	}
	if (!op.quiet) fprintf(stderr, "Error: Reached end of stream without an END marker from sender.\n");
	EVP_MD_free(shaimpl);
	EVP_MD_CTX_free(shacx);
	OSSL_LIB_CTX_free(sslcx);
	return false;
}

int submain(int argc, char** argv)
{
	int ret = 0;
	SSL_load_error_strings();
	//ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();

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
		if (!op.quiet) fprintf(stderr, "%s\n", s);
		return 1;
	}

#ifdef WIN32	
	if (op.verbose)
	{
		char curDir[MAX_PATH];
		GetCurrentDirectoryA(MAX_PATH, curDir);
		if (!op.quiet) printf("Current directory is %s\n", curDir);
	}
#endif

	if (op.serve)
	{
		std::string parentDirToken = "..";
		parentDirToken += sFilePathSeparator;
		for (std::string filename : op.files)
		{
			if (filename.find(parentDirToken) != std::string::npos)
			{
				fprintf(stderr, "Security warning: Destination path traverses parent directory. Aborting.\n");
				return 1;
			}
		}
	}


	if (op.listen)
	{
		sockaddr_in listenAddr;
		listenAddr.sin_family = AF_INET;
#ifdef _WINDOWS_
		listenAddr.sin_addr.S_un.S_addr = 0;
#else
		listenAddr.sin_addr.s_addr = 0;
#endif
		memset(listenAddr.sin_zero, 0, sizeof(listenAddr.sin_zero));
		listenAddr.sin_port = htons(op.port);
		auto listenSock = socket(AF_INET, SOCK_STREAM, 0);
		//u_long temp = 1;
		bind(listenSock, (sockaddr*)&listenAddr, sizeof(listenAddr));
		listen(listenSock, 1);
		sockaddr_in clientAddr;
		socklen_t clientAddrSize = sizeof(clientAddr);
		if (!op.quiet) printf("Waiting for connection on port %d ...\n", op.port);
		auto con = accept(listenSock, (sockaddr*)&clientAddr, &clientAddrSize);
		closesocket(listenSock);
#if defined(WIN32) || defined(_WIN32) || defined(_WINDOWS_)
		if (con == INVALID_SOCKET)
#else
		if (con == -1)
#endif
		{
			if (!op.quiet) fprintf(stderr, "No connections accepted.");
			return 6;
		}
		sockaddr_in peer;
		socklen_t peerSize = sizeof(peer);
		memset(&peer, 0, sizeof(peer));
		getpeername(con, (sockaddr*)&peer, &peerSize);
		
		if (!op.quiet || op. showConnectedIp)
		{
#ifdef _WINDOWS_
			printf("Connection accepted from %d.%d.%d.%d\n", 
				peer.sin_addr.S_un.S_un_b.s_b1, 
				peer.sin_addr.S_un.S_un_b.s_b2,
				peer.sin_addr.S_un.S_un_b.s_b3,
				peer.sin_addr.S_un.S_un_b.s_b4
				);
#else
			printf("Connection accepted from %d.%d.%d.%d\n",
				//peer.sin_addr.S_un.S_un_b.s_b1,
				//peer.sin_addr.S_un.S_un_b.s_b2,
				//peer.sin_addr.S_un.S_un_b.s_b3,
				//peer.sin_addr.S_un.S_un_b.s_b4
				longbyte(peer.sin_addr.s_addr, 0),
				longbyte(peer.sin_addr.s_addr, 1),
				longbyte(peer.sin_addr.s_addr, 2),
				longbyte(peer.sin_addr.s_addr, 3)
			);
#endif
		}

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
		if (!op.remote)
		{
			if (!op.quiet) fprintf(stderr, "No remote address specified.\n");
			return 2;
		}
		auto s = ::socket(AF_INET, SOCK_STREAM, 0);
		if (!s) 
		{
			if (!op.quiet) fprintf(stderr, "socket() returned null\n");
			return 3;
		}
		if (0 != connect(s, op.remote->ai_addr, sizeof(*op.remote->ai_addr)))
		{
#ifdef WIN32
			int e = WSAGetLastError();
			char emsg[200];
			FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, e, 0, emsg, 200, nullptr);
			if (!op.quiet) fprintf(stderr, "Could not open a connection to the remote. Winsock error %d - %s\n", e, emsg);
#else
			if (!op.quiet) fprintf(stderr, "Could not open a connection to the remote.\n");
#endif
			return 4;
		}

		if (!op.quiet)
		{
			printf("TCP Connection established.\n");
		}

		if (op.serve)
		{
			if (!sendFiles(s, op)) { ret = 1; }
		}
		else
		{
			if (receiveFiles(s, op)) { ret = 2; }
		}
	}
	return ret;
}

int main(int argc, char** argv)
{
	//bool pauseAnyway = false;
	int ret = submain(argc, argv);
//#ifdef WIN32
//	if ((ret != 0) || (pauseAnyway)) { system("pause"); }
//#endif
	return ret;
}

