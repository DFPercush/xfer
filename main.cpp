
// Default port, Must be an integer between 1 and 65535, recommend higher than 10000
// Can be overridden by the -p option at run time
#define DEFAULT_PORT 50420

// Maximum transmission unit
// The maximum number of bytes that can be sent over the network
// in a single packet.
// This value takes into account up to 100 octets of headers.
#define DEFAULT_MTU 1400

#define DEFAULT_BUFFER_SIZE 0x4000

#include <sys/types.h>
#ifdef WIN32
//#include <Winsock.h>
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

//#include <boost/asio.hpp>
//using boost::asio::ip::tcp;
//boost::asio::
//tcp::socket
typedef long long bigint;


class MessageBuffer
{
public:
	std::mutex m;
	std::list<std::string> messages;
};

class ProgramOptions
{	
public:
	int mtu;
	int bufferSize;

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
		remote = nullptr;
		port = DEFAULT_PORT;
		mtu = DEFAULT_MTU;
		bufferSize = DEFAULT_BUFFER_SIZE;
	}
};

bool isIntString(const char* s)
{
	char testOut[30];
	int test = atoi(s);
	sprintf_s<30>(testOut, "%d", test);
	if (!strcmp(s, testOut)) { return true; }
	else { return false; }
}

/*************************************************************************************
addrinfo* lookup(const std::string& str)
{
	//union {
	//	sockaddr raw;
	//	sockaddr_in ip4;
	//	//sockaddr_in6 ip6;
	//} ret;
	int dots[3];
	unsigned char ip4addr[4]; // Will be stored big endian in this arrangement
	//unsigned char ip6addr[6];
	bool ip4 = true;
	bool ip6 = false;

	dots[0] = str.find_first_of('.', 0);
	if (dots[0] > 0) { dots[1] = str.find_first_of('.', dots[0] + 1); }
	else { ip4 = false; }
	if (ip4 && dots[1] > 0) { dots[2] = str.find_first_of('.', dots[1] + 1); }
	else { ip4 = false; }
	if (ip4 && dots[2] > 0)
	{
		int testVals[4];
		std::string subs[4];
		subs[0] = str.substr(0, dots[0]);
		subs[1] = str.substr(dots[0] + 1, dots[1] - dots[0] - 1);
		subs[2] = str.substr(dots[1] + 1, dots[2] - dots[1] - 1);
		subs[3] = str.substr(dots[2] + 1, str.length() - dots[2] - 1);
		if (isIntString(subs[0].c_str()) &&
			isIntString(subs[1].c_str()) &&
			isIntString(subs[2].c_str()) &&
			isIntString(subs[3].c_str()))
		{
			for (int i = 0; i < 4; i++)
			{
				testVals[i] = atoi(subs[i].c_str());
				if (testVals[i] < 0 || testVals[i] > 255)
				{
					ip4 = false;
					break;
				}
				ip4addr[i] = (unsigned char)testVals[i];
			}
		}
		else { ip4 = false; }

	}
	else { ip4 = false; }

	addrinfo* result;
	if (!ip4)
	{
		// argument is a domain or host name
		//auto host = gethostbyname(str.c_str());
		//host->
		getaddrinfo(str.c_str(), nullptr, nullptr, &result);
	}
	return result;
}
****************************************************************************/

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
		if (!strcmp(argv[i], "--")) { break; }
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
		else if (!strcmp(argv[i], "--")) 
		{
			i++;
			break; 
		}
		else if (!strcmp(argv[i], "--mtu"))
		{
			i++;
			if (i < argc)
			{
				if (isIntString(argv[i]))
				{
					op.mtu = atoi(argv[i]);
				}
				else
				{
					throw "--mtu requires an integer size";
				}
			}
			else
			{
				throw "--mtu requires a size";
			}
		}
		else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
		{
			op.verbose = true;
		}
		///////////////////////////////////////////////////////////////////////////////
		// Insert new commands here (above plz)
		//else if (argv[i][0] == '-')
		//{
		//	std::string emsg = "Unknown option ";
		//	emsg += argv[i];
		//	throw emsg.c_str();
		//}
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


bool sendFiles(SOCKET s, const ProgramOptions& op)
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
			//fileHeader = "FilenameLength:";
			//ss << fn.length();
			//ss >> numstr;
			//fileHeader += numstr;
			//fileHeader += (char)0x0A;

			fileHeader += "Filename:" + fn;
			fileHeader += (char)0x0A;
			fileHeader += "Size:";
			//char numbuf[30];
			//sprintf_s<30>(numbuf, "%)
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
				nr = fread(filebuf, 1, toRead, f);
				if (b.write(filebuf, nr) < (unsigned)nr)
				{
					fprintf(stderr, "Error: Connection closed early.\n");
					return false;
				}
				sent += nr;
			}
		}
	}
	b.write("(end)\n", 6);
	if (op.verbose)
	{
		printf("Wrote end of stream.\n");
	}
	return true;
}


bool sendFiles_x(SOCKET s, const ProgramOptions& op)
{
	//expects a connected socket
	int bufferSize = op.bufferSize;
	int bufferReads;
	char* buffer = nullptr;
	int mtu = op.mtu;
	int sendResult;

	if (mtu <= 0)
	{
		mtu = DEFAULT_MTU;
		bufferReads = DEFAULT_BUFFER_SIZE / DEFAULT_MTU;
	}
	else
	{
		if (bufferSize < mtu) { bufferSize = mtu; }
		bufferReads = bufferSize / op.mtu;
		buffer = new char[bufferSize];
	}
	if (!buffer) return false;

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
			}

			
			std::string fileHeader;
			//fileHeader = "FilenameLength:";
			//ss << fn.length();
			//ss >> numstr;
			//fileHeader += numstr;
			//fileHeader += (char)0x0A;

			fileHeader += "Filename:" + fn;
			fileHeader += (char)0x0A;
			fileHeader += "Size:";
			//char numbuf[30];
			//sprintf_s<30>(numbuf, "%)
			ss << fs.st_size;
			ss >> numstr;
			fileHeader += numstr;
			fileHeader += (char)0x0A;
			fileHeader += (char)0x0A;

			// TODO: send may send less data than you tell it to, need to buffer that
			sendResult = send(s, fileHeader.c_str(), fileHeader.length(), 0);
			// The "let the os handle buffering" strategy
			while (!feof(f))
			{
				auto nr = fread(buffer, 1, bufferSize, f);
				sendResult = send(s, buffer, nr, 0);
			}

			fclose(f);
			printf("Success: %s\n", fn.c_str());
		}
	}
	else
	{
		std::string fileHeader;
		if (op.destFilename == "")
		{
			fileHeader = "xfer.bin";
		}
		else
		{
			fileHeader = op.destFilename;
		}
		fileHeader += (char)0x0A;
		fileHeader += (char)0x0A;
		while (!feof(stdin))
		{
			auto nr = fread(buffer, 1, bufferSize, stdin);
			send(s, buffer, nr, 0);
		}
	}
	return true;
}

bool receiveFiles(SOCKET s, const ProgramOptions& op)
{
	std::string line, k, v;
	int colonPos;
	SocketStreamBuffer b(s);
	std::string fname;
	off_t fsize;
	std::stringstream ss;
	FILE* f;
	char buf[0x1000];
	off_t nx;

	fsize = 0;
	fname = "";
	int ll = 0;
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
						nr = b.read(buf, 0x1000);
						if (nr != 0x1000)
						{
							fprintf(stderr, "Warning: Connection closed early\n");
							conOpen = false;
						}
						fwrite(buf, 1, nr, f);
					}
					nr = b.read(buf, fsize - nx);
					if (nr != fsize - nx)
					{
						fprintf(stderr, "Warning: Connection closed early\n");
						conOpen = false;
					}
					fwrite(buf, 1, nr, f);
				}
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
					fname = v;
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

bool receiveFiles_x(SOCKET s, const ProgramOptions& op)
{
	//expects a connected socket
	int bufferSize;
	if (op.bufferSize <= 0) { bufferSize = 0x2000; }
	else { bufferSize = op.bufferSize; }
	char* buffer = new char[bufferSize];
	std::string line;
	std::stringstream lineStream, numstream;
	FILE* f = nullptr;
	struct {
		std::string name;
		off_t size;
	} fileInfo;
	fileInfo.name = "";
	fileInfo.size = 0;
	int rn = 1;
	int leftover = 0;
	off_t w; // number of bytes written for a particular file
	while (rn > 0)
	{
		w = 0;
		//while (rn = recv(s, buffer, bufferSize, 0))
		while ([&]() -> bool 
			{
				if (leftover == 0)
				{
					rn = recv(s, buffer, bufferSize, 0);
					if (rn > 0) { return true; }
					else { return false; }
				}
				else
				{
					int destPos = 0;
					int tempNewRn = rn - leftover;
					// Slow memcpy, must be direction safe, but probably faster than network anyway.
					while (leftover < rn)
					{
						buffer[destPos++] = buffer[leftover++];
					}
					leftover = 0;
					rn = tempNewRn;
					return true;
				}
			}())
		{
			line = "";
			for (int p = 0; p < rn; p++)
			{
				if (buffer[p] == 0x0D) {} // ignore CR
				if (buffer[p] == 0x0A) // Parse LF
				{
					lineStream.write(buffer, p);
					//lineStream.read(lineStream.getline())
					std::getline(lineStream, line);
					lineStream.clear();
					int pColon = line.find(':');
					if (pColon > 0)
					{
						std::string k, v;
						k = line.substr(0, pColon);
						v = line.substr(pColon + 1, line.length() - pColon - 1);
						if (k == "Filename")
						{
							fileInfo.name = v;
						}
						else if (k == "Size")
						{
							numstream.clear();
							numstream << v;
							numstream >> fileInfo.size;
						}
					}
					else if (line == "")
					{
						// Beginning of data
						fopen_s(&f, fileInfo.name.c_str(), "wb");
						if (!f)
						{
							fprintf(stderr, "Could not open file for writing: %s\n", fileInfo.name.c_str());
							fileInfo.size = 0;
						}
						else
						{
							w = rn - p - 1;
							if (fileInfo.size < w) 
							{
								leftover = w - fileInfo.size;
								w = fileInfo.size;
							}
							fwrite(&(buffer[p + 1]), 1, w, f);
							//if (leftover)
							//{
							//	lineStream.clear();
							//}
						}
						break;
					}
				}
				else
				{
					lineStream.write(buffer, rn);
				}
			}
		}

		while ((w < fileInfo.size) && (rn = recv(s, buffer, bufferSize, 0)))
		{
			if (w + rn > fileInfo.size)
			{
				fwrite(buffer, 1, fileInfo.size - w, f);
				w = fileInfo.size;
			}
			else
			{
				fwrite(buffer, 1, rn, f);
				w += rn;
			}
		}
		if (f) { fclose(f); }
		f = nullptr;
	}
	return true;
}

int submain(int argc, char** argv)
{
	if (argc < 2)
	{
		usage();
		return 7;
	}
#ifdef WIN32
	WSADATA winsockStartupInfo;
	WSAStartup(0x0101, &winsockStartupInfo);
	char curDir[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, curDir);
	printf("Current directory is %s\n", curDir);
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

