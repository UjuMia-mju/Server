#pragma once
#include "ServerSession.h"

// =========================================================
// 단계별 테스트 공용 도구 (InviteFlowTest, GameFlowTest)
//   CreateClients -> MakeGroups -> RunPhase(...) 반복 -> Report
// =========================================================
namespace TestPhase
{
	using Client = shared_ptr<ServerSession>;

	// members[0] = 호스트, 나머지 = 게스트
	struct Group
	{
		vector<Client> members;

		const Client& Host() const { return members[0]; }
		vector<Client> Guests() const { return vector<Client>(members.begin() + 1, members.end()); }
	};

	struct PhaseResult
	{
		string name;
		int32 total = 0;
		int32 success = 0;
		int64 elapsedMs = 0;		// 단계 전체 시간 (첫 요청 ~ 마지막 완료 or 타임아웃)
		vector<int64> latencyUs;	// 성공한 대상별 요청 ~ 완료 시간
	};

	const auto PHASE_TIMEOUT = 30s;	// 단계마다 최대 대기 시간

	int64 NowUs();

	// count명 접속 (로그인은 아직 안 함). i번째 = player(i+1)
	vector<Client> CreateClients(int32 count, ostream& out);
	// size명씩 그룹으로 나눔 (남는 인원은 제외)
	vector<Group> MakeGroups(const vector<Client>& clients, int32 size);
	vector<Client> Hosts(const vector<Group>& groups);
	vector<Client> AllMembers(const vector<Group>& groups);

	// pred를 만족하는 그룹만 남김 (이전 단계에서 실패한 그룹은 다음 단계에서 제외)
	template<typename Pred>
	vector<Group> FilterGroups(const vector<Group>& groups, Pred pred)
	{
		vector<Group> result;
		for (auto& group : groups)
			if (pred(group))
				result.push_back(group);

		return result;
	}

	// targets 전부에게 action을 보내고 isDone이 될 때까지 대기 (최대 PHASE_TIMEOUT)
	//   - 대상마다 action 직전 시각을 기록해서 완료까지의 지연을 측정
	//   - 완료 여부는 10ms 단위로 폴링하므로 지연에 최대 10ms 오차가 있음
	template<typename T, typename Action, typename IsDone>
	PhaseResult RunPhase(const string& name, const vector<T>& targets, Action action, IsDone isDone, ostream& out)
	{
		PhaseResult result;
		result.name = name;
		result.total = static_cast<int32>(targets.size());

		vector<int64> sentUs(targets.size());
		vector<bool> done(targets.size(), false);

		auto start = chrono::steady_clock::now();
		for (size_t i = 0; i < targets.size(); i++)
		{
			sentUs[i] = NowUs();
			action(targets[i]);
		}

		while (result.success < result.total && chrono::steady_clock::now() - start < PHASE_TIMEOUT)
		{
			this_thread::sleep_for(10ms);

			int64 now = NowUs();
			for (size_t i = 0; i < targets.size(); i++)
			{
				if (done[i] || isDone(targets[i]) == false)
					continue;

				done[i] = true;
				result.success++;
				result.latencyUs.push_back(now - sentUs[i]);
			}
		}

		result.elapsedMs = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start).count();
		out << "[Phase] " << name << " " << result.success << " / " << result.total << " (" << result.elapsedMs << "ms)" << endl;
		return result;
	}

	// 단계별 성공 수 / 전체 시간 / 지연(avg, p50, p99, max) 표 출력
	void Report(vector<PhaseResult> results, ostream& out);

	// 엔터 입력까지 대기
	void WaitForEnter(const string& message, ostream& out);
}
