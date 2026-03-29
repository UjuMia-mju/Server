#pragma once
#include<future>
#include<vector>
#include<functional>
#include<mutex>

class DBCacheManager
{
public:
	static DBCacheManager& GetInstance()
	{
		static DBCacheManager instance;
		return instance;
	}
	void InitAsync();// DB에서 정보를 비동기로 캐싱함.
	void WaitAll();// 캐싱 대기
private:
	void CacheGachaData();
	void CacheStageData();

	vector<packaged_task<void()>> _tasks;
	vector<future<void>> _futures;
	USE_LOCK;
};