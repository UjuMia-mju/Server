#include "pch.h"
#include "GachaManager.h"
#include "DBConnectionPool.h"
#include "DBBind.h"
#include <random>
#include <ctime>
#include "Player.h"

// TIMESTAMP_STRUCT를 int64(Unix Timestamp)로 변환하는 유틸리티 함수
static int64 ConvertDbTimestampToUnix(const TIMESTAMP_STRUCT& ts)
{
	// DB에 기본값(NULL 등) 처리가 되어 연도가 비정상적일 경우 0 처리
	if (ts.year == 0) return 0;

	struct tm timeInfo = {};
	timeInfo.tm_year = ts.year - 1900; // 1900년부터 시작
	timeInfo.tm_mon = ts.month - 1;   // 0~11로 처리
	timeInfo.tm_mday = ts.day;
	timeInfo.tm_hour = ts.hour;
	timeInfo.tm_min = ts.minute;
	timeInfo.tm_sec = ts.second;
	timeInfo.tm_isdst = -1;

	time_t epochTime = mktime(&timeInfo);
	if (epochTime == -1)
	{
		return 0; // 변환 실패 여부 체크
	}

	return static_cast<int64>(epochTime);
}

bool GachaManager::Init(const WCHAR* langCode)
{
	// 초기화 전에 기존 캐시 클리어 (중요)
	_poolCache.clear();
	_skinCache.clear();

	DBConnectionGuard conn(GDBConnectionPool);

	// 1. gacha_pools 에서 정보 가져오기
	DBBind<0, 8> dbPoolBind(conn, L"SELECT id, name, pool_type, cost_coin, cost_gem, max_pull, start_at, end_at FROM gacha_pools WHERE is_active = 1");
	
	int32 pId = 0, cCoin = 0, cGem = 0, maxPull = 0;
	WCHAR name[100] = { 0, };
	WCHAR poolType[20] = { 0, };
	TIMESTAMP_STRUCT startAt = {};
	TIMESTAMP_STRUCT endAt = {};

	// SELECT 순서에 맞게 인덱스(0~7) 바인딩
	dbPoolBind.BindCol(0, pId);
	dbPoolBind.BindCol(1, name);
	dbPoolBind.BindCol(2, poolType);
	dbPoolBind.BindCol(3, cCoin);
	dbPoolBind.BindCol(4, cGem);
	dbPoolBind.BindCol(5, maxPull);
	dbPoolBind.BindCol(6, startAt);
	dbPoolBind.BindCol(7, endAt);

	if (dbPoolBind.Execute())
	{
		while (dbPoolBind.Fetch())
		{
			GachaPoolInfo info;
			info.poolId = pId;
			info.costCoin = cCoin;
			info.costGem = cGem;
			info.maxPull = maxPull;

			// 1) name: WCHAR -> std::string 변환
			int lenName = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
			std::string strName(lenName - 1, 0);
			WideCharToMultiByte(CP_UTF8, 0, name, -1, &strName[0], lenName, nullptr, nullptr);
			info.name = strName;

			// 2) poolType: WCHAR -> std::string 변환
			int lenType = WideCharToMultiByte(CP_UTF8, 0, poolType, -1, nullptr, 0, nullptr, nullptr);
			std::string strType(lenType - 1, 0);
			WideCharToMultiByte(CP_UTF8, 0, poolType, -1, &strType[0], lenType, nullptr, nullptr);
			info.poolType = strType;

			// 3) 시간: TIMESTAMP_STRUCT -> int64 (초 단위) 변환
			info.startAt = ConvertDbTimestampToUnix(startAt);
			info.endAt = ConvertDbTimestampToUnix(endAt);

			_poolCache[pId] = info;
		}
	}

	// 2. gacha_pool_items 에서 아이템 목록(Weight) 로드 및 캐시에 넣기
	DBBind<0, 3> dbItemBind(conn, L"SELECT pool_id, skin_id, weight FROM gacha_pool_skins");
	int32 poolId, skinId, weight;
	dbItemBind.BindCol(0, poolId);
	dbItemBind.BindCol(1, skinId);
	dbItemBind.BindCol(2, weight);

	if (dbItemBind.Execute())
	{
		while (dbItemBind.Fetch())
		{
			if (_poolCache.find(poolId) != _poolCache.end())
			{
				_poolCache[poolId].items.push_back({ skinId, weight });
			}
		}
	}

	// 3. skins 테이블에서 스킨 메타 데이터 로드 (선택 사항)
	DBBind<1, 6> dbSkinBind(conn,
		L"SELECT s.id, COALESCE(l.skin_name, 'Unknown'), COALESCE(l.skin_des, ''), s.skin_type, s.skin_rank, s.is_limited " 
		L"FROM skins s "
		L"LEFT JOIN skin_locales l ON s.id = l.skin_id AND l.lan_code = ?");

	int32 sId = 0, sRarity = 0, sType = 0;
	WCHAR sName[100] = { 0, };
	WCHAR sDesc[255] = { 0, }; 
	bool isLimited = false;

	dbSkinBind.BindParam(0, langCode);
	dbSkinBind.BindCol(0, sId);
	dbSkinBind.BindCol(1, sName);
	dbSkinBind.BindCol(2, sDesc);
	dbSkinBind.BindCol(3, sType);
	dbSkinBind.BindCol(4, sRarity);
	dbSkinBind.BindCol(5, isLimited);
	

	if (dbSkinBind.Execute())
	{
		while (dbSkinBind.Fetch())
		{
			SkinMetaData meta;
			meta.skinId = sId;
			meta.rarity = sRarity;

			// 이름 WCHAR -> UTF-8 String 변환
			int lenName = WideCharToMultiByte(CP_UTF8, 0, sName, -1, nullptr, 0, nullptr, nullptr);
			if (lenName > 0) {
				std::string strName(lenName - 1, 0);
				WideCharToMultiByte(CP_UTF8, 0, sName, -1, &strName[0], lenName, nullptr, nullptr);
				meta.name = strName;
			}

			// 설명 WCHAR -> UTF-8 String 변환
			int lenDesc = WideCharToMultiByte(CP_UTF8, 0, sDesc, -1, nullptr, 0, nullptr, nullptr);
			if (lenDesc > 0) {
				std::string strDesc(lenDesc - 1, 0);
				WideCharToMultiByte(CP_UTF8, 0, sDesc, -1, &strDesc[0], lenDesc, nullptr, nullptr);
				meta.description = strDesc;
			}

			meta.skinType = sType;
			meta.isLimited = isLimited;

			// 메모리에 저장
			_skinCache[sId] = meta;
			cout << "Loaded Skin Meta - ID: " << sId << ", Name: " << meta.name << ", Rarity: " << sRarity << endl;
		}
	}

	return true;
}

bool GachaManager::ExecuteGacha(PlayerInfoRef playerInfo, int32 poolId, OUT int32& outObtainedSkinId)
{
	outObtainedSkinId = 0;
	cout << "start gacha !!!!!" << endl;
	// 1. 메모리에 캐시된 풀 정보 찾기
	auto poolIt = _poolCache.find(poolId);
	if (poolIt == _poolCache.end())
	{
		cout << "fail to-" << endl;
		return false; // 존재하지 않는 풀
	}
		
	const GachaPoolInfo& poolInfo = poolIt->second;

	// 2. 메모리 상에서 유저 재화 1차 검사 (DB 가기 전 빠른 차단)
	if (playerInfo->GetCoin() < poolInfo.costCoin || playerInfo->GetGem() < poolInfo.costGem)
	{
		cout << "fail to-2" << endl;
		return false;
	}
		
	// 3. 인메모리 필터링 - 유저가 보유하지 않은 스킨만 남기고 총 가중치 계산
	xvector<GachaItem> availablePool;
	int32 totalWeight = 0;

	for (const GachaItem& item : poolInfo.items)
	{
		// Player 객체에 HasSkin() 같은 보유 스킨 체크 함수가 있다고 가정합니다
		if (playerInfo->HasSkin(item.skinId) == false)
		{
			cout << "Available Skin - ID: " << item.skinId << ", Weight: " << item.weight << endl;
			availablePool.push_back(item);
			totalWeight += item.weight;
		}
	}

	if (availablePool.empty() || totalWeight <= 0)
	{
		cout << "fail to-3" << endl;
		return false; // 더 이상 뽑을 수 있는 스킨이 없음
	}
		
	// 4. 가중치 기반 난수 뽑기
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int32> dist(1, totalWeight);
	int32 randomValue = dist(gen);

	int32 currentWeight = 0;

	for (const GachaItem& item : availablePool)
	{
		currentWeight += item.weight;
		if (randomValue <= currentWeight)
		{
			outObtainedSkinId = item.skinId;
			break;
		}
	}

	if (outObtainedSkinId == 0)
	{
		cout << "fail to-4" << endl;
		return false;
	}

	// 5. DB 저장 프로시저 호출 (원자성. DB 안에서 다 처리)
	bool isSuccess = PerformDBTransaction(playerInfo->GetDbUserId(), outObtainedSkinId, poolInfo.costCoin, poolInfo.costGem);

	if (isSuccess)
	{
		// 6. DB 트랜잭션이 완벽히 성공했으므로, C++ 메모리의 Player 객체에도 반영해 줌
		playerInfo->SetCoin(playerInfo->GetCoin() - poolInfo.costCoin);
		playerInfo->SetGem(playerInfo->GetGem() - poolInfo.costGem);
		playerInfo->AddSkin(outObtainedSkinId); // 메모리에 보유 처리
		return true;
	}

	cout << "fail procedure" << endl;
	// 재화가 부족하거나 등 기타 여러 이유로 DB 에러
	return false;
}

bool GachaManager::PerformDBTransaction(int32 userId, int32 skinId, int32 costCoin, int32 costGem)
{
	DBConnectionGuard conn(GDBConnectionPool);

	// 저장 프로시저 파라미터 (IN 4개, OUT 1개)
	DBBind<4, 1> dbBind(conn, L"{CALL sp_ExecuteGacha(?, ?, ?, ?)}");

	int32 result = -1;

	dbBind.BindParam(0, userId);
	dbBind.BindParam(1, skinId);
	dbBind.BindParam(2, costCoin);
	dbBind.BindParam(3, costGem);

	dbBind.BindCol(0, result);

	if (dbBind.Execute())
	{
		// SELECT 결과를 꺼내옵니다.
		if (dbBind.Fetch())
		{
			std::cout << "[DB_SP LOG] sp_ExecuteGacha result = " << result << std::endl;
			if (result == 0)
			{
				return true; // 드디어 완벽한 성공!
			}
		}
		else
		{
			std::cout << "[DB_SP ERROR] Fetch() fail!" << std::endl;
		}
	}
	else
	{
		std::cout << "[DB_SP ERROR] Execute fail" << std::endl;
	}

	cout << "result : " << result << endl;
	return false;
}

bool GachaManager::GetMySkins(PlayerInfoRef playerInfo, OUT xvector<SkinMetaData>& outSkins)
{
	outSkins.clear();

	auto userId = playerInfo->GetDbUserId();
	DBConnectionGuard conn(GDBConnectionPool);

	DBBind<2, 5> dbBind(conn,
		L"SELECT s.id, COALESCE(l.skin_name, 'Unknown'), COALESCE(l.skin_des, ''), s.skin_type, s.skin_rank "
		L"FROM user_owned_skins us "
		L"JOIN skins s ON us.skin_id = s.id "
		L"LEFT JOIN skin_locales l ON s.id = l.skin_id AND l.lan_code = ? "
		L"WHERE us.user_id = ?");

	int32 sId = 0, sRarity = 0, sType = 0;
	WCHAR sName[100] = { 0, };
	WCHAR sDesc[255] = { 0, };

	dbBind.BindParam(0, L"ko"); // 언어 코드 (예: "en", "ko")
	dbBind.BindParam(1, userId);

	dbBind.BindCol(0, sId);
	dbBind.BindCol(1, sName);
	dbBind.BindCol(2, sDesc);
	dbBind.BindCol(3, sType);
	dbBind.BindCol(4, sRarity);

	if (dbBind.Execute())
	{
		while (dbBind.Fetch())
		{
			SkinMetaData meta;
			meta.skinId = sId;
			meta.rarity = sRarity;
			meta.skinType = sType;
			// 이름 WCHAR -> UTF-8 String 변환
			int lenName = WideCharToMultiByte(CP_UTF8, 0, sName, -1, nullptr, 0, nullptr, nullptr);
			if (lenName > 0) {
				std::string strName(lenName - 1, 0);
				WideCharToMultiByte(CP_UTF8, 0, sName, -1, &strName[0], lenName, nullptr, nullptr);
				meta.name = strName;
			}
			// 설명 WCHAR -> UTF-8 String 변환
			int lenDesc = WideCharToMultiByte(CP_UTF8, 0, sDesc, -1, nullptr, 0, nullptr, nullptr);
			if (lenDesc > 0) {
				std::string strDesc(lenDesc - 1, 0);
				WideCharToMultiByte(CP_UTF8, 0, sDesc, -1, &strDesc[0], lenDesc, nullptr, nullptr);
				meta.description = strDesc;
			}
			outSkins.push_back(meta);
		}
		return true;
	}

	return false;
}

void GachaManager::FillGachaPoolListPacket(Protocol::S_GACHA_POOL_LIST& pkt) const
{
	for (const auto& [poolId, poolInfo] : _poolCache)
	{
		auto poolEntry = pkt.add_pools();
		poolEntry->set_pool_id(poolInfo.poolId);
		poolEntry->set_pool_name(poolInfo.name);
		poolEntry->set_cost_gem(poolInfo.costGem);
		poolEntry->set_cost_coin(poolInfo.costCoin);
		poolEntry->set_max_pull(poolInfo.maxPull);
		poolEntry->set_is_active(poolInfo.IsActive());
		poolEntry->set_start_at(poolInfo.startAt);
		poolEntry->set_end_at(poolInfo.endAt);
		for (const auto& skinId : poolInfo.items)
		{
			auto skinMeta = GGACHA.GetSkinMetaData(skinId.skinId);
			if (skinMeta)
			{
				auto itemEntry = poolEntry->add_skins();
				itemEntry->set_skin_id(skinMeta->skinId);
				itemEntry->set_skin_name(skinMeta->name);
				itemEntry->set_skin_des(skinMeta->description);
				itemEntry->set_rarity(skinMeta->rarity);
			}
			else
			{
				cout << "Failed to retrieve skin metadata for skin ID: " << skinId.skinId << endl;
			}
		}
	}
}

void GachaManager::FillSkinListPacket(Protocol::S_SKIN_LIST& pkt) const
{
	for (const auto& [skinId, meta] : _skinCache)
	{
		auto skinEntry = pkt.add_skins();
		skinEntry->set_skin_id(meta.skinId);
		skinEntry->set_skin_name(meta.name);
		skinEntry->set_skin_des(meta.description);
		skinEntry->set_rarity(meta.rarity);
	}
}
