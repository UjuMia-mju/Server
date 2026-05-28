#pragma once
#include <sql.h>
#include <sqlext.h>


enum
{
	WVARCHAR_SIZE = 4000,
	BINARY_MAX = 8000
};

class DBConnection
{
public:
	bool			Connect(SQLHENV henv, const WCHAR* connString);
	void			Clear();

	bool			Execute(const WCHAR* query);
	bool			Fetch(); //  데이터가 있으면 긁어오겠다.

	int32			GetRowCount();
	void			Unbind();

public:
	bool			BindParam(int32 paramIndex, bool* value, SQLLEN* index);
	bool			BindParam(int32 paramIndex, float* value, SQLLEN* index);
	bool			BindParam(int32 paramIndex, double* value, SQLLEN* index);
	bool			BindParam(int32 paramIndex, int8* value, SQLLEN* index);
	bool			BindParam(int32 paramIndex, int16* value, SQLLEN* index);
	bool			BindParam(int32 paramIndex, int32* value, SQLLEN* index);
	bool			BindParam(int32 paramIndex, int64* value, SQLLEN* index);
	bool			BindParam(int32 paramIndex, TIMESTAMP_STRUCT* value, SQLLEN* index);
	bool			BindParam(int32 paramIndex, const WCHAR* str, SQLLEN* index);
	bool			BindParam(int32 paramIndex, const BYTE* bin,int32 size, SQLLEN* index);

	bool			BindCol(int32 columnIndex, bool* value, SQLLEN* index);
	bool			BindCol(int32 columnIndex, float* value, SQLLEN* index);
	bool			BindCol(int32 columnIndex, double* value, SQLLEN* index);
	bool			BindCol(int32 columnIndex, int8* value, SQLLEN* index);
	bool			BindCol(int32 columnIndex, int16* value, SQLLEN* index);
	bool			BindCol(int32 columnIndex, int32* value, SQLLEN* index);
	bool			BindCol(int32 columnIndex, int64* value, SQLLEN* index);
	bool			BindCol(int32 columnIndex, TIMESTAMP_STRUCT* value, SQLLEN* index);
	bool			BindCol(int32 columnIndex, WCHAR* str, int32 size, SQLLEN* index);
	bool			BindCol(int32 columnIndex, BYTE* bin, int32 size, SQLLEN* index);

private:
	// 실행할때 넘겨주는 인자들
	bool			BindParam(SQLUSMALLINT paramIndex, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* index);
	// 받아올 때 사용하는 인자
	bool			BindCol(SQLUSMALLINT columnIndex, SQLSMALLINT cType, SQLLEN len, SQLPOINTER value, SQLLEN* index);
	void			HandleError(SQLRETURN ret);
private:
	// 핸들인데 데이터베이스 커넥션
	SQLHDBC			_connection = SQL_NULL_HANDLE;
	// 스테이트먼트, 어떤 상태를 관리
	SQLHSTMT		_statement = SQL_NULL_HANDLE;
};

