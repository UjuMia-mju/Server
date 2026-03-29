#pragma once
#include "Protocol.pb.h"

// 가중치 기반 스킨 선택 (중복 제거 방식)
struct GachaItem
{
	int32 skinId;
	int32 weight;
};

struct GachaPoolInfo
{
	int32 poolId;
	string name;
	string poolType;
	int32 maxPull; // 1회 최대 뽑기 가능 횟수 (예: 10회 뽑기)
	int32 costCoin;
	int32 costGem;
	int64 startAt;
	int64 endAt;
	xvector<GachaItem> items; // 이 풀에서 나오는 아이템 목록

	bool IsActive() const
	{
		int64 now = std::time(nullptr);
		return now >= startAt && now <= endAt;
	}
};

struct SkinMetaData
{
	int32 skinId;
	
	std::string name;
	std::string description;

	int32 skinType;
	int32 rarity;

	bool isLimited; // 기간 한정 여부
};


class GachaManager
{
public:
	bool Init(const WCHAR* langCode);
	static GachaManager& GetInstance()
	{
		static GachaManager instance; // 메모리에 한 번만 안전하게 생성됨
		return instance;
	}

	// 클라이언트에서 가챠 요청 시 호출
	bool ExecuteGacha(PlayerInfoRef playerInfo, int32 poolId, OUT int32& outObtainedSkinId);
	bool PerformDBTransaction(int32 userId, int32 skinId, int32 costCoin, int32 costGem);
	bool GetMySkins(PlayerInfoRef playerInfo, OUT xvector<SkinMetaData>& outSkins);


	const SkinMetaData* GetSkinMetaData(int32 skinId) const
	{
		auto it = _skinCache.find(skinId);
		if (it != _skinCache.end())
			return &(it->second);
		return nullptr;
	}

	xmap<int32, GachaPoolInfo> GetAllGachaPools() const
	{
		return _poolCache;
	}

	xmap<int32, SkinMetaData> GetAllSkinMetaData() const
	{
		return _skinCache;
	}

private:
	xmap<int32, GachaPoolInfo> _poolCache;
	xmap<int32, SkinMetaData> _skinCache;
};

#define GGACHA GachaManager::GetInstance()