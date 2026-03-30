#include "pch.h"
#include "AccountDB.h"
#include "Player.h"
#include <ctime> 


static int64 ConvertDbTimestampToUnix(const TIMESTAMP_STRUCT& ts)
{
	// 기본값이거나 초기화되지 않은 경우
	if (ts.year == 0) return 0;

	struct tm timeInfo = {};
	timeInfo.tm_year = ts.year - 1900;
	timeInfo.tm_mon = ts.month - 1;
	timeInfo.tm_mday = ts.day;
	timeInfo.tm_hour = ts.hour;
	timeInfo.tm_min = ts.minute;
	timeInfo.tm_sec = ts.second;
	timeInfo.tm_isdst = -1; // 시스템 기본 썸머타임 정책 따름

	time_t epochTime = mktime(&timeInfo);
	if (epochTime == -1) return 0;

	return static_cast<int64>(epochTime);
}

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

bool AccountDB::GetPlayerInfo(const string& email,const std::string& password, OUT int32& playerId, OUT string& playerName, OUT int32& playerTag)
{
	DBConnectionGuard guard(GDBConnectionPool);
	if (!guard)
	{
		return false;
	}

	// email로 사용자 정보 조회
	DBBind<2, 3> dbBind(guard, L"SELECT id, username, tag FROM users WHERE email = ? AND password = ?");

	int32 emailLen = ::MultiByteToWideChar(CP_UTF8, 0, email.c_str(), -1, nullptr, 0);
	int32 pwLen = ::MultiByteToWideChar(CP_UTF8, 0, password.c_str(), -1, nullptr, 0);
	if (emailLen <= 0 || pwLen <= 0)
	{
		return false;
	}

	// 2. 파라미터를 바인딩
	std::wstring wEmail(emailLen - 1, L'\0');
	std::wstring wPassword(pwLen - 1, L'\0');

	::MultiByteToWideChar(CP_UTF8, 0, email.c_str(), -1, wEmail.data(), emailLen);
	::MultiByteToWideChar(CP_UTF8, 0, password.c_str(), -1, wPassword.data(), pwLen);

	// 핵심: 반드시 const WCHAR* 오버로드 사용
	dbBind.BindParam(0, wEmail.c_str());
	dbBind.BindParam(1, wPassword.c_str());

	int32 idCol = 0;
	WCHAR nameCol[100] = { 0, };
	int32 tagCol = 0;

	dbBind.BindCol(0, idCol);
	dbBind.BindCol(1, nameCol);
	dbBind.BindCol(2, tagCol);

	// 3. 쿼리 실행 (조건이 틀리면 Fetch가 false를 뱉고 실패 처리됨)
	if (dbBind.Execute() && dbBind.Fetch())
	{
		playerId = idCol;
		playerTag = tagCol;

		int len = ::WideCharToMultiByte(CP_UTF8, 0, nameCol, -1, nullptr, 0, nullptr, nullptr);
		if (len > 0)
		{
			playerName = std::string(len - 1, 0);
			::WideCharToMultiByte(CP_UTF8, 0, nameCol, -1, &playerName[0], len, nullptr, nullptr);
		}

		return true; // 로그인 성공 + 정보 추출 완료!
	}

	return false;
}

bool AccountDB::GetUserProfileInfo(int32 dbUserId, OUT int32& outCoin, OUT int32& outGem, OUT vector<OwnedSkinInfo>& outOwnedSkins)
{
	// 초기화
	outCoin = 0;
	outGem = 0;
	outOwnedSkins.clear();

	DBConnectionGuard conn(GDBConnectionPool);
	if (!conn)
	{
		return false;
	}
	
	// 1. 유저의 재화 가져오기 (테이블: user_goods 또는 users)
	DBBind<1, 2> dbGoodsBind(conn, L"SELECT coin, gem FROM user_goods WHERE user_id = ?");
	dbGoodsBind.BindParam(0, dbUserId);

	dbGoodsBind.BindCol(0, outCoin);
	dbGoodsBind.BindCol(1, outGem);

	if (dbGoodsBind.Execute())
	{
		dbGoodsBind.Fetch();
	}
	else
	{
		// 쿼리 자체가 실패한 경우
		return false;
	}

	// 2. 유저가 보유한 스킨 목록 및 상태 상세 정보 가져오기 
	DBBind<1, 3> dbSkinBind(conn,
		L"SELECT skin_id, skin_get_at, is_equipped FROM user_owned_skins WHERE user_id = ?"
	);
	dbSkinBind.BindParam(0, dbUserId);

	int32 sId = 0;
	TIMESTAMP_STRUCT sGetAt = {};
	bool sEquipped = false;

	dbSkinBind.BindCol(0, sId);
	dbSkinBind.BindCol(1, sGetAt);
	dbSkinBind.BindCol(2, sEquipped);

	if (dbSkinBind.Execute())
	{
		// 유저가 가진 스킨이 10개라면 Fetch() 가 10번 True를 반환합니다.
		while (dbSkinBind.Fetch())
		{
			// 타임스탬프 구조체를 우리가 쓰기 편한 int64(Unix)로 변환
			int64 unixGetAt = ConvertDbTimestampToUnix(sGetAt);
			// 완성된 스킨 구조체 하나를 리스트에 밀어 넣습니다.
			outOwnedSkins.push_back({ sId, unixGetAt, sEquipped });
		}
	}
	else
	{
		return false; // 스킨 쿼리 실패
	}

	return true;
}

bool AccountDB::GetUserGoods(int32 dbUserId, OUT int32& outCoin, OUT int32& outGem)
{
	// 초기화
	outCoin = 0;
	outGem = 0;

	DBConnectionGuard conn(GDBConnectionPool);
	if (!conn)
	{
		return false;
	}

	// 1. 유저의 재화 가져오기 (테이블: user_goods 또는 users)
	DBBind<1, 2> dbGoodsBind(conn, L"SELECT coin, gem FROM user_goods WHERE user_id = ?");
	dbGoodsBind.BindParam(0, dbUserId);

	dbGoodsBind.BindCol(0, outCoin);
	dbGoodsBind.BindCol(1, outGem);

	if (dbGoodsBind.Execute())
	{
		dbGoodsBind.Fetch();
	}
	else
	{
		// 쿼리 자체가 실패한 경우
		return false;
	}

	return true;
}
