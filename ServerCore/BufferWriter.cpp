#include "pch.h"
#include "BufferWriter.h"

/*-----------------
	BufferWriter
	역할: 버퍼를 쓰는 클래스
-------------------*/

BufferWriter::BufferWriter()
{
}

BufferWriter::BufferWriter(BYTE* buffer, uint32 size, uint32 pos)
	: _buffer(buffer), _size(size), _pos(pos)
{
}

BufferWriter::~BufferWriter()
{
}

bool BufferWriter::Write(void* src, uint32 len)
{
	if (FreeSize() < len)
	{
		return false;
	}

	::memcpy(&_buffer[_pos], src, len);
	_pos += len;
	return true;
}
