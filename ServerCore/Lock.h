#pragma once
#include "Types.h"

/*--------------------------------------------------------
	Reader Writer Spin Lock
----------------------------------------------------------*/

/*
[WWWWWWWWW][WWWWWWWW][RRRRRRRR][RRRRRRRRR]

W: Writer Waiting Bit
R: Reader Count Bits (Read Flag)
*/


// W -> W (can)
// W -> R (can)
// R -> W (cant)
class Lock
{
	enum : uint32
	{
		ACQUIRE_TIMEOUT_TICK = 10000,
		MAX_SPIN_COUNT = 5000,
		WRITE_THREAD_MASK = 0xFFFF'0000, // 정확하게 상위 16비트를 뽑아오기 위한 마스크
		READ_COUNT_MASK = 0x0000'FFFF, // 정확하게 하위 16비트를 뽑아오기 위한 마스크
		EMPTY_FLAG = 0x0000'0000,
		// F = 1을 의미함. 이걸 이용해서 비트 연산을 수행
	};
public:
	void WriteLock(const char* name);
	void WriteUnlock(const char* name);

	void ReadLock(const char* name);
	void ReadUnlock(const char* name);
private:
	Atomic<uint32> _lockFlag = EMPTY_FLAG;
	uint16 _writeCount = 0;
};

/*--------------------------------------------------------
	LockGuards
-----------------------------------------------------------*/

class ReadLockGuard
{
public:
	ReadLockGuard(Lock& lock, const char* name) : _lock(lock), _name(name)
	{
		_lock.ReadLock(name);
	}
	~ReadLockGuard()
	{
		_lock.ReadUnlock(_name);
	}
private:
	Lock& _lock;
	const char* _name;
};

class WriteLockGuard
{
public:
	WriteLockGuard(Lock& lock, const char* name) : _lock(lock), _name(name)
	{
		_lock.WriteLock(name);
	}
	~WriteLockGuard()
	{
		_lock.WriteUnlock(_name);
	}
private:
	Lock& _lock;
	const char* _name;
};