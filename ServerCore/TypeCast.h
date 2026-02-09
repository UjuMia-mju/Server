#pragma once
#include "Types.h"

#pragma region TypeList // 타입 리스트 정의

template<typename... T>
struct TypeList;

template<typename T, typename U>
struct TypeList<T, U>
{
	using Head = T;
	using Tail = U;
};

template<typename T, typename... U>
struct TypeList<T, U...>
{
	using Head = T;
	using Tail = TypeList<U...>;
};

#pragma endregion

#pragma region Length // 타입 리스트의 길이 계산
template<typename T>
struct Length;

template<>
struct Length<TypeList<>>
{
	enum { value = 0 };
};

template<typename T, typename... U>
struct Length<TypeList<T, U...>>
{
	enum { value = 1 + Length<TypeList<U...>>::value };
};
#pragma endregion

#pragma region TypeAt // 타입 리스트에서 인덱스에 해당하는 타입 추출
template<typename TL, int32 index>
struct TypeAt;

template<typename Head, typename... Tail>
struct TypeAt<TypeList<Head, Tail...>, 0>
{
	using Result = Head;
};

template<typename Head, typename... Tail, int32 index>
struct TypeAt<TypeList<Head, Tail...>, index>
{
	using Result = typename TypeAt<TypeList<Tail...>, index - 1>::Result;
};

#pragma endregion

#pragma region IndexOf // 타입 리스트에서 특정 타입의 인덱스 찾기
template<typename TL, typename T>
struct IndexOf;

template<typename... Tail, typename T>
struct IndexOf<TypeList<T, Tail...>, T>
{
	enum { value = 0 };
};

template<typename T>
struct IndexOf<TypeList<>, T>
{
	enum { value = -1 };
};

template<typename Head, typename... Tail, typename T>
struct IndexOf<TypeList<Head, Tail...>, T>
{
private:
	enum { temp = IndexOf<TypeList<Tail...>, T>::value };
public:
	enum { value = (temp == -1 ? -1 : 1 + temp) };
};

#pragma endregion

#pragma region Conversion
template<typename From, typename To>
class Conversion
{
private:
	using Small = __int8;
	using Big = __int32;

	static Small Test(const To&) { return 0; };
	static Big Test(...) { return 0; };
	static From MakeFrom() { return 0; };
public:
	enum { exists = sizeof(Test(MakeFrom())) == sizeof(Small) };
};

#pragma endregion

#pragma region TypeCast
template<int32 V>
struct Int2Type
{
	enum { value = V};
};

template<typename TL>
class TypeConversion
{
public:
	enum
	{
		length = Length<TL>::value
	};

	TypeConversion()
	{

	}

	// 2중 for문을 template 재귀를 이용해서 구현.
	template<int32 i, int32 j>
	static void MakeTable(Int2Type<i>, Int2Type<j>)
	{
		using FromType = typename TypeAt<TL, i>::Result;
		using ToType = typename TypeAt<TL, j>::Result;

		if (Conversion<const FromType*, const ToType*>::exists)
		{
			s_convert[i][j] = true;
		}
		else
		{
			s_convert[i][j] = false;
		}

		// 다음 j
		MakeTable(Int2Type<i>(), Int2Type<j + 1>());
	}

	// 다음 i
	template<int32 i>
	static void MakeTable(Int2Type<i>, Int2Type<length>)
	{
		MakeTable(Int2Type<i + 1>(), Int2Type<0>());
	}

	template<int j>
	static void MakeTable(Int2Type<length>, Int2Type<j>)
	{
		// 종료
	}


	static inline bool CanConvert(int32 from, int32 to)
	{
		static TypeConversion conversion;
		return s_convert[from][to];
	}
public:
	static bool s_convert[length][length];
};

template<typename TL>
bool TypeConversion<TL>::s_convert[length][length];

// 여기부터는 살짝 이해가 안됨.
// 포인터 방식
template<typename To, typename From>
To TypeCast(From* ptr)
{
	if (ptr == nullptr) { return nullptr; }

	using TL = typename From::TL;

	if (TypeConversion<TL>::CanConvert(ptr->_typeId, IndexOf<TL, remove_pointer_t<To>>::value))
	{
		return static_cast<To>(ptr);
	}

	return nullptr;
}

//Shared_ptr
template<typename To, typename From>
shared_ptr<To> TypeCast(shared_ptr<From> ptr)
{
	if (ptr == nullptr) { return nullptr; }

	using TL = typename From::TL;

	if (TypeConversion<TL>::CanConvert(ptr->_typeId, IndexOf<TL, remove_pointer_t<To>>::value))
	{
		return static_pointer_cast<To>(ptr);
	}

	return nullptr;
}

// 타입 캐스트가 되는지 확인
template<typename To, typename From>
bool CanCast(From* ptr)
{
	if (ptr == nullptr) { return false; }

	using TL = typename From::TL;
	return TypeConversion<TL>::CanConvert(ptr->_typeId, IndexOf<TL, remove_pointer_t<To>>::value);
}

template<typename To, typename From>
bool CanCast(shared_ptr<From> ptr)
{
	if (ptr == nullptr) { return false; }

	using TL = typename From::TL;
	return TypeConversion<TL>::CanConvert(ptr->_typeId, IndexOf<TL, remove_pointer_t<To>>::value);
}

#pragma endregion

#define DECLARE_TL			using TL = TL; int32 _typeId = 0;
#define INIT_TL(Type)		_typeId = IndexOf<TL, Type>::value;

//----------- EXAMPLE USAGE -----------
/*
using TL = TypeList<class A, class B, class C>;

class A
{
public:
	A()
	{
		INIT_TL(A);
	}
	virtual ~A() {}

	DECLARE_TL;
};

class B : public A
{
public:
	B() { INIT_TL(B); }
};

class C : public A
{
public:
	C() { INIT_TL(C); }
};

int main()
{
	{
		A* a = new A();
		bool canCast = CanCast<B*>(a); // false
		B* b = TypeCast<B*>(a); // nullptr

		delete a;
	}
}
*/