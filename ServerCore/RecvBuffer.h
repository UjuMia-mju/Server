#pragma once

/*----------------------
	RecvBuffer
	역할: 수신 버퍼 관리
------------------------*/


//[r][][w] [][][] [][][] [][][]
// buffer size = [][][]
// capacity = 저거의 10개
class RecvBuffer
{
	enum { BUFFER_COUNT = 10 };
public:
	RecvBuffer(int32 bufferSize);
	~RecvBuffer();

	void				Clean();
	bool				OnRead(int32 numOfBytes);
	bool				OnWrite(int32 numOfBytes);

	BYTE*				ReadPos() { return &_buffer[_readPos]; }
	BYTE*				WritePos() { return &_buffer[_writePos]; }
	int32				DataSize() { return _writePos - _readPos; }
	int32				FreeSize() { return _capacity - _writePos; }

private:
	int32				_capacity = 0;
	int32				_bufferSize = 0;
	int32				_readPos = 0;
	int32				_writePos = 0;
	xvector<BYTE>		_buffer;
};

