
#include <sys/types.h>
#ifdef _WIN32
#include <Ws2tcpip.h>
#else
#include <socket.h>
#include <netdb.h>
#endif
#include <list>
#include <string>
#include <sstream>


class FileTransferReceiver
{
public:
	FileTransferReceiver(SOCKET s);
	int recv();
private:
	int recvChunk(const char* buffer, int size);
	SOCKET sock;
};

FileTransferReceiver::FileTransferReceiver(SOCKET s)
{
	sock = s;
}

int FileTransferReceiver::recv()
{
	// return # of files received
	return 0;
}

int FileTransferReceiver::recvChunk(const char* buffer, int size)
{
	return 0;
}

