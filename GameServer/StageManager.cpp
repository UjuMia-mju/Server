#include "pch.h"
#include "StageManager.h"
#include "DBConnectionPool.h"
#include "DBBind.h"

bool StageManager::Init(const WCHAR* langCode)
{
	_stageCache.clear();

	DBConnectionGuard conn(GDBConnectionPool);

	DBBind<1, 8> dbBind(conn, 
		L"SELECT m.id, m.chapter, m.stage, m.is_boss, m.difficulty, m.estimated_time, COALESCE(ml.name, 'Unknown'), COALESCE(ml.description, '') " 
		L"FROM maps m "
	    L"LEFT JOIN map_locales ml ON m.id = ml.map_id AND ml.lan_code = ?"
	);

    int32_t chapter = 0, stage = 0, difficulty = 0, estimatedTime = 0, stage_id = 0;
    bool isBoss = false;
    WCHAR name[100] = { 0, };
    WCHAR description[255] = { 0, };

    dbBind.BindParam(0, langCode);
	dbBind.BindCol(0, stage_id);
    dbBind.BindCol(1, chapter);
    dbBind.BindCol(2, stage);
    dbBind.BindCol(3, isBoss);
    dbBind.BindCol(4, difficulty);
    dbBind.BindCol(5, estimatedTime);
    dbBind.BindCol(6, name);
    dbBind.BindCol(7, description);

    if (dbBind.Execute())
    {
        while (dbBind.Fetch())
        {
            StageInfo info;
			info.map_id = stage_id;
            info.chapter = chapter;
            info.stage = stage;
            info.isBoss = isBoss;
            info.difficulty = difficulty;
            info.estimated_clearTime = estimatedTime;

            // WCHAR -> std::string 변환
            int lenName = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
            if (lenName > 0) {
                std::string strName(lenName - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, name, -1, &strName[0], lenName, nullptr, nullptr);
                info.mapName = strName;
            }

            int lenDesc = WideCharToMultiByte(CP_UTF8, 0, description, -1, nullptr, 0, nullptr, nullptr);
            if (lenDesc > 0) {
                std::string strDesc(lenDesc - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, description, -1, &strDesc[0], lenDesc, nullptr, nullptr);
                info.mapDescription = strDesc;
            }

            _stageCache[stage_id] = info;
			cout << "Loaded Stage: " << info.mapName << " (Chapter " << chapter << ", Stage " << stage << ")" << endl;
        }
    }

    return true;
}

optional<StageInfo> StageManager::GetStageInfo(int mapId, int chapter, int stage)
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

bool StageManager::GetMyStageClearInfo(int32 userId, OUT xvector<StageClearInfo>& clears)
{
    clears.clear();

    DBConnectionGuard conn(GDBConnectionPool);

    DBBind<1, 3> dbBind(conn,
        L"SELECT map_id, star, clear_time "
        L"FROM user_map_clears "
        L"WHERE user_id = ?"
    );

    int32 stageId = 0;
    int32 star = 0;
    int32 clearTime = 0;

    dbBind.BindParam(0, userId);
    dbBind.BindCol(0, stageId);
    dbBind.BindCol(1, star);
    dbBind.BindCol(2, clearTime);

    if (dbBind.Execute())
    {
        while (dbBind.Fetch())
        {
            StageClearInfo info;
            info.stageId = stageId;
            info.star = star;
            info.clearTime = clearTime;
            clears.push_back(info);
        }
        return true;
    }

    return false;
}

void StageManager::FillStageListPacket(Protocol::S_STAGE_INFO& pkt) const
{
    for (const auto& pair : _stageCache)
    {
        const StageInfo& info = pair.second;
        Protocol::StageInfo* stageInfo = pkt.add_stages();
        stageInfo->set_map_id(info.map_id);
        stageInfo->set_chapter(info.chapter);
        stageInfo->set_stage(info.stage);
        stageInfo->set_difficulty(info.difficulty);
        stageInfo->set_estimated_clear_time(info.estimated_clearTime);
        stageInfo->set_isbossstage(info.isBoss);
        stageInfo->set_stage_name(info.mapName);
        stageInfo->set_description(info.mapDescription);
	}
}

StageInfo StageManager::GetStageInfoById(int mapId) const
{
	auto it = _stageCache.find(mapId);

    if (it != _stageCache.end())
    {
        return it->second;
    }
    else
    {
        cout << "error: stage cahche miss for mapId " << mapId << endl;
        return StageInfo(); // 기본값 반환
	}

}

void StageManager::ChangeProtocolToStageInfo(const Protocol::StageInfo& proto, OUT StageInfo& stageInfo) const
{
	stageInfo.map_id = proto.map_id();
	stageInfo.chapter = proto.chapter();
	stageInfo.stage = proto.stage();
    
	stageInfo.difficulty = proto.difficulty();
	stageInfo.estimated_clearTime = proto.estimated_clear_time();
	stageInfo.isBoss = proto.isbossstage();
	stageInfo.estimated_clearTime = proto.estimated_clear_time();
	
    stageInfo.mapName = proto.stage_name();
	stageInfo.mapDescription = proto.description();
}

void StageManager::ChangeStageInfoToProtocol(const StageInfo& stageInfo, OUT Protocol::StageInfo& proto) const
{
    proto.set_map_id(stageInfo.map_id);
    proto.set_chapter(stageInfo.chapter);
    proto.set_stage(stageInfo.stage);
    
    proto.set_difficulty(stageInfo.difficulty);
    proto.set_estimated_clear_time(stageInfo.estimated_clearTime);
    proto.set_isbossstage(stageInfo.isBoss);
    proto.set_stage_name(stageInfo.mapName);
	proto.set_description(stageInfo.mapDescription);
}

bool StageManager::UpdateStageClearInfo(int32 userId, int mapId, int star, int clearTime)
{
    DBConnectionGuard conn(GDBConnectionPool);

    // MySQL 전용 Upsert 구문 (INSERT ON DUPLICATE KEY UPDATE)
    DBBind<4, 0> dbBind(conn,
        L"INSERT INTO user_map_clears (user_id, map_id, star, clear_time) "
        L"VALUES (?, ?, ?, ?) AS new_val "
        L"ON DUPLICATE KEY UPDATE "
        L"    clear_time = CASE "
        L"        WHEN new_val.star > user_map_clears.star THEN new_val.clear_time "
        L"        WHEN new_val.star = user_map_clears.star AND new_val.clear_time < user_map_clears.clear_time THEN new_val.clear_time "
        L"        ELSE user_map_clears.clear_time "
        L"    END, "
        L"    star = GREATEST(user_map_clears.star, new_val.star)"
    );

    dbBind.BindParam(0, userId);
    dbBind.BindParam(1, mapId);
    dbBind.BindParam(2, star);
    dbBind.BindParam(3, clearTime);
	
    return dbBind.Execute();
}
