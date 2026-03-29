#include "pch.h"
#include "DBConnectionPool.h"

DBConnectionPool::DBConnectionPool()
{
}

DBConnectionPool::~DBConnectionPool()
{
	Clear();
}

bool DBConnectionPool::Connect(int32 maxConnections, const WCHAR* connString)
{
	WRITE_LOCK;

	// 이미 초기화된 경우
	if (_initialized)
	{
		cout << "DBConnectionPool already initialized!" << endl;
		return false;
	}

	// ODBC 환경 핸들 생성
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

	_maxConnections = maxConnections;
	// 최소 개수만큼 미리 생성
	_connections.reserve(_maxConnections);

	for (int32 i = 0; i < _maxConnections; i++)
	{
		DBConnection* connection = xnew<DBConnection>();
		if (connection->Connect(_env, connString) == false)
		{
			return false;
		}
		_connections.push_back(connection);
	}
	_initialized = true;
	cout << "DBConnectionPool Connected. Count : " << maxConnections << endl;
	return true;
}

void DBConnectionPool::Clear()
{
	WRITE_LOCK;

	if (_env != SQL_NULL_HANDLE)
	{
		// 정리
		::SQLFreeHandle(SQL_HANDLE_ENV, _env);
		_env = SQL_NULL_HANDLE;
	}

	for (DBConnection* connection : _connections)
	{
		xdelete(connection);
	}

	_connections.clear();
	_initialized = false;
}

DBConnection* DBConnectionPool::Pop(uint32 timeoutMs)
{
	const uint64 startTime = ::GetTickCount64();

	while (true)
	{
		// 락을 걸고 꺼낼 수 있는지 확인
		{
			WRITE_LOCK;
			if (_connections.empty() == false)
			{
				DBConnection* connection = _connections.back();
				_connections.pop_back();
				return connection;
			}
		}

		// 타임아웃 시간이 지났으면 포기
		if (::GetTickCount64() > startTime + timeoutMs)
		{
			return nullptr;
		}
			

		// 스레드가 빈 루프를 돌면서 CPU를 잡아먹지 않도록 양보
		this_thread::yield();
	}
}

// 반납
void DBConnectionPool::Push(DBConnection* connection)
{
	WRITE_LOCK;
	_connections.push_back(connection);
}