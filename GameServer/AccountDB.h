#pragma once
#include "pch.h"
#include "DBConnectionPool.h"
#include "DBBind.h"

/*----------------------
	AccountDB
	: 계정 정보 관리
------------------------*/
struct OwnedSkinInfo;

class AccountDB
{
public: 
	static bool ValidateAccount(const string& email, const string& password);
	static bool GetPlayerInfo(const string& email, const std::string& password, OUT int32& playerId, OUT string& playerName, OUT int32& playerTag);
	static bool GetUserProfileInfo(int32 dbUserId, OUT int32& outCoin, OUT int32& outGem, OUT vector<OwnedSkinInfo>& outOwnedSkins);
};
