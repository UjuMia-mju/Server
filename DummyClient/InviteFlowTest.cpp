#include "pch.h"
#include "InviteFlowTest.h"
#include "PacketStats.h"
#include <algorithm>

using namespace TestPhase;

// =========================================================
// 테스트 설정 (여기 값만 바꿔서 실행)
// =========================================================
static const int32 CLIENT_COUNT = 100;		// 접속 인원 (최대 100, DB 계정 수)
static const int32 GROUP_SIZE = 4;			// 방 하나 인원 (호스트 포함)
static const bool VERBOSE = false;			// true면 패킷 핸들러의 cout 로그도 출력

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
// [공용] 접속 -> Login -> Create -> Join
// =========================================================
vector<Group> InviteFlowTest::SetupRooms(int32 clientCount, int32 groupSize, vector<PhaseResult>& results, int32& allGroupCount, ostream& out)
{
	// 초대 자동 수락 시 1초 대기하면 IOCP 워커가 막혀서 측정이 왜곡되므로 즉시 수락
	GInviteAcceptDelayMs = 0;

	auto clients = CreateClients(clientCount, out);
	auto groups = MakeGroups(clients, groupSize);
	allGroupCount = static_cast<int32>(groups.size());

	results.push_back(RunPhase("Login", clients, Login, IsLoggedIn, out));

	auto loggedInGroups = FilterGroups(groups, [](const Group& g) { return all_of(g.members.begin(), g.members.end(), IsLoggedIn); });
	results.push_back(RunPhase("Create", Hosts(loggedInGroups), CreateRoom, IsRoomCreated, out));

	auto createdGroups = FilterGroups(loggedInGroups, [](const Group& g) { return IsRoomCreated(g.Host()); });
	results.push_back(RunPhase("Join", createdGroups, InviteGuests, IsAllJoined, out));

	return FilterGroups(createdGroups, IsAllJoined);
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

	out << "\n========== INVITE FLOW TEST START ==========" << endl;
	out << "  Clients: " << CLIENT_COUNT << " / Group size: " << GROUP_SIZE << endl;

	vector<PhaseResult> results;
	int32 allGroupCount = 0;
	auto rooms = SetupRooms(CLIENT_COUNT, GROUP_SIZE, results, allGroupCount, out);

	Report(results, out);
	out << "  (Join is per group: host invite -> all " << (GROUP_SIZE - 1) << " guests entered)" << endl;
	PacketStats::Report(out);

	// 바로 종료하면 콘솔 창이 닫혀서 리포트를 못 보므로 엔터 입력까지 대기
	WaitForEnter("Press Enter to exit...", out);

	// IOCP 워커 스레드가 무한 루프라 프로세스를 바로 종료
	::ExitProcess(static_cast<int32>(rooms.size()) == allGroupCount ? 0 : 1);
}
// =========================================================
