#pragma once
#include "DBConnection.h"

template<int32 C>
struct FullBits { enum { value = (1 << (C - 1)) | FullBits<C - 1>::value }; };

template<>
struct FullBits<1> { enum { value = 1 }; };

template<>
struct FullBits<0> { enum { value = 0 }; };



template<int32 ParamCount, int32 ColumnCount>
class DBBind
{
public:
	DBBind(DBConnection& dbConnection, const WCHAR* query)
		: _dbConnection(dbConnection), _query(query)
	{
		::memset(_paramIndex, 0, sizeof(_paramIndex));
		::memset(_columnIndex, 0, sizeof(_columnIndex));
		_paramBindFlag = 0;
		_columnBindFlag = 0;
		dbConnection.Unbind();
	}

	bool Validate()
	{
		return _paramBindFlag == FullBits<ParamCount>::value
			&& _columnBindFlag == FullBits<ColumnCount>::value;
	}

	bool Execute()
	{
		ASSERT_CRASH(Validate());
		return _dbConnection.Execute(_query);
	}

	bool Fetch()
	{
		return _dbConnection.Fetch();
	}

public:
	template<typename T>
	void BindParam(int32 index, T& value)
	{
		_dbConnection.BindParam(index + 1, &value, &_paramIndex[index]);
		_paramBindFlag |= (1ULL << index);
	}

	void BindParam(int32 index, const WCHAR* str)
	{
		_dbConnection.BindParam(index + 1, str, &_paramIndex[index]);
		_paramBindFlag |= (1ULL << index);
	}

	template<typename T, int32 N>
	void BindParam(int32 index, T(&value)[N])
	{
		_dbConnection.BindParam(index + 1, (const BYTE*)value, size32(T) * N, &_paramIndex[index]);
		_paramBindFlag |= (1ULL << index);
	}

	template<typename T>
	void BindParam(int32 index, T* value, int32 N)
	{
		_dbConnection.BindParam(index + 1, (const BYTE*)value, size32(T) * N, &_paramIndex[index]);
		_paramBindFlag |= (1ULL << index);
	}

	template<typename T>
	void BindCol(int32 idx, T& value)
	{
		_dbConnection.BindCol(idx + 1, &value, &_columnIndex[idx]);
		_columnBindFlag |= (1ULL << idx);
	}

	template<int32 N>
	void BindCol(int32 idx, WCHAR(&value)[N])
	{
		_dbConnection.BindCol(idx + 1, value, N, &_columnIndex[idx]);
		_columnBindFlag |= (1ULL << idx);
	}

	void BindCol(int32 idx, WCHAR* value, int32 len)
	{
		_dbConnection.BindCol(idx + 1, value, len, &_columnIndex[idx]);
		_columnBindFlag |= (1ULL << idx);
	}

	template<typename T, int32 N>
	void BindCol(int32 idx, T(&value)[N])
	{
		_dbConnection.BindCol(idx + 1, value, size32(T) * N, &_columnIndex[idx]);
		_columnBindFlag |= (1ULL << idx);
	}

protected:
	DBConnection&	_dbConnection;
	const WCHAR*	_query;
	SQLLEN			_paramIndex[ParamCount > 0 ? ParamCount : 1];
	SQLLEN			_columnIndex[ColumnCount > 0 ? ColumnCount : 1];
	uint64			_paramBindFlag = 0;
	uint64			_columnBindFlag = 0;	
};

