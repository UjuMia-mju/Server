#include "pch.h"
#include "DeadLockProfiler.h"

void DeadLockProfiler::PushLock(const char* lockName)
{
	LockGuard guard(_mutex);

	// 아이디를 찾거나 발급한다.
	int32 lockId = 0;

	auto findId = _nameToId.find(lockName);
	if (findId == _nameToId.end())
	{
		lockId = static_cast<int32>(_nameToId.size());
		_nameToId[lockName] = lockId;
		_idToName[lockId] = lockName;
	}
	else
	{
		lockId = findId->second;
	}

	// 잡고 있는 락이 있었다면
	if (LLockStack.empty() == false)
	{
		// 기존에 발견되지 않은 케이스라면 데드락 여부 다시 확인.
		const int32 prevId = LLockStack.top();
		if (lockId != prevId)
		{
			set<int32>& history = _lockHistory[prevId];
			if (history.find(lockId) == history.end())
			{
				history.insert(lockId);
				CheckCycle();
			}
		}
	}

	LLockStack.push(lockId);
}

void DeadLockProfiler::PopLock(const char* lockName)
{
	LockGuard guard(_mutex);

	if (LLockStack.empty())
	{
		CRASH("PopLockFail_StackEmpty");
	}

	int32 lockId = _nameToId[lockName];

	if (LLockStack.top() != lockId)
	{
		CRASH("PopLockFail_LockMismatch");
	}

	LLockStack.pop();
}


void DeadLockProfiler::CheckCycle()
{
	const int32 lockCount = static_cast<int32>(_nameToId.size());
	_discoverOrder = vector<int32>(lockCount, -1);
	_discoverCount = 0;
	_finished = vector<bool>(lockCount, false);
	_parent = vector<int32>(lockCount, -1);

	for (int32 lockId = 0; lockId < lockCount; lockId++)
	{
		Dfs(lockId);
	}

	// 연산이 끝나면 정리
	_discoverOrder.clear();
	_finished.clear();
	_parent.clear();
}

void DeadLockProfiler::Dfs(int32 here)
{
	// 
	if (_discoverOrder[here] != -1)
	{
		return;
	}

	_discoverOrder[here] = _discoverCount++;

	auto findIt = _lockHistory.find(here);
	if (findIt == _lockHistory.end())
	{
		_finished[here] = true;
		return;
	}

	set<int32>& nextSet = findIt->second;
	for (int32 there : nextSet)
	{
		// 아직 방문한 적이 없다면 방문
		if (_discoverOrder[there] == -1)
		{
			_parent[there] = here;
			Dfs(there);
			continue;
		}

		// here가 there보다 먼저 발견되었다면, there는 here의 자손노드 (순방향)
		if (_discoverOrder[here] < _discoverOrder[there])
		{
			continue;
		}

		// 순방향아니고, Dfs(there)가 아직 종료하지 않았다면, there은 here의 부모노드이다. (역방향)
		if (_finished[there] == false)
		{
			printf("%s -> %s\n", _idToName[here], _idToName[there]);

			int32 now = here;
			while (true)
			{
				printf("%s -> %s\n", _idToName[now], _idToName[_parent[now]]);
				now = _parent[now];
				if (now == there)
				{
					break;
				}
			}

			CRASH("DEAD_LOCK_DETACTECTED");
		}

	}

	_finished[here] = true;
}
