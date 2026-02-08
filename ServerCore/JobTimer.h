#pragma once

struct JobData
{
	JobData(weak_ptr<JobQueue> owner, JobRef job) : owner(owner), job(job)
	{
	}
	weak_ptr<JobQueue>	owner;
	JobRef				job;
};

struct TimerItem
{
	bool operator<(const TimerItem& other) const
	{
		return executeTick > other.executeTick;
	}

	// 실행되어야 하는 틱
	uint64			executeTick = 0;
	JobData*		jobData = nullptr;
};


class JobTimer
{
public:
	void Reserve(uint64 tickAfter, weak_ptr<JobQueue> owner, JobRef job);
	void Distribute(uint64 now);
	void Clear();

private:
	USE_LOCK;
	xpriority_queue<TimerItem>	_timerItems;
	Atomic<bool>				_distributing = false; // 다른 스레드가 실행중인지 여부.

};

