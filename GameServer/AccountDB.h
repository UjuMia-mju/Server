#pragma once
#include "pch.h"
#include "DBConnectionPool.h"
#include "DBBind.h"

/*----------------------
	AccountDB
	: 계정 정보 관리
------------------------*/

class AccountDB
{
public: 
	static bool ValidateAccount(const string& email, const string& password);
	static bool GetPlayerInfo(const string& userId, OUT int32& playerId, OUT wstring& playerName);
};
