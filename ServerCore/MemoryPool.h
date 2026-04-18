#pragma once

enum
{
	SLIST_ALIGNMENT = 16
};

/*-----------------
	MemoryHeader
------------------*/

// 16바이트 정렬을 하겠다는 의미.
DECLSPEC_ALIGN(SLIST_ALIGNMENT)
struct MemoryHeader : public SLIST_ENTRY
{
	// [MemoryHeader][Data]
	MemoryHeader(int32 size) : allocSize(size) { }

	// 헤더 붙이기 / 떼어내기
	static void* AttachHeader(MemoryHeader* header, int32 size)
	{
		new(header)MemoryHeader(size); // placement new
		return reinterpret_cast<void*>(++header);
	}

	static MemoryHeader* DetachHeader(void* ptr)
	{
		MemoryHeader* header = reinterpret_cast<MemoryHeader*>(ptr) - 1;
		return header;
	}

	int32 allocSize;
	// TODO : 필요한 추가 정보
};

/*-----------------
	MemoryPool
------------------*/

DECLSPEC_ALIGN(SLIST_ALIGNMENT)
class MemoryPool
{
public:
	MemoryPool(int32 allocSize);
	~MemoryPool();

	void			Push(MemoryHeader* ptr);
	MemoryHeader* Pop();
private:
	SLIST_HEADER	_header;
	int32			_allocSize = 0;
	atomic<int32>	_useCount = 0; // 현재 사용 중인 개수
	atomic<int32>	_reserveCount = 0; // 풀에 대기 중인 개수
};


