#include "pch.h"
#include "Lock.h"
#include "CoreTLS.h"
#include "DeadLockProfiler.h"

void Lock::WriteLock(const char* name)
{
#if _DEBUG
	GDeadLockProfiler->PushLock(name);
#endif

	// 동일한 스레드가 소유하고 있다면 무조건 성공
	const uint32 lockThreadId = (_lockFlag.load() & WRITE_THREAD_MASK) >> 16;
	if (LthreadId == lockThreadId)
	{
		_writeCount++;
		return;
	}

	// 아무도 소유 및 공유하고 있지 않을 때, 경합해서 소유권을 얻는다. (EMPTY_FLAG)
	// 소유: 누군가가 WriteLock을 획득한 상태
	// 공유: 누군가가 ReadLock을 획득한 상태

	const uint32 beginTick = ::GetTickCount64();
	
	// 상위 16비트에 쓰레드 아이디를 넣음
	const uint32 desired = ((LthreadId << 16) & WRITE_THREAD_MASK); 
	
	while (true)
	{
		for (uint32 spinCount = 0; spinCount < MAX_SPIN_COUNT; spinCount++)
		{
			uint32 expected = EMPTY_FLAG;
			if (_lockFlag.compare_exchange_strong(OUT expected, desired))
			{
				_writeCount++;
				return;
			}
		}

		if (::GetTickCount64() - beginTick > ACQUIRE_TIMEOUT_TICK)
		{
			// 타임아웃
			CRASH("LockTimeOut");
		}

		this_thread::yield();
	}
}

void Lock::WriteUnlock(const char* name)
{
#if _DEBUG
	GDeadLockProfiler->PopLock(name);
#endif

	// ReadLock 다 풀기 전에는 WriteUnlock 불가
	if ((_lockFlag.load() & READ_COUNT_MASK) != 0)
	{
		CRASH("ReadLockExist_WriteUnlockFail");
	}

	const int32 lockCount = --_writeCount;
	if (lockCount == 0)
	{
		_lockFlag.store(EMPTY_FLAG);
	}
}

void Lock::ReadLock(const char* name)
{
#if _DEBUG
	GDeadLockProfiler->PushLock(name);
#endif

	// 동일한 스레드가 쓰기 잠금을 소유하고 있다면 무조건 성공.
	const uint32 lockThreadId = (_lockFlag.load() & WRITE_THREAD_MASK) >> 16;
	if (LthreadId == lockThreadId)
	{
		_lockFlag.fetch_add(1);
		return;
	}

	// 아무도 소유하고 있지 않을 때 경합해서 공유권을 얻는다.
	const uint32 beginTick = ::GetTickCount64();

	while (true)
	{
		for (uint32 spinCount = 0; spinCount < MAX_SPIN_COUNT; spinCount++)
		{
			uint32 expected = (_lockFlag.load() & READ_COUNT_MASK);
			if (_lockFlag.compare_exchange_strong(OUT expected, expected + 1))
			{
				return;
			}

			if (::GetTickCount64() - beginTick > ACQUIRE_TIMEOUT_TICK)
			{
				// 타임아웃
				CRASH("LockTimeOut");
			}

			this_thread::yield();
		}	
	}
}

void Lock::ReadUnlock(const char* name)
{
#if _DEBUG
	GDeadLockProfiler->PopLock(name);
#endif
	// 정보 .fetch_sub(1) 의 반환값은 언락되기 전의 값이므로, 0이 된다면 이전이 01이었다는 뜻
	if ((_lockFlag.fetch_sub(1) & READ_COUNT_MASK) == 0)
	{
		CRASH("ReadUnlock_Underflow");
	}
}
