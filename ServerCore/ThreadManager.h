#pragma once

#include <thread>
#include <functional>

class ThreadManager
{
public:
	ThreadManager();
	~ThreadManager();

	// 실행
	void Launch(function<void(void)> callback);
	// 끝
	void Join();
	
	static void InitTLS();
	static void DestroyTLS();

	// 글로벌 큐에 있는 일을 하겠다.
	static void DoGlobalQueueWork();
	static void DistributeReservedJobs();
private:
	Mutex				_lock;
	vector<thread>		_threads;
};

