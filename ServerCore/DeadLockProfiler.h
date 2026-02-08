#pragma once

#include <stack>
#include <map>
#include <vector>

class DeadLockProfiler
{
public:
	void PushLock(const char* lockName);
	void PopLock(const char* lockName);
	void CheckCycle();

private:
	void Dfs(int32 index);

private:
	unordered_map<const char*, int32> _nameToId;
	unordered_map<int32, const char*> _idToName;
	map<int32, set<int32>> _lockHistory;

	Mutex _mutex;

private:
	vector<int32>_discoverOrder; // 사이클의 값들을 임시로 넣음. 노드가 발견된 순서를 기록하는 배열
	int32 _discoverCount = 0; // 노드가 발견된 순서를 기록하는 카운터
	vector<bool> _finished; // 사이클 탐색이 끝난 노드인지 여부
	vector<int32> _parent; // 사이클을 추적하기 위한 부모 노드 배열
};

