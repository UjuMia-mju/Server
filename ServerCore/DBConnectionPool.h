#pragma once
#include "DBConnection.h"

class DBConnectionPool
{
public:
	DBConnectionPool();
	~DBConnectionPool();

	bool			Connect(int32 connectionCount, const WCHAR* connString);
	void			Clear();

	DBConnection*	Pop();
	void			Push(DBConnection* connection);
private:
	USE_LOCK;
	SQLHENV					_env = SQL_NULL_HANDLE;
	xvector<DBConnection*>	_connections;
};

