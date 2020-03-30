
#include "SocketStreamBuffer.h"

#define DEFAULT_BUFFER_SIZE 0x1000


void SocketStreamBuffer::socketStreamBuffer_init()
{
	readBuffers[0].ptr = new char[DEFAULT_BUFFER_SIZE];
	readBuffers[0].allocSize = DEFAULT_BUFFER_SIZE;
	readBuffers[0].recvSize = 0;
	readBuffers[0].pos = 0;

	readBuffers[1].ptr = new char[DEFAULT_BUFFER_SIZE];
	readBuffers[1].allocSize = DEFAULT_BUFFER_SIZE;
	readBuffers[1].recvSize = 0;
	readBuffers[1].pos = 0;

	activeReadBuffer = &(readBuffers[0]);

	writeBuffer.ptr = new char[DEFAULT_BUFFER_SIZE];
	writeBuffer.allocSize = DEFAULT_BUFFER_SIZE;
	writeBuffer.pos = 0;
	writeBuffer.sent = 0;
	s = INVALID_SOCKET; // (~0)
}

//SocketStreamBuffer::SocketStreamBuffer()
//{
//	socketStreamBuffer_init();
//}

SocketStreamBuffer::SocketStreamBuffer(SOCKET sp)
{
	socketStreamBuffer_init();
	s = sp;
}

SocketStreamBuffer::~SocketStreamBuffer()
{
	if (readBuffers[0].ptr != nullptr) { delete[] readBuffers[0].ptr; }
	readBuffers[0].ptr = nullptr;
	if (readBuffers[1].ptr != nullptr) { delete[] readBuffers[1].ptr; }
	readBuffers[1].ptr = nullptr;
#ifdef WIN32
	closesocket(s);
#else
	close(s);
#endif
	s = 0;
}

//void SocketStreamBuffer::readChunk(int maxlen)
//{
//	activeReadBuffer->
//}

size_t SocketStreamBuffer::read(char* buf, size_t len)
{
	char* curOutPtr = buf;
	size_t curOutLen = 0;
	int copyLen = activeReadBuffer->recvSize - activeReadBuffer->pos;
	if (copyLen > 0) //activeReadBuffer->recvSize > activeReadBuffer->pos)
	{
		if ((unsigned)copyLen > len) { copyLen = len; }
		memcpy(curOutPtr, &(activeReadBuffer->ptr[activeReadBuffer->pos]), copyLen);
		activeReadBuffer->pos += copyLen;
		curOutLen += copyLen;
		curOutPtr += copyLen;
	}
	while (curOutLen < len)
	{
		swapBuffers();
		activeReadBuffer->recvSize = recv(s, activeReadBuffer->ptr, activeReadBuffer->allocSize, 0);
		if (activeReadBuffer->recvSize == 0) { return curOutLen; }
		if (activeReadBuffer->recvSize < (len - curOutLen)) 
		{
			memcpy(curOutPtr, activeReadBuffer->ptr, activeReadBuffer->recvSize);
			activeReadBuffer->pos += activeReadBuffer->recvSize;
			curOutLen += activeReadBuffer->recvSize;
			curOutPtr += activeReadBuffer->recvSize;
		}
		else
		{
			copyLen = (len - curOutLen);
			memcpy(curOutPtr, activeReadBuffer->ptr, copyLen);
			activeReadBuffer->pos += copyLen;
			curOutLen += copyLen;
			curOutPtr += copyLen;
		}
	}
	return curOutLen;
}

size_t SocketStreamBuffer::readCache(char* buf, size_t len)
{
	int copyLen = activeReadBuffer->recvSize - activeReadBuffer->pos;
	if (copyLen > 0)
	{
		if ((unsigned)copyLen > len) { copyLen = len; }
		memcpy(buf, &(activeReadBuffer->ptr[activeReadBuffer->pos]), copyLen);
		activeReadBuffer->pos += copyLen;
		return copyLen;
	}
	return 0;
}

size_t SocketStreamBuffer::write(const char* buf, size_t len)
{
	size_t curPos = 0;
	int nSent;
	size_t copyLen = writeBuffer.allocSize - writeBuffer.pos;
	if (len < copyLen) { copyLen = len; }
	memcpy(&(writeBuffer.ptr[writeBuffer.pos]), &(buf[curPos]), copyLen);
	writeBuffer.pos += copyLen;
	curPos += copyLen;
	while (writeBuffer.sent < writeBuffer.pos)
	{
		nSent = send(s, &(writeBuffer.ptr[writeBuffer.sent]), writeBuffer.pos - writeBuffer.sent, 0);
		if (nSent == SOCKET_ERROR) {} // TODO: error handling
		writeBuffer.sent += nSent;
	}
	writeBuffer.pos = 0;
	writeBuffer.sent = 0;
	while (curPos + writeBuffer.allocSize <= len)
	{
		nSent = send(s, &(buf[curPos]), (len - curPos), 0);
		if (nSent == SOCKET_ERROR) {} // TODO: error handling
		curPos += nSent;
	}
	if (curPos < len)
	{
		writeBuffer.pos = len - curPos;
		memcpy(writeBuffer.ptr, &(buf[curPos]), writeBuffer.pos);
	}
	return curPos;
}

bool SocketStreamBuffer::flush()
{
	int nSent;
	while (writeBuffer.sent < writeBuffer.pos)
	{
		nSent = send(s, &(writeBuffer.ptr[writeBuffer.sent]), writeBuffer.pos - writeBuffer.sent, 0);
		if (nSent == SOCKET_ERROR) { return false; } // TODO: error handling
		writeBuffer.sent += nSent;
	}
	writeBuffer.pos = 0;
	writeBuffer.sent = 0;
	return true;
}

void SocketStreamBuffer::swapBuffers()
{
	if (activeReadBuffer == &(readBuffers[0])) { activeReadBuffer = &(readBuffers[1]); }
	else { activeReadBuffer = &(readBuffers[0]); }
	activeReadBuffer->pos = 0;
	activeReadBuffer->recvSize = 0;
}

size_t SocketStreamBuffer::getline(std::string& out)
{
	out = "";
	char c = 0;
	char prevc = 0;
	int linePos = 0;
	while (true)
	{
		while (activeReadBuffer->pos < activeReadBuffer->recvSize)
		{
			prevc = c;
			c = activeReadBuffer->ptr[activeReadBuffer->pos];
			activeReadBuffer->pos++;
			if (c == 0x0D)
			{
			}
			else if (c == 0x0A)
			{
				return linePos;
			}
			else
			{
				out.push_back(c);
			}
			linePos++;
		}
		swapBuffers();
		activeReadBuffer->recvSize = recv(s, activeReadBuffer->ptr, activeReadBuffer->allocSize, 0);
	}

	return linePos;
}

