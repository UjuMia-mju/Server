#pragma once
#include "DBConnection.h"

class DBConnectionPool
{
public:
	DBConnectionPool();
	~DBConnectionPool();

	bool			Connect(int32 maxConnections ,const WCHAR* connString);
	void			Clear();

	DBConnection*	Pop(uint32 timeoutMs = 5000);
	void			Push(DBConnection* connection);
	void			HandleKeepAlive();
private:
	USE_LOCK;
	SQLHENV					_env = SQL_NULL_HANDLE;
	xvector<DBConnection*>	_connections;
	Atomic<int32>			_usedCount = 0; // 사용 중인 Connection 개수
	int32 					_maxConnections = 0;    // 최대 개수

	bool					_initialized = false;
};

// Connection 자동 반납 헬퍼 클래스
class DBConnectionGuard
{
public:
	DBConnectionGuard(DBConnectionPool* pool)
		: _pool(pool), _connection(pool->Pop())
	{
	}

	~DBConnectionGuard()
	{
		if (_connection)
			_pool->Push(_connection);
	}

	DBConnection* Get() { return _connection; }
	DBConnection* operator->() { return _connection; }
	explicit operator bool() const { return _connection != nullptr; }

	operator DBConnection& ()
	{
		ASSERT_CRASH(_connection != nullptr);
		return *_connection;
	}

	// 복사 방지
	DBConnectionGuard(const DBConnectionGuard&) = delete;
	DBConnectionGuard& operator=(const DBConnectionGuard&) = delete;

private:
	DBConnectionPool* _pool;
	DBConnection* _connection;
};