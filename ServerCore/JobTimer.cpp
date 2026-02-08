#include "pch.h"
#include "JobTimer.h"
#include "JobQueue.h"

void JobTimer::Reserve(uint64 tickAfter, weak_ptr<JobQueue> owner, JobRef job)
{
	const uint64 executeTick = ::GetTickCount64() + tickAfter;
	JobData* jobData = ObjectPool<JobData>::Pop(owner, job);

	WRITE_LOCK;
	_timerItems.push(TimerItem{ executeTick, jobData });
}

void JobTimer::Distribute(uint64 now)
{
	// 한번에 한 스레드만 통과 (참고로 이전값 튀어나옴, true라면 이미 누군가 실행중이니까 return)
	if (_distributing.exchange(true) == true)
	{
		return;
	}
	
	xvector<TimerItem> items;
	
	{
		WRITE_LOCK;

		while (_timerItems.empty() == false)
		{
			const TimerItem& timerItem = _timerItems.top();
			if (now < timerItem.executeTick)
			{
				break;
			}

			items.push_back(timerItem);
			_timerItems.pop();
		}
	}

	for (TimerItem& item : items)
	{
		if (JobQueueRef owner = item.jobData->owner.lock())
		{
			owner->Push(item.jobData->job);
		}
		ObjectPool<JobData>::Push(item.jobData);
	}

	// 끝났으면 풀어준다.
	_distributing.store(false);
}

void JobTimer::Clear()
{
	WRITE_LOCK;

	while (_timerItems.empty() == false)
	{
		const TimerItem& timerItem = _timerItems.top();
		ObjectPool<JobData>::Push(timerItem.jobData);
		_timerItems.pop();
	}
}
