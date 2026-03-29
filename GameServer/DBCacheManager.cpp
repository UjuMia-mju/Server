#include "pch.h"
#include "DBCacheManager.h"
#include "GachaManager.h"
#include "StageManager.h"

void DBCacheManager::InitAsync()
{
	WRITE_LOCK;

	_tasks.clear();

	// »Ì±â Á¤º¸ Ä³½Ì
	_tasks.emplace_back(packaged_task<void()>(
		[this]() {
			CacheGachaData();
		}
	));

	// ½ºÅ×ÀÌÁö Á¤º¸ Ä³½Ì
	_tasks.emplace_back(packaged_task<void()>(
		[this]() {
			CacheStageData();
		}
	));
	
	_futures.clear();

	for (size_t i = 0; i < _tasks.size(); ++i)
	{
		_futures.push_back(_tasks[i].get_future());
		thread(std::move(_tasks[i])).detach();
	}
}

void DBCacheManager::WaitAll()
{
	for (auto& fut : _futures)
	{
		fut.get();
	}
}

void DBCacheManager::CacheGachaData()
{
	GachaManager::GetInstance().Init(L"ko");
}

void DBCacheManager::CacheStageData()
{
	StageManager::GetInstance().Init(L"ko");
}