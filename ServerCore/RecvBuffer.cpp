#include "pch.h"
#include "RecvBuffer.h"

/*----------------------
	RecvBuffer
	역할: 수신 버퍼 관리
------------------------*/

RecvBuffer::RecvBuffer(int32 bufferSize) : _bufferSize(bufferSize)
{
	_capacity = bufferSize * BUFFER_COUNT;
	_buffer.resize(_capacity);
}

RecvBuffer::~RecvBuffer()
{
}

void RecvBuffer::Clean()
{
	int32 dataSize = DataSize();
	if (dataSize == 0)
	{
		// 마침 읽기, 쓰기의 커서 위치가 동일위치라면 둘 다 초기화.
		_readPos = _writePos = 0;
	}
	else
	{
		// [][][][][][][][r][][w] -> [r][][w][][][][][][]
		// 데이터를 앞으로 당긴다. (아직 다 안 읽은 데이터가 있으므로)
		if (FreeSize() < _bufferSize)
		{
			::memcpy(&_buffer[0], &_buffer[_readPos], dataSize);
			_readPos = 0;
			_writePos = dataSize;
		}
		
	}
}

bool RecvBuffer::OnRead(int32 numOfBytes)
{
	if (numOfBytes > DataSize())
	{
		return false;
	}

	_readPos += numOfBytes;

	return true;
}

bool RecvBuffer::OnWrite(int32 numOfBytes)
{
	if (numOfBytes > FreeSize())
	{
		return false;
	}

	_writePos += numOfBytes;
	return true;
}
