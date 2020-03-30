/*
SocketStreamBuffer must be initialized with a connected SOCKET.
Handling the connection setup is up to you, but this class can help you consume the correct number of octets from the stream.
Using this class ensures that the full length of data you specify to read or write gets processed all in one call, assuming the connection stays good.

If you use this class to handle a socket, don't try to manually send() and recv() on that socket, because there could be data
in this object's buffers that you will lose. To empty these buffers, call readCache() and flush().

The destructor closes the socket.
*/

#ifndef SocketStreamBuffer_h
#define SocketStreamBuffer_h

#include <sys/types.h>
#if defined(WIN32) || defined(_WIN32)
//#include <Winsock.h>
#include <Ws2tcpip.h>
#else
#include <socket.h>
#include <netdb.h>
#endif

#include <sstream>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>



class SocketStreamBuffer
{
public:
	SocketStreamBuffer() = delete;
	SocketStreamBuffer(SOCKET sp);
	virtual ~SocketStreamBuffer();
	SocketStreamBuffer(const SocketStreamBuffer&) = delete; // no copy, move only
	SocketStreamBuffer(SocketStreamBuffer&& mv) = default;
	SocketStreamBuffer& operator=(const SocketStreamBuffer&) = delete; // no copy, move only
	SocketStreamBuffer& operator=(SocketStreamBuffer&& mv) = default;

	size_t read(char* buf, size_t len); // Always reads [len] octets if connection is active.
	size_t readCache(char* buf, size_t len); // Reads from internal buffers, but does not make any system calls to read the socket.
	size_t write(const char* buf, size_t len); // Accumulates data in internal buffers and sends when full. Large data sets may be partially sent. Call flush() to send immediately. 
	bool flush(); // Sends any buffered written data immediately. No effect on incoming readable data.
	size_t getline(std::string& out);

protected:

	SOCKET s;
	struct ReadBuffer_t
	{
		char* ptr;
		size_t allocSize;
		size_t recvSize;
		size_t pos;
	};
	ReadBuffer_t readBuffers[2];
	ReadBuffer_t* activeReadBuffer;

	struct WriteBuffer_t
	{
		char* ptr;
		size_t allocSize;
		size_t pos;
		size_t sent;
	};
	WriteBuffer_t writeBuffer;

	void swapBuffers();

private:
	void socketStreamBuffer_init();
};

#endif
