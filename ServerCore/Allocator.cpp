#include "pch.h"
#include "Allocator.h"
#include "Memory.h"


/*------------------------
	Base Allocator
-------------------------*/


void* BaseAllocator::Alloc(int32 size)
{
	return malloc(size);
}

void BaseAllocator::Release(void* ptr)
{
	free(ptr);
}


/*------------------------
	Stomp Allocator
-------------------------*/

void* StompAllocator::Alloc(int32 size)
{
	//[[  ]  page-size      ] 이런식으로 할당함. -> 근데 이러면 빈 공간이 많이 할당이 되고, 오버플로우 문제 생김.
	//[      page-size  [  ]] 이렇게 뒤에 할당하면 오버플로우 문제는 할당 안됨.
	const int64 pageCount = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	const int64 dataOffset = pageCount * PAGE_SIZE - size;

	void* baseAddress = ::VirtualAlloc(NULL, pageCount * PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	return static_cast<void*>(static_cast<int8*>(baseAddress) + dataOffset);
}

void StompAllocator::Release(void* ptr)
{
	const int64 address = reinterpret_cast<int64>(ptr);
	const int64 baseAddress = address - (address % PAGE_SIZE);
	::VirtualFree(reinterpret_cast<void*>(baseAddress), 0, MEM_RELEASE);
}

/*------------------------
	Pool Allocator
-------------------------*/

void* PoolAllocator::Alloc(int32 size)
{
	return GMemory->Allocate(size);
}

void PoolAllocator::Release(void* ptr)
{
	GMemory->Release(ptr);
}