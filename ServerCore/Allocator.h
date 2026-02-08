#pragma once

/*------------------------
	Base Allocator
-------------------------*/

class BaseAllocator
{
public:
	static void* Alloc(int32 size);
	static void Release(void* p);
private:
};


/*------------------------
	Stomp Allocator
-------------------------*/

class StompAllocator : public BaseAllocator
{
	enum 
	{
		PAGE_SIZE = 0x1000
	};
public: 
	static void* Alloc(int32 size);
	static void Release(void* p);
};

/*------------------------
	Pool Allocator
-------------------------*/

class PoolAllocator : public BaseAllocator
{
public:
	static void*	Alloc(int32 size);
	static void		Release(void* p);
};


/*------------------------
	STL Allocator
-------------------------*/

template<typename T>
class StlAllocator
{
public:
	using value_type = T;

	StlAllocator() {};

	template<typename Other>
	StlAllocator(const StlAllocator<Other>&) {}

	T* allocate(size_t count)
	{
		const int32 size = static_cast<int32>(count * sizeof(T));
		return static_cast<T*>(PoolAllocator::Alloc(size));
	}

	void deallocate(T* ptr, size_t count)
	{
		PoolAllocator::Release(ptr);
	}
private:
};