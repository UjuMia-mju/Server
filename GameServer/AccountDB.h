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
struct OwnedSkinInfo;

class AccountDB
{
public: 
	static bool ValidateAccount(const string& email, const string& password);
	static bool GetPlayerInfo(const string& email, const std::string& password, OUT int32& playerId, OUT string& playerName, OUT int32& playerTag);
	static bool GetPlayerClearInfo(int32 playerId, OUT vector<StageClearData>& outClearData);
	static bool GetUserProfileInfo(int32 dbUserId, OUT int32& outCoin, OUT int32& outGem, OUT vector<OwnedSkinInfo>& outOwnedSkins);
};
