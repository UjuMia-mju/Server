#include "pch.h"
#include "InviteFlowTest.h"
#include "StressTest.h"
#include "Service.h"
#include "ThreadManager.h"
#include "ServerSession.h"
#include <algorithm>
#include <iomanip>
#include <numeric>

// =========================================================
// 테스트 설정 (여기 값만 바꿔서 실행)
// =========================================================
static const int32 CLIENT_COUNT = 100;		// 접속 인원 (최대 100, DB 계정 수)
static const int32 GROUP_SIZE = 4;			// 방 하나 인원 (호스트 포함)
static const auto PHASE_TIMEOUT = 30s;		// 단계마다 최대 대기 시간
static const bool VERBOSE = false;			// true면 패킷 핸들러의 cout 로그도 출력

static const string PASSWORD = "1234";

using Client = shared_ptr<ServerSession>;

// groups[i].members[0] = 호스트, 나머지 = 초대받을 게스트
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

// =========================================================
// 공용 유틸
// =========================================================
static int64 NowUs()
{
	return chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now().time_since_epoch()).count();
}

// =========================================================
// [준비] count명 접속 (로그인은 아직 안 함). i번째 = player(i+1)
// =========================================================
static vector<Client> CreateClients(int32 count, ostream& out)
{
	count = StressTest::ClampClientCount(count, out);

	vector<Client> clients;
	ClientServiceRef service = MakeShared<ClientService>(
		NetAddress(L"127.0.0.1", 7777),
		MakeShared<IocpCore>(),
		[&]() {
			string email = "player" + to_string(clients.size() + 1) + "@test.com";
			auto client = MakeShared<ServerSession>(email, PASSWORD, false);
			clients.push_back(client);
			return client;
		},
		count);

	for (int32 i = 0; i < 8; i++)
	{
		GThreadManager->Launch([service]()
			{
				while (true)
				{
					service->GetIocpCore()->Dispatch();
				}
			});
	}

	// Start()는 세션을 순서대로 동기 생성하므로, 리턴 후 clients는 더 이상 바뀌지 않음
	ASSERT_CRASH(service->Start());

	auto start = chrono::steady_clock::now();
	int32 connected = 0;
	while (chrono::steady_clock::now() - start < PHASE_TIMEOUT)
	{
		connected = static_cast<int32>(count_if(clients.begin(), clients.end(), [](const Client& c) { return c->IsConnected(); }));
		if (connected == count)
			break;

		this_thread::sleep_for(10ms);
	}

	out << "[InviteFlow] Connected " << connected << " / " << count << endl;
	return clients;
}

// =========================================================
// [준비] size명씩 그룹으로 나눔 (남는 인원은 제외)
// =========================================================
static vector<Group> MakeGroups(const vector<Client>& clients, int32 size)
{
	vector<Group> groups;
	for (size_t i = 0; i + size <= clients.size(); i += size)
		groups.push_back(Group{ vector<Client>(clients.begin() + i, clients.begin() + i + size) });

	return groups;
}

static vector<Client> Hosts(const vector<Group>& groups)
{
	vector<Client> hosts;
	for (auto& group : groups)
		hosts.push_back(group.Host());

	return hosts;
}

// pred를 만족하는 그룹만 남김 (이전 단계에서 실패한 그룹은 다음 단계에서 제외)
template<typename Pred>
static vector<Group> FilterGroups(const vector<Group>& groups, Pred pred)
{
	vector<Group> result;
	for (auto& group : groups)
		if (pred(group))
			result.push_back(group);

	return result;
}

// =========================================================
// [단계 실행] targets 전부에게 action을 보내고 isDone이 될 때까지 대기
//   - 대상마다 action 직전 시각을 기록해서 완료까지의 지연을 측정
//   - 완료 여부는 10ms 단위로 폴링하므로 지연에 최대 10ms 오차가 있음
// =========================================================
template<typename T, typename Action, typename IsDone>
static PhaseResult RunPhase(const string& name, const vector<T>& targets, Action action, IsDone isDone, ostream& out)
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
	out << "[InviteFlow] " << name << " " << result.success << " / " << result.total << " (" << result.elapsedMs << "ms)" << endl;
	return result;
}

// =========================================================
// 단계별 action / 완료 조건
// =========================================================
static void Login(const Client& client) { client->SendLogin(); }
static bool IsLoggedIn(const Client& client) { return client->IsLoginCompleted(); }

static void CreateRoom(const Client& host) { host->SendCreateRoom(); }
static bool IsRoomCreated(const Client& host) { return host->IsInRoom(); }

static void InviteGuests(const Group& group)
{
	for (auto& guest : group.Guests())
		group.Host()->SendInvitePacket(guest->GetPlayerName(), guest->GetPlayerTag());
}

static bool IsAllJoined(const Group& group)
{
	for (auto& guest : group.Guests())
		if (guest->GetRoomId() != group.Host()->GetRoomId())
			return false;

	return true;
}

// =========================================================
// [리포트]
// =========================================================
static double PercentileMs(vector<int64>& sortedUs, int32 p)
{
	if (sortedUs.empty())
		return 0;

	size_t index = min(sortedUs.size() - 1, sortedUs.size() * p / 100);
	return sortedUs[index] / 1000.0;
}

static void Report(vector<PhaseResult> results, ostream& out)
{
	out << "\n========== INVITE FLOW REPORT ==========" << endl;
	out << left << setw(8) << "Phase" << right << setw(10) << "Success" << setw(10) << "Total"
		<< setw(10) << "avg" << setw(10) << "p50" << setw(10) << "p99" << setw(10) << "max" << endl;

	out << fixed << setprecision(1);
	for (auto& r : results)
	{
		auto& lat = r.latencyUs;
		sort(lat.begin(), lat.end());
		double avg = lat.empty() ? 0 : accumulate(lat.begin(), lat.end(), 0LL) / 1000.0 / lat.size();
		double maxMs = lat.empty() ? 0 : lat.back() / 1000.0;

		out << left << setw(8) << r.name << right
			<< setw(10) << (to_string(r.success) + "/" + to_string(r.total))
			<< setw(8) << r.elapsedMs << "ms"
			<< setw(8) << avg << "ms" << setw(8) << PercentileMs(lat, 50) << "ms"
			<< setw(8) << PercentileMs(lat, 99) << "ms" << setw(8) << maxMs << "ms" << endl;
	}

	for (auto& r : results)
		if (r.success < r.total)
			out << "  * " << r.name << ": " << (r.total - r.success) << " failed or timed out" << endl;

	out << "  (Join is per group: host invite -> all " << (GROUP_SIZE - 1) << " guests entered)" << endl;
}

// =========================================================
// [방 생성 + 초대 흐름 테스트]
// =========================================================
void InviteFlowTest::Run()
{
	// 패킷 핸들러들의 cout 로그가 클라이언트를 느리게 만들기 때문에 꺼두고, 통계는 out으로만 출력
	streambuf* consoleBuf = cout.rdbuf();
	ostream out(consoleBuf);
	if (VERBOSE == false)
		cout.rdbuf(nullptr);

	// 초대 자동 수락 시 1초 대기하면 IOCP 워커가 막혀서 측정이 왜곡되므로 즉시 수락
	GInviteAcceptDelayMs = 0;

	out << "\n========== INVITE FLOW TEST START ==========" << endl;
	out << "  Clients: " << CLIENT_COUNT << " / Group size: " << GROUP_SIZE << endl;

	auto clients = CreateClients(CLIENT_COUNT, out);
	auto groups = MakeGroups(clients, GROUP_SIZE);

	auto login = RunPhase("Login", clients, Login, IsLoggedIn, out);

	auto loggedInGroups = FilterGroups(groups, [](const Group& g) { return all_of(g.members.begin(), g.members.end(), IsLoggedIn); });
	auto create = RunPhase("Create", Hosts(loggedInGroups), CreateRoom, IsRoomCreated, out);

	auto createdGroups = FilterGroups(loggedInGroups, [](const Group& g) { return IsRoomCreated(g.Host()); });
	auto join = RunPhase("Join", createdGroups, InviteGuests, IsAllJoined, out);

	Report({ login, create, join }, out);

	// 바로 종료하면 콘솔 창이 닫혀서 리포트를 못 보므로 엔터 입력까지 대기
	out << "\nPress Enter to exit..." << endl;
	cin.get();

	// IOCP 워커 스레드가 무한 루프라 프로세스를 바로 종료
	::ExitProcess(join.success == join.total && join.total == static_cast<int32>(groups.size()) ? 0 : 1);
}
// =========================================================
