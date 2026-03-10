#include "pch.h"
#include "AccountDB.h"

/*----------------------
	AccountDB
	: 계정 정보 관리
------------------------*/

bool AccountDB::ValidateAccount(const string& email, const string& password)
{
	DBConnectionGuard guard(GDBConnectionPool);
	if (!guard)
	{
		return false;
	}

	// UTF-8 -> UTF-16 변환
	wstring wUserId(email.begin(), email.end());
	wstring wPassword(password.begin(), password.end());

	// 파라미터 2개, 결과 컬럼 1개
	DBBind<2, 1> dbBind(guard, L"SELECT COUNT(*) FROM users WHERE email = ? AND password = ?");

	int32 count = 0;
	dbBind.BindParam(0, wUserId.c_str());
	dbBind.BindParam(1, wPassword.c_str());
	dbBind.BindCol(0, count);

	if (dbBind.Execute())
	{
		if (guard->Fetch())
		{
			return count > 0;
		}
	}

	return false;
}

bool AccountDB::GetPlayerInfo(const string& email, OUT int32& playerId, OUT wstring& playerName, OUT int32& playerTag)
{
	DBConnection* dbConn = GDBConnectionPool->Pop();
	if (dbConn == nullptr)
		return false;

	std::wstring wEmail(email.begin(), email.end());

	// email로 사용자 정보 조회
	DBBind<1, 3> dbBind(*dbConn, L"SELECT id, username, tag FROM users WHERE email = ?");

	WCHAR nameBuffer[51] = { 0 };
	dbBind.BindParam(0, wEmail.c_str());
	dbBind.BindCol(0, playerId);
	dbBind.BindCol(1, nameBuffer, sizeof(nameBuffer));
	dbBind.BindCol(2, playerTag);

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

bool AccountDB::GetPlayerClearInfo(int32 playerId, OUT vector<StageClearData>& outClearData)
{
	DBConnection* dbConn = GDBConnectionPool->Pop();
	if (dbConn == nullptr)
		return false;

	// 플레이어 ID로 클리어 정보 조회 (스테이지, 레벨 순으로 정렬)
	DBBind<1, 4> dbBind(*dbConn, L"SELECT stage, level, star, UNIX_TIMESTAMP(clear_time) FROM user_map_clears WHERE user_id = ? ORDER BY stage, level");

	int32 stage = 0;
	int32 level = 0;
	int32 star = 0;
	int64 clearTime = 0;

	dbBind.BindParam(0, playerId);
	dbBind.BindCol(0, stage);
	dbBind.BindCol(1, level);
	dbBind.BindCol(2, star);
	dbBind.BindCol(3, clearTime);

	bool result = false;
	if (dbBind.Execute())
	{
		while (dbConn->Fetch())
		{
			StageClearData data;
			data.stage = stage;
			data.level = level;
			data.star = star;
			data.clearTime = clearTime;
			outClearData.push_back(data);
		}
		result = true;
	}

	dbConn->Unbind();
	GDBConnectionPool->Push(dbConn);
}
