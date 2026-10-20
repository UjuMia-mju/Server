#include "pch.h"
#include "ServerAuthorityTest.h"
#include "TestPhase.h"
#include "ServerPacketHandler.h"
#include "RelayStats.h"
#include <algorithm>
#include <array>

using namespace TestPhase;

// =========================================================
// 테스트 설정 (여기 값만 바꿔서 실행)
// =========================================================
static const int32 CLIENT_COUNT = 100;		// 접속 인원 (최대 100, DB 계정 수)
static const int32 ENTER_ITERATIONS = 50;	// enter: 방 20개 x 50회 = 동시 입장 경합 1000건
static const int32 START_ITERATIONS = 40;	// start: 방 25개 x 40회 = 시작/레디 취소 경합 1000건
static const auto RESPONSE_TIMEOUT = 10s;		// 응답 하나를 기다리는 최대 시간
static const bool VERBOSE = false;			// true면 패킷 핸들러의 cout 로그도 출력

struct ScenarioResult
{
	string name;
	int32 total = 0;
	int32 passed = 0;
	bool timedOut = false;
	vector<string> failures;	// 실패 사유 (앞의 몇 개만 보관)

	void Fail(const string& reason)
	{
		if (failures.size() < 5)
			failures.push_back(reason);
	}
	bool IsPassed() const { return timedOut == false && total > 0 && passed == total; }
};

// =========================================================
// 공용 도구
// =========================================================
template<typename Pred>
static bool WaitUntil(Pred pred, chrono::milliseconds timeout = RESPONSE_TIMEOUT)
{
	auto start = chrono::steady_clock::now();
	while (pred() == false)
	{
		if (chrono::steady_clock::now() - start > timeout)
			return false;

		this_thread::sleep_for(5ms);
	}
	return true;
}

static int32 RecvCount(const Client& client, uint16 pktId)
{
	return client->GetRecv(pktId).count;
}

static bool CreateRooms(const vector<Client>& hosts)
{
	for (auto& host : hosts)
		host->SendCreateRoom();

	return WaitUntil([&]() { return all_of(hosts.begin(), hosts.end(), [](const Client& c) { return c->IsInRoom(); }); });
}

// 방 ID로 입장하고 S_ENTER_ROOM 성공까지 대기
static bool EnterRooms(const vector<pair<Client, uint64>>& entries)
{
	for (auto& [client, roomId] : entries)
		client->SendEnterRoom(roomId);

	return WaitUntil([&]() {
		return all_of(entries.begin(), entries.end(), [](const pair<Client, uint64>& e) { return e.first->GetRoomId() == e.second; });
		});
}

// 방장이 나가면 방이 파괴되고, 남아 있던 사람(방장 포함)은 S_ROOM_DESTROY를 받아 roomId가 0이 됨
static bool CloseRooms(const vector<Client>& hosts, const vector<Client>& everyone)
{
	for (auto& host : hosts)
		host->SendLeaveRoom();

	return WaitUntil([&]() { return all_of(everyone.begin(), everyone.end(), [](const Client& c) { return c->IsInRoom() == false; }); });
}

static void ClearRecv(const vector<Client>& clients)
{
	for (auto& client : clients)
		client->ClearRecv();
}

// =========================================================
// [unauth] 로그인하지 않은 클라이언트가 로그인/방이 필요한 패킷을 보냄 (작업 4)
//   예전에는 C_PLAYER_ANIMATION, C_GET_CURRENCY, C_TEST_ENTER_GAME에서 널 역참조로 서버가 죽었음
//   통과 조건: 응답 패킷이 있는 것은 실패 응답, 나머지는 무시 + 그 뒤에 같은 클라이언트가 정상 로그인 (서버 생존)
// =========================================================
static ScenarioResult RunUnauthenticated(const vector<Client>& clients, ostream& out)
{
	ScenarioResult result;
	result.name = "unauth";

	ClearRecv(clients);
	for (auto& client : clients)
	{
		{ Protocol::C_PLAYER_ANIMATION pkt; pkt.set_state(1); client->Send(ServerPacketHandler::MakeSendBuffer(pkt)); }
		{ Protocol::C_GET_CURRENCY pkt; client->Send(ServerPacketHandler::MakeSendBuffer(pkt)); }
		{ Protocol::C_TEST_ENTER_GAME pkt; client->Send(ServerPacketHandler::MakeSendBuffer(pkt)); }
		{ Protocol::C_TEST_ENTER_ROOM pkt; client->Send(ServerPacketHandler::MakeSendBuffer(pkt)); }
		{ Protocol::C_MOVE pkt; client->Send(ServerPacketHandler::MakeSendBuffer(pkt)); }
		{ Protocol::C_PLAYER_STAT_EVENT pkt; client->Send(ServerPacketHandler::MakeSendBuffer(pkt)); }
		{ Protocol::C_SHOW_STAGE pkt; client->Send(ServerPacketHandler::MakeSendBuffer(pkt)); }
		{ Protocol::C_GET_DB_DATA pkt; client->Send(ServerPacketHandler::MakeSendBuffer(pkt)); }
		{ Protocol::C_GACHA_POOL_LIST pkt; client->Send(ServerPacketHandler::MakeSendBuffer(pkt)); }
	}

	// 실패 응답이 오는 두 패킷의 응답을 기다린 뒤, 나머지 응답이 늦게 오는 것도 잡도록 조금 더 대기
	WaitUntil([&]() { return all_of(clients.begin(), clients.end(), [](const Client& c) {
		return RecvCount(c, PKT_S_GET_CURRENCY) > 0 && RecvCount(c, PKT_S_SHOW_STAGE) > 0; }); }, 3s);
	this_thread::sleep_for(500ms);

	// 서버가 살아 있으면 같은 클라이언트가 그대로 로그인할 수 있어야 함
	for (auto& client : clients)
		client->SendLogin();
	WaitUntil([&]() { return all_of(clients.begin(), clients.end(), [](const Client& c) { return c->IsLoginCompleted(); }); });

	for (auto& client : clients)
	{
		RecvRecord currency = client->GetRecv(PKT_S_GET_CURRENCY);
		RecvRecord showStage = client->GetRecv(PKT_S_SHOW_STAGE);
		// 로그인/방이 필요한 패킷이 처리됐다면 받았을 응답들 (받으면 안 됨)
		int32 leaked = RecvCount(client, PKT_S_ENTER_GAME) + RecvCount(client, PKT_S_ENTER_ROOM)
			+ RecvCount(client, PKT_S_STAGE_INFO) + RecvCount(client, PKT_S_GACHA_POOL_LIST) + RecvCount(client, PKT_S_SKIN_LIST);

		bool currencyRejected = currency.count > 0 && currency.success == false;
		bool showStageRejected = showStage.count > 0 && showStage.success == false;

		result.total++;
		if (currencyRejected && showStageRejected && leaked == 0 && client->IsLoginCompleted())
		{
			result.passed++;
			continue;
		}

		result.Fail(client->GetPlayerName() + ": currency rejected=" + to_string(currencyRejected)
			+ ", show stage rejected=" + to_string(showStageRejected) + ", unexpected responses=" + to_string(leaked)
			+ ", login after=" + to_string(client->IsLoginCompleted()));
	}

	return result;
}

// =========================================================
// [enter] 3인 방의 남은 1자리에 2명이 동시에 입장 (작업 1)
//   그룹 5명: [0] 방장, [1][2] 먼저 입장, [3][4] 동시에 입장
// =========================================================
static ScenarioResult RunEnterRace(const vector<Client>& clients, ostream& out)
{
	ScenarioResult result;
	result.name = "enter";

	auto groups = MakeGroups(clients, 5);
	auto hosts = Hosts(groups);
	auto everyone = AllMembers(groups);

	for (int32 iter = 1; iter <= ENTER_ITERATIONS; iter++)
	{
		ClearRecv(everyone);

		if (CreateRooms(hosts) == false)
		{
			result.timedOut = true;
			out << "  [enter] timeout: create room (iter " << iter << ")" << endl;
			break;
		}

		vector<pair<Client, uint64>> firstEntries;
		for (auto& group : groups)
		{
			firstEntries.push_back({ group.members[1], group.Host()->GetRoomId() });
			firstEntries.push_back({ group.members[2], group.Host()->GetRoomId() });
		}

		if (EnterRooms(firstEntries) == false)
		{
			result.timedOut = true;
			out << "  [enter] timeout: fill room to 3 (iter " << iter << ")" << endl;
			break;
		}

		// 남은 1자리에 2명이 동시에 입장
		for (auto& group : groups)
		{
			group.members[3]->SendEnterRoom(group.Host()->GetRoomId());
			group.members[4]->SendEnterRoom(group.Host()->GetRoomId());
		}

		bool allResponded = WaitUntil([&]() {
			return all_of(groups.begin(), groups.end(), [](const Group& g) {
				return RecvCount(g.members[3], PKT_S_ENTER_ROOM) > 0 && RecvCount(g.members[4], PKT_S_ENTER_ROOM) > 0;
				});
			});

		if (allResponded == false)
		{
			result.timedOut = true;
			out << "  [enter] timeout: concurrent enter response (iter " << iter << ")" << endl;
			break;
		}

		for (auto& group : groups)
		{
			RecvRecord r1 = group.members[3]->GetRecv(PKT_S_ENTER_ROOM);
			RecvRecord r2 = group.members[4]->GetRecv(PKT_S_ENTER_ROOM);
			int32 entered = (r1.success ? 1 : 0) + (r2.success ? 1 : 0);
			const RecvRecord& winner = r1.success ? r1 : r2;
			const RecvRecord& loser = r1.success ? r2 : r1;

			result.total++;
			// 정확히 1명만 성공 + 실패한 쪽은 "Room is full" + 성공 응답의 멤버 목록은 입장이 반영된 4명
			if (entered == 1 && loser.errorMsg == "Room is full" && winner.value == 4)
			{
				result.passed++;
				continue;
			}

			result.Fail("iter " + to_string(iter) + " room " + to_string(group.Host()->GetRoomId())
				+ ": entered=" + to_string(entered) + ", members in S_ENTER_ROOM=" + to_string(winner.value)
				+ ", loser error='" + loser.errorMsg + "'");
		}

		if (CloseRooms(hosts, everyone) == false)
		{
			result.timedOut = true;
			out << "  [enter] timeout: close room (iter " << iter << ")" << endl;
			break;
		}
	}

	return result;
}

// =========================================================
// [start] 방장의 C_START_ROOM과 멤버의 C_READY(false)를 동시에 보냄 (작업 2)
//   그룹 4명: [0] 방장, [1] 레디 취소하는 멤버, [2][3] 멤버
//   방장이 받은 순서로 판정 (둘 다 방 작업 큐에서 보내므로 처리 순서 그대로 도착)
//   - S_START_ROOM(성공) -> S_READY(false) : 시작 시점에 전원 준비 상태였음 (통과)
//   - S_READY(false) -> S_START_ROOM(실패) : 레디 취소가 먼저 처리돼서 시작 거절 (통과)
//   - S_READY(false) -> S_START_ROOM(성공) : 준비 안 된 사람이 있는데 시작됨 (실패)
// =========================================================
static ScenarioResult RunStartReadyRace(const vector<Client>& clients, ostream& out)
{
	ScenarioResult result;
	result.name = "start";

	auto groups = MakeGroups(clients, 4);
	auto hosts = Hosts(groups);
	auto everyone = AllMembers(groups);
	int32 started = 0;
	int32 rejected = 0;

	for (int32 iter = 1; iter <= START_ITERATIONS; iter++)
	{
		ClearRecv(everyone);

		if (CreateRooms(hosts) == false)
		{
			result.timedOut = true;
			out << "  [start] timeout: create room (iter " << iter << ")" << endl;
			break;
		}

		vector<pair<Client, uint64>> entries;
		for (auto& group : groups)
			for (auto& guest : group.Guests())
				entries.push_back({ guest, group.Host()->GetRoomId() });

		if (EnterRooms(entries) == false)
		{
			result.timedOut = true;
			out << "  [start] timeout: enter room (iter " << iter << ")" << endl;
			break;
		}

		// 전원 레디 -> 방장이 4명분 S_READY를 받으면 준비 완료
		for (auto& client : everyone)
			client->SendReady(true);

		if (WaitUntil([&]() { return all_of(hosts.begin(), hosts.end(), [](const Client& c) { return RecvCount(c, PKT_S_READY) >= 4; }); }) == false)
		{
			result.timedOut = true;
			out << "  [start] timeout: all ready (iter " << iter << ")" << endl;
			break;
		}

		// 시작과 레디 취소를 동시에
		ClearRecv(hosts);
		for (auto& group : groups)
		{
			group.Host()->SendStartRoom();
			group.members[1]->SendReady(false);
		}

		if (WaitUntil([&]() { return all_of(hosts.begin(), hosts.end(), [](const Client& c) {
			return RecvCount(c, PKT_S_START_ROOM) > 0 && RecvCount(c, PKT_S_READY) > 0; }); }) == false)
		{
			result.timedOut = true;
			out << "  [start] timeout: start / ready(false) response (iter " << iter << ")" << endl;
			break;
		}

		for (auto& group : groups)
		{
			RecvRecord start = group.Host()->GetRecv(PKT_S_START_ROOM);
			RecvRecord ready = group.Host()->GetRecv(PKT_S_READY);
			bool readyCancelledFirst = ready.lastSeq < start.lastSeq;

			result.total++;
			if (start.success && readyCancelledFirst)
			{
				result.Fail("iter " + to_string(iter) + " room " + to_string(group.Host()->GetRoomId())
					+ ": game started after S_READY(false)");
				continue;
			}

			result.passed++;
			(start.success ? started : rejected)++;
		}

		if (CloseRooms(hosts, everyone) == false)
		{
			result.timedOut = true;
			out << "  [start] timeout: close room (iter " << iter << ")" << endl;
			break;
		}
	}

	out << "  [start] started " << started << " / rejected (not all ready) " << rejected << endl;
	return result;
}

// =========================================================
// [relay] 보낸 사람과 require_host_authority 조합별 전달 대상 (작업 3)
//   그룹 4명: [0] 방장, [1] 보내는 멤버, [2][3] 멤버
//   케이스마다 한 번씩 보내고, 각자 받은 S_RELAY_PACKET 수가 기대값과 정확히 같은지 확인
// =========================================================
static ScenarioResult RunRelayRoute(const vector<Client>& clients, ostream& out)
{
	ScenarioResult result;
	result.name = "relay";

	struct RelayCase
	{
		string name;
		int32 senderIndex;
		bool requireHostAuthority;
		array<int32, 4> expected;	// [0] 방장 ~ [3] 멤버가 받아야 하는 수
	};
	const vector<RelayCase> cases =
	{
		{ "member/false", 1, false, { 1, 0, 0, 0 } },	// 멤버는 플래그와 상관없이 방장에게만
		{ "member/true",  1, true,  { 1, 0, 0, 0 } },
		{ "host/false",   0, false, { 0, 1, 1, 1 } },	// 방장: 본인 제외 전체
		{ "host/true",    0, true,  { 1, 1, 1, 1 } },	// 방장: 본인 포함 전체
	};

	auto groups = MakeGroups(clients, 4);
	auto hosts = Hosts(groups);
	auto everyone = AllMembers(groups);
	ClearRecv(everyone);

	if (CreateRooms(hosts) == false)
	{
		result.timedOut = true;
		out << "  [relay] timeout: create room" << endl;
		return result;
	}

	vector<pair<Client, uint64>> entries;
	for (auto& group : groups)
		for (auto& guest : group.Guests())
			entries.push_back({ guest, group.Host()->GetRoomId() });

	if (EnterRooms(entries) == false)
	{
		result.timedOut = true;
		out << "  [relay] timeout: enter room" << endl;
		return result;
	}

	string payload(sizeof(int64), 'x');
	for (auto& relayCase : cases)
	{
		ClearRecv(everyone);

		for (auto& group : groups)
		{
			RelayStats::StampSendTime(payload);

			Protocol::C_RELAY_PACKET pkt;
			pkt.set_packet_id(1);
			pkt.set_require_host_authority(relayCase.requireHostAuthority);
			pkt.set_payload(payload);
			group.members[relayCase.senderIndex]->Send(ServerPacketHandler::MakeSendBuffer(pkt));
		}

		// 받아야 하는 사람이 다 받을 때까지 기다린 뒤, 받으면 안 되는 사람에게 늦게 도착하는 것도 잡도록 조금 더 대기
		WaitUntil([&]() {
			return all_of(groups.begin(), groups.end(), [&](const Group& g) {
				for (int32 i = 0; i < 4; i++)
					if (RecvCount(g.members[i], PKT_S_RELAY_PACKET) < relayCase.expected[i])
						return false;
				return true;
				});
			}, 3s);
		this_thread::sleep_for(300ms);

		for (auto& group : groups)
		{
			array<int32, 4> got = {};
			for (int32 i = 0; i < 4; i++)
				got[i] = RecvCount(group.members[i], PKT_S_RELAY_PACKET);

			result.total++;
			if (got == relayCase.expected)
			{
				result.passed++;
				continue;
			}

			result.Fail(relayCase.name + " room " + to_string(group.Host()->GetRoomId()) + ": recv [host, sender/member, member, member] = ["
				+ to_string(got[0]) + ", " + to_string(got[1]) + ", " + to_string(got[2]) + ", " + to_string(got[3]) + "]");
		}
	}

	if (CloseRooms(hosts, everyone) == false)
	{
		result.timedOut = true;
		out << "  [relay] timeout: close room" << endl;
	}

	return result;
}

// =========================================================
// [invite] 초대를 받은 뒤 방장이 게임 시작 -> 수락 (작업 1)
//   그룹 3명: [0] A = 초대하는 방장 (혼자 있는 방 RA)
//             [1] C = 초대받는 사람 (자기 방 RC의 방장)
//             [2] D = RC 멤버 (C가 RC에 그대로 남았는지 확인용)
// =========================================================
static ScenarioResult RunInviteWhilePlaying(const vector<Client>& clients, ostream& out)
{
	ScenarioResult result;
	result.name = "invite";

	auto groups = MakeGroups(clients, 3);
	auto everyone = AllMembers(groups);
	vector<Client> hostsA, hostsC;
	for (auto& group : groups)
	{
		hostsA.push_back(group.members[0]);
		hostsC.push_back(group.members[1]);
	}

	// 초대 알림을 받아도 자동 수락하지 않고, 게임이 시작된 뒤에 직접 수락
	GInviteAutoAccept = false;
	ClearRecv(everyone);

	auto fail = [&](const string& step) {
		result.timedOut = true;
		out << "  [invite] timeout: " << step << endl;
		GInviteAutoAccept = true;
		return result;
		};

	vector<Client> roomHosts = hostsA;
	roomHosts.insert(roomHosts.end(), hostsC.begin(), hostsC.end());
	if (CreateRooms(roomHosts) == false)
		return fail("create rooms");

	vector<pair<Client, uint64>> entries;
	for (auto& group : groups)
		entries.push_back({ group.members[2], group.members[1]->GetRoomId() });
	if (EnterRooms(entries) == false)
		return fail("D enters RC");

	// 1. A가 C를 초대하고 레디
	for (auto& group : groups)
	{
		group.members[0]->SendInvitePacket(group.members[1]->GetPlayerName(), group.members[1]->GetPlayerTag());
		group.members[0]->SendReady(true);
	}

	if (WaitUntil([&]() { return all_of(groups.begin(), groups.end(), [](const Group& g) {
		return RecvCount(g.members[1], PKT_S_INVITE_NOTIFICATION) > 0 && RecvCount(g.members[0], PKT_S_READY) > 0; }); }) == false)
		return fail("invite notification / A ready");

	// 2. A가 게임 시작 (A 혼자라 전원 준비 상태)
	for (auto& host : hostsA)
		host->SendStartRoom();

	if (WaitUntil([&]() { return all_of(hostsA.begin(), hostsA.end(), [](const Client& c) { return RecvCount(c, PKT_S_START_ROOM) > 0; }); }) == false)
		return fail("A start room");

	// 3. 게임이 시작된 뒤에 C가 초대 수락
	for (auto& group : groups)
		group.members[1]->SendInviteResponse(static_cast<uint64>(group.members[1]->GetRecv(PKT_S_INVITE_NOTIFICATION).value), true);

	if (WaitUntil([&]() { return all_of(hostsC.begin(), hostsC.end(), [](const Client& c) { return RecvCount(c, PKT_S_INVITE_RESPONSE) > 0; }); }) == false)
		return fail("C invite response");

	// 4. C가 RC에 그대로 있는지: C가 레디하면 같은 방 D가 C의 S_READY를 받아야 함
	for (auto& group : groups)
	{
		group.members[2]->ClearRecv();
		group.members[1]->SendReady(true);
	}
	WaitUntil([&]() { return all_of(groups.begin(), groups.end(), [](const Group& g) { return RecvCount(g.members[2], PKT_S_READY) > 0; }); }, 3s);

	for (auto& group : groups)
	{
		const Client& a = group.members[0];
		const Client& c = group.members[1];
		const Client& d = group.members[2];
		RecvRecord invite = c->GetRecv(PKT_S_INVITE_RESPONSE);
		RecvRecord dReady = d->GetRecv(PKT_S_READY);
		uint64 rc = d->GetRoomId();

		bool rejected = invite.success == false;
		bool stayed = rc != 0 && c->GetRoomId() == rc && dReady.count > 0 && dReady.value == c->GetPlayerId();
		bool notInPlayingRoom = RecvCount(a, PKT_S_ROOM_MEMBER_ENTER) == 0;

		result.total++;
		if (rejected && stayed && notInPlayingRoom)
		{
			result.passed++;
			continue;
		}

		result.Fail(c->GetPlayerName() + ": invite success=" + to_string(invite.success) + " ('" + invite.errorMsg + "')"
			+ ", stayed in own room=" + to_string(stayed) + ", entered playing room=" + to_string(!notInPlayingRoom));
	}

	GInviteAutoAccept = true;

	// 정리: RA, RC 둘 다 방장이 나가서 파괴. 실패해서 C가 RA로 옮겨간 경우에도 A가 나가면 같이 정리됨
	if (CloseRooms(roomHosts, everyone) == false)
	{
		result.timedOut = true;
		out << "  [invite] timeout: close rooms" << endl;
	}

	return result;
}

// =========================================================
// [실행]
// =========================================================
static void Report(const vector<ScenarioResult>& results, ostream& out)
{
	out << "\n========== SERVER AUTHORITY REPORT ==========" << endl;
	for (auto& r : results)
	{
		out << "  [" << (r.IsPassed() ? "PASS" : "FAIL") << "] " << r.name << " : " << r.passed << " / " << r.total
			<< (r.timedOut ? " (timeout)" : "") << endl;
		for (auto& reason : r.failures)
			out << "      - " << reason << endl;
	}
}

int32 ServerAuthorityTest::Run(const vector<string>& scenarios)
{
	// 패킷 핸들러들의 cout 로그가 클라이언트를 느리게 만들기 때문에 꺼두고, 결과는 out으로만 출력
	streambuf* consoleBuf = cout.rdbuf();
	ostream out(consoleBuf);
	if (VERBOSE == false)
		cout.rdbuf(nullptr);

	auto wants = [&](const string& name) {
		return scenarios.empty() || find(scenarios.begin(), scenarios.end(), name) != scenarios.end();
		};

	out << "\n========== SERVER AUTHORITY TEST START ==========" << endl;
	GInviteAcceptDelayMs = 0;

	auto clients = CreateClients(CLIENT_COUNT, out);
	vector<ScenarioResult> results;

	// 로그인 전에 해야 하므로 가장 먼저 (앞의 4명만 사용, 이 4명은 시나리오 안에서 로그인까지 함)
	if (wants("unauth"))
		results.push_back(RunUnauthenticated(vector<Client>(clients.begin(), clients.begin() + min<size_t>(4, clients.size())), out));

	RunPhase("Login", clients, [](const Client& c) { if (c->IsLoginCompleted() == false) c->SendLogin(); }, [](const Client& c) { return c->IsLoginCompleted(); }, out);

	if (wants("enter"))
		results.push_back(RunEnterRace(clients, out));
	if (wants("invite"))
		results.push_back(RunInviteWhilePlaying(clients, out));
	if (wants("start"))
		results.push_back(RunStartReadyRace(clients, out));
	if (wants("relay"))
		results.push_back(RunRelayRoute(clients, out));

	Report(results, out);

	bool allPassed = all_of(results.begin(), results.end(), [](const ScenarioResult& r) { return r.IsPassed(); });
	return allPassed ? 0 : 1;
}
