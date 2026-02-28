#include "pch.h"
#include "AccountDB.h"

/*----------------------
	AccountDB
	: 계정 정보 관리
------------------------*/

bool AccountDB::ValidateAccount(const string& email, const string& password)
{
	DBConnection* dbConn = GDBConnectionPool->Pop();
	if (dbConn == nullptr)
		return false;

	// UTF-8 -> UTF-16 변환
	wstring wUserId(email.begin(), email.end());
	wstring wPassword(password.begin(), password.end());

	// 파라미터 2개, 결과 컬럼 1개
	DBBind<2, 1> dbBind(*dbConn, L"SELECT COUNT(*) FROM users WHERE email = ? AND password = ?");

	int32 count = 0;
	dbBind.BindParam(0, wUserId.c_str());
	dbBind.BindParam(1, wPassword.c_str());
	dbBind.BindCol(0, count);

	bool result = false;
	if (dbBind.Execute())
	{
		if (dbConn->Fetch())
		{
			result = (count > 0);
		}
	}

	dbConn->Unbind();
	GDBConnectionPool->Push(dbConn);

	return result;
}

bool AccountDB::GetPlayerInfo(const string& email, OUT int32& playerId, OUT wstring& playerName, OUT int32& playerTag)
{
	DBConnection* dbConn = GDBConnectionPool->Pop();
	if (dbConn == nullptr)
		return false;

	std::wstring wEmail(email.begin(), email.end());

	// email로 사용자 정보 조회
	DBBind<1, 2> dbBind(*dbConn, L"SELECT id, username FROM users WHERE email = ?");

	WCHAR nameBuffer[51] = { 0 };
	dbBind.BindParam(0, wEmail.c_str());
	dbBind.BindCol(0, playerId);
	dbBind.BindCol(1, nameBuffer, sizeof(nameBuffer));

	bool result = false;
	if (dbBind.Execute())
	{
		if (dbConn->Fetch())
		{
			playerName = nameBuffer;
			result = true;
		}
	}

	dbConn->Unbind();
	GDBConnectionPool->Push(dbConn);

	return result;
}