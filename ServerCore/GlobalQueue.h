#pragma once

// 전역에서 사용하는 Job Queue
class GlobalQueue
{
public:
	GlobalQueue();
	~GlobalQueue();

	void			Push(JobQueueRef jobQueue);
	JobQueueRef		Pop();
private:
	LockQueue<JobQueueRef>	_jobQueues;
};

