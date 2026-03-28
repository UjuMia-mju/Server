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
			info.stage_id = stage_id;
            info.chapter = chapter;
            info.stage = stage;
            info.isBoss = isBoss;
            info.difficulty = difficulty;
            info.estimated_clearTime = estimatedTime;

            // WCHAR -> std::string º¯È¯
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

bool StageManager::GetMyStageClearInfo(int32 userId, OUT xvector<StageClearInfo>& clears)
{
    clears.clear();

    DBConnectionGuard conn(GDBConnectionPool);

    DBBind<1, 3> dbBind(conn,
        L"SELECT map_id, star, clear_time "
        L"FROM user_map_clears "
        L"WHERE user_id = ?"
    );

    int stageId = 0;
    int star = 0;
    int clearTime = 0;

    dbBind.BindParam(0, userId);
    dbBind.BindCol(0, stageId);
    dbBind.BindCol(1, star);
    dbBind.BindCol(2, clearTime);

    if (dbBind.Execute())
    {
        while (dbBind.Fetch())
        {
            StageClearInfo info;
            info.stage_id = stageId;
            info.star = star;
            info.clear_time = clearTime;
            clears.push_back(info);
        }
        return true;
    }

    return false;
}
