#include "pch.h"
#include "DBConnectionPool.h"

DBConnectionPool::DBConnectionPool()
{
}

DBConnectionPool::~DBConnectionPool()
{
	Clear();
}

bool DBConnectionPool::Connect(int32 connectionCount, const WCHAR* connString)
{
	WRITE_LOCK;

	if (::SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &_env) != SQL_SUCCESS)
	{
		cout << "SQLAllocHandle ENV FAILED" << endl;
		return false;
	}

	if (::SQLSetEnvAttr(_env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0) != SQL_SUCCESS)
	{
		cout << "SQLSetEnvAttr ENV FAILED" << endl;
		return false;
	}

	for (int32 i = 0; i < connectionCount; i++)
	{
		DBConnection* connection = xnew<DBConnection>();
		if (connection->Connect(_env, connString) == false)
		{
			return false;
		}

		_connections.push_back(connection);
	}

	cout << "DBConnectionPool Connected. Count : " << connectionCount << endl;
	return true;
}

void DBConnectionPool::Clear()
{
	WRITE_LOCK;

	if (_env != SQL_NULL_HANDLE)
	{
		// Á¤¸®
		::SQLFreeHandle(SQL_HANDLE_ENV, _env);
		_env = SQL_NULL_HANDLE;
	}

	for (DBConnection* connection : _connections)
	{
		xdelete(connection);
	}

	_connections.clear();
}

DBConnection* DBConnectionPool::Pop()
{
	WRITE_LOCK;

	if (_connections.empty())
	{
		return nullptr;
	}

	DBConnection* connection = _connections.back();
	_connections.pop_back();
	return connection;
}

// ¹Ý³³
void DBConnectionPool::Push(DBConnection* connection)
{
	WRITE_LOCK;
	_connections.push_back(connection);
}
