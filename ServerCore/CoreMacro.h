#pragma once

#define OUT

/*----------------------
 Lock
-----------------------*/

#define USE_MANY_LOCKS(count)	Lock _locks[count]
#define USE_LOCK				USE_MANY_LOCKS(1)
#define READ_LOCK_IDX(idx)		ReadLockGuard readLockGuard_##idx(_locks[idx], typeid(this).name());
// #을 2개를 붙이면 idx의 값이 들어가서 예를들어 idx가 1이면 readLockGuard_1이 된다. (전처리 단계)
#define READ_LOCK				READ_LOCK_IDX(0)
#define WRITE_LOCK_IDX(idx)		WriteLockGuard writeLockGuard_##idx(_locks[idx], typeid(this).name());
#define WRITE_LOCK				WRITE_LOCK_IDX(0)

///*----------------------
// Memory
//-----------------------*/
//#ifdef _DEBUG
//#define yalloc(size)			PoolAllocator::Alloc(size)
//#define yrelease(ptr) 			PoolAllocator::Release(ptr)
//#else
//#define yalloc(size)			PoolAllocator::Alloc(size)
//#define yrelease(ptr) 			PoolAllocator::Release(ptr)
//#endif


/*----------------------
	Crash Macro
-----------------------*/

#define CRASH(cause)					\
{										\
	uint32* crash = nullptr;			\
	__analysis_assume(crash != nullptr);\
	*crash = 0xDEADBEEF;				\
}

#define ASSERT_CRASH(expr)				\
{										\
	if (!(expr))						\
	{									\
		CRASH(#expr);					\
	}									\
}