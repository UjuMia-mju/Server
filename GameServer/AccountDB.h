#pragma once
#include "pch.h"
#include "DBConnectionPool.h"
#include "DBBind.h"

/*----------------------
	StageClearData
	: 스테이지 클리어 정보
------------------------*/
struct StageClearData
{
	int32 stage = 0;
	int32 level = 0;
	int32 star = 0;
	int64 clearTime = 0; // Unix timestamp
};

/*----------------------
	AccountDB
	: 계정 정보 관리
------------------------*/

class AccountDB
{
public: 
	static bool ValidateAccount(const string& email, const string& password);
	static bool GetPlayerInfo(const string& userId, OUT int32& playerId, OUT wstring& playerName, OUT int32& playerTag);
	static bool GetPlayerClearInfo(int32 playerId, OUT vector<StageClearData>& outClearData);
};
