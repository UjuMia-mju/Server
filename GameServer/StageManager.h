#pragma once
#include "Protocol.pb.h"
#include <optional>

struct StageInfo
{
	int stage_id;
	int chapter;
	int stage;

	int difficulty;
	int estimated_clearTime; // 예상 클리어 시간 (초 단위)

	bool isBoss; // 보스 스테이지 여부

	string mapName;
	string mapDescription;
};

struct StageClearInfo
{
	int stageId;
	int star; // 클리어 별 개수 (예: 0~3)
	int clearTime; // 실제 클리어 시간 (초 단위)
};

class StageManager
{
public:
	bool Init(const WCHAR* langCode);
	static StageManager& GetInstance()
	{
		static StageManager instance;
		return instance;
	}

	optional<StageInfo> GetStageInfo(int mapId, int chapter, int stage) const
	{
		auto it = _stageCache.find(mapId);
		if (it != _stageCache.end())
		{
			if (it->second.chapter == chapter && it->second.stage == stage)
			{
				return it->second;
			}
			else
			{
				cout << "error: stage cahche miss" << endl;
				return nullopt;
			}
		}
		return nullopt;
	}

	unordered_map<int32_t, StageInfo> GetAllStages() const
	{
		return _stageCache;
	}

	bool GetMyStageClearInfo(int32 userId, OUT xvector<StageClearInfo>& clears);

private:
	unordered_map<int32_t, StageInfo> _stageCache;
};

#define GStageManager StageManager::GetInstance()

