#include "pch.h"
#include "GameFlowTest.h"
#include "InviteFlowTest.h"
#include "RelayStats.h"
#include "PacketStats.h"
#include "ServerPacketHandler.h"
#include <random>

using namespace TestPhase;

// =========================================================
// 테스트 설정 (여기 값만 바꿔서 실행)
// =========================================================
static const int32 CLIENT_COUNT = 100;			// 접속 인원 (최대 100, DB 계정 수)
static const int32 GROUP_SIZE = 4;				// 방 하나 인원 (호스트 포함)
static const int32 BROWSE_MIN = 1;				// 스테이지 선택 화면에서 방장이 둘러보는 스테이지 수 (C_SHOW_STAGE, min ~ max 랜덤)
static const int32 BROWSE_MAX = 5;
static const int32 THINK_MIN_MS = 500;			// 다음 스테이지로 넘기기까지 생각 시간 (min ~ max 랜덤)
static const int32 THINK_MAX_MS = 2000;
static const int32 RELAY_MIN_PER_SEC = 10;		// 클라이언트 한 명이 초당 보내는 릴레이 수 (min ~ max 랜덤, 1초마다 다시 뽑음)
static const int32 RELAY_MAX_PER_SEC = 20;
static const int32 RELAY_PAYLOAD_SIZE = 64;		// 릴레이 payload 크기(byte), 앞 8byte는 보낸 시각
static const int32 RELAY_DURATION_SEC = 30;		// 릴레이 시간(초)
static const bool WAIT_ENTER_BEFORE_GAME = true;	// true면 방 입장 후 엔터 쳐야 Ready부터 진행
static const bool VERBOSE = false;				// true면 패킷 핸들러의 cout 로그도 출력

static const uint32 RELAY_PACKET_ID = 1;

struct RelayResult
{
	int32 senders = 0;
	int64 sent = 0;
	int64 recv = 0;
	int64 expected = 0;
	vector<int64> latencyUs;
};

// =========================================================
// 단계별 action / 완료 조건
// =========================================================
static void ReadyAll(const Group& group)
{
	for (auto& member : group.members)
		member->SendReady(true);
}

// S_READY는 방 전체에 브로드캐스트되므로 호스트가 전원분을 받았으면 서버에도 전원 레디 반영 완료
static bool IsAllReady(const Group& group)
{
	return group.Host()->GetReadyCount() >= static_cast<int32>(group.members.size());
}

static void StartRoom(const Group& group) { group.Host()->SendStartRoom(); }

// S_START_ROOM은 방 전체에 브로드캐스트 -> 전원이 받으면 스테이지 선택 화면 진입 완료
static bool IsAllRoomStarted(const Group& group)
{
	for (auto& member : group.members)
		if (member->IsRoomStarted() == false)
			return false;

	return true;
}

// 스테이지 선택 화면에서 고를 목록 (C_GET_DB_DATA -> S_STAGE_INFO)
static void LoadStageList(const Group& group)
{
	for (auto& member : group.members)
		member->SendGetDbDataPacket();
}

static bool IsAllStageListLoaded(const Group& group)
{
	for (auto& member : group.members)
		if (member->HasStageList() == false || member->GetStageList().empty())
			return false;

	return true;
}

// 방장이 S_HOST_SHOW_STAGE로 공유한 스테이지를 방장 목록에서 찾음
static StageKey FindHostStage(const Client& host)
{
	int32 mapId = host->GetHostStage();
	for (auto& stage : host->GetStageList())
		if (stage.mapId == mapId)
			return stage;

	return StageKey{};
}

// 방장이 고른 스테이지를 방 전원이 받았는지
static bool IsStageChosen(const Group& group)
{
	int32 mapId = group.Host()->GetHostStage();
	if (mapId == 0)
		return false;

	return all_of(group.members.begin(), group.members.end(),
		[&](const Client& member) { return member->GetHostStage() == mapId; });
}

// 방장 C_START_STAGE -> 전원 S_START_STAGE -> (자동) C_ENTER_GAME
static void StartStage(const Group& group)
{
	StageKey stage = FindHostStage(group.Host());
	group.Host()->SendStartStage(stage.mapId, stage.chapter, stage.stage);
}

static bool IsAllInGame(const Group& group)
{
	for (auto& member : group.members)
		if (member->IsGameReady() == false)
			return false;

	return true;
}

// =========================================================
// [StageSelect] 스테이지 선택 화면
//   - 방장만 생각 시간(THINK_MIN ~ MAX)을 두고 랜덤 스테이지를 C_SHOW_STAGE로 BROWSE_MIN ~ MAX번 확인
//   - 다 둘러본 뒤 생각 시간을 한 번 더 두고 하나 골라서 C_HOST_SHOW_STAGE (방 전체 공유)
//   - 게스트는 요청 없이 방장이 고른 스테이지(S_HOST_SHOW_STAGE)를 받기만 함
//   - 결과 2개
//       ShowStage : C_SHOW_STAGE ~ S_SHOW_STAGE 응답 (대상 = 요청 수)
//       HostShow  : 방장 C_HOST_SHOW_STAGE ~ 방 전원 S_HOST_SHOW_STAGE 수신 (대상 = 방 수)
// =========================================================
static vector<PhaseResult> RunStageSelect(const vector<Group>& groups, ostream& out)
{
	struct Browser
	{
		Client client;
		size_t groupIndex = 0;
		int32 browseCount = 0;	// 둘러볼 스테이지 수
		int32 sent = 0;			// 보낸 C_SHOW_STAGE 수
		int64 sentUs = 0;		// 마지막 C_SHOW_STAGE 보낸 시각 (0 = 응답 대기 중 아님)
		int64 nextUs = 0;		// 다음 행동 시각 (생각 시간 끝)
		bool picked = false;	// C_HOST_SHOW_STAGE 보냈는지
	};

	mt19937 rng(random_device{}());
	uniform_int_distribution<int32> thinkMs(THINK_MIN_MS, THINK_MAX_MS);
	auto thinkUs = [&]() { return static_cast<int64>(thinkMs(rng)) * 1000; };
	auto pickStage = [&](const Client& client)
		{
			vector<StageKey> stages = client->GetStageList();
			return stages[rng() % stages.size()];
		};

	PhaseResult showStage;
	showStage.name = "ShowStage";
	PhaseResult hostShow;
	hostShow.name = "HostShow";
	hostShow.total = static_cast<int32>(groups.size());

	vector<int32> chosenMapId(groups.size(), 0);
	vector<int64> pickedUs(groups.size(), 0);
	vector<bool> groupDone(groups.size(), false);

	int64 startUs = NowUs();
	vector<Browser> browsers;	// 방마다 방장 1명
	for (size_t g = 0; g < groups.size(); g++)
	{
		Browser browser;
		browser.client = groups[g].Host();
		browser.groupIndex = g;
		browser.browseCount = uniform_int_distribution<int32>(BROWSE_MIN, BROWSE_MAX)(rng);
		browser.nextUs = startUs + thinkUs();
		browsers.push_back(browser);
		showStage.total += browser.browseCount;
	}

	auto start = chrono::steady_clock::now();
	while (hostShow.success < hostShow.total && chrono::steady_clock::now() - start < PHASE_TIMEOUT)
	{
		int64 now = NowUs();

		for (auto& browser : browsers)
		{
			// 응답 도착 확인 (생각 시간이 응답보다 훨씬 길어서 한 명당 대기 중인 요청은 최대 1개)
			if (browser.sentUs != 0 && browser.client->GetShowStageCount() >= browser.sent)
			{
				showStage.success++;
				showStage.latencyUs.push_back(now - browser.sentUs);
				browser.sentUs = 0;
			}

			if (now < browser.nextUs)
				continue;

			if (browser.sent < browser.browseCount)
			{
				StageKey stage = pickStage(browser.client);
				browser.sentUs = NowUs();
				browser.client->SendStageData(stage.mapId, stage.chapter, stage.stage);
				browser.sent++;
				browser.nextUs = now + thinkUs();
			}
			else if (browser.picked == false)
			{
				StageKey stage = pickStage(browser.client);
				chosenMapId[browser.groupIndex] = stage.mapId;
				pickedUs[browser.groupIndex] = NowUs();
				browser.client->SendHostStageSelect(stage.mapId);
				browser.picked = true;
			}
		}

		// 방장이 고른 스테이지를 방 전원이 받았으면 그 방은 선택 완료
		for (size_t g = 0; g < groups.size(); g++)
		{
			if (groupDone[g] || chosenMapId[g] == 0 || IsStageChosen(groups[g]) == false)
				continue;

			groupDone[g] = true;
			hostShow.success++;
			hostShow.latencyUs.push_back(now - pickedUs[g]);
		}

		this_thread::sleep_for(10ms);
	}

	int64 elapsedMs = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start).count();
	showStage.elapsedMs = elapsedMs;
	hostShow.elapsedMs = elapsedMs;

	out << "[Phase] ShowStage " << showStage.success << " / " << showStage.total << " (" << elapsedMs << "ms)" << endl;
	out << "[Phase] HostShow " << hostShow.success << " / " << hostShow.total << " (" << elapsedMs << "ms)" << endl;
	return { showStage, hostShow };
}

// =========================================================
// [Relay] rooms 전원이 릴레이 전송, 1초마다 통계 출력
//   - 클라이언트마다 초당 전송 수를 minPerSec ~ maxPerSec에서 뽑고, 1초마다 다시 뽑음
//   - 전송 간격도 매번 ±50% 흔들고, 첫 전송 시점도 클라이언트마다 다르게 (한 순간에 몰리지 않게)
// =========================================================
static RelayResult RunRelay(const vector<Group>& rooms, int32 minPerSec, int32 maxPerSec, int32 payloadSize, int32 durationSec, ostream& out)
{
	struct Sender
	{
		Client client;
		int64 receivers = 0;	// 한 번 보내면 받는 사람 수 (방 인원 - 1)
		int32 perSec = 0;		// 이번 1초 동안의 초당 전송 수
		int64 nextUs = 0;		// 다음 전송 시각
	};

	if (minPerSec < 1) minPerSec = 1;
	if (maxPerSec < minPerSec) maxPerSec = minPerSec;
	if (payloadSize < static_cast<int32>(sizeof(int64))) payloadSize = sizeof(int64);

	mt19937 rng(random_device{}());
	uniform_int_distribution<int32> rateDist(minPerSec, maxPerSec);
	uniform_real_distribution<double> jitter(0.5, 1.5);
	auto intervalUs = [&](int32 perSec) { return static_cast<int64>(1'000'000.0 / perSec * jitter(rng)); };

	RelayResult result;
	vector<Sender> senders;
	int64 startUs = NowUs();
	for (auto& room : rooms)
	{
		for (auto& member : room.members)
		{
			Sender sender;
			sender.client = member;
			sender.receivers = static_cast<int64>(room.members.size()) - 1;
			sender.perSec = rateDist(rng);
			sender.nextUs = startUs + uniform_int_distribution<int64>(0, 1'000'000 / sender.perSec)(rng);
			senders.push_back(sender);
		}
	}
	result.senders = static_cast<int32>(senders.size());

	out << "\n[Relay] " << result.senders << " clients x " << minPerSec << "~" << maxPerSec << "/s (random per client, re-rolled every second) for "
		<< durationSec << "s (payload " << payloadSize << "B)" << endl;

	RelayStats::Reset();
	string payload(payloadSize, 'x');
	int64 sentThisSec = 0;
	int64 lastRecv = 0;

	auto start = chrono::steady_clock::now();
	auto lastReport = start;
	PacketStats::TakeRate();	// 이전 단계에서 보낸 개수는 버림

	while (true)
	{
		int64 nowUs = NowUs();
		for (auto& sender : senders)
		{
			// 잠깐 늦게 깨어났으면 밀린 만큼 보내서 평균 전송 수를 맞춤
			while (sender.nextUs <= nowUs)
			{
				RelayStats::StampSendTime(payload);

				Protocol::C_RELAY_PACKET pkt;
				pkt.set_packet_id(RELAY_PACKET_ID);
				pkt.set_require_host_authority(false); // 보낸 사람 제외 방 전체에게
				pkt.set_payload(payload);
				sender.client->Send(ServerPacketHandler::MakeSendBuffer(pkt));

				sentThisSec++;
				result.sent++;
				result.expected += sender.receivers;
				sender.nextUs += intervalUs(sender.perSec);
			}
		}

		auto now = chrono::steady_clock::now();
		if (now - lastReport >= 1s)
		{
			int64 recv = RelayStats::RecvCount();
			int64 elapsed = chrono::duration_cast<chrono::seconds>(now - start).count();
			vector<int64> samples = RelayStats::TakeSamples();

			out << "  [" << elapsed << "s] Sent " << sentThisSec << " pkt/s, Recv " << (recv - lastRecv)
				<< " pkt/s | Latency " << RelayStats::FormatLatency(samples) << endl;

			out << "        " << PacketStats::TakeRate() << endl;

			result.latencyUs.insert(result.latencyUs.end(), samples.begin(), samples.end());
			sentThisSec = 0;
			lastRecv = recv;
			lastReport = now;

			// 다음 1초 동안의 초당 전송 수를 클라이언트마다 다시 뽑음
			for (auto& sender : senders)
				sender.perSec = rateDist(rng);
		}

		if (now - start >= chrono::seconds(durationSec))
			break;

		this_thread::sleep_for(1ms);
	}

	// 마지막 패킷이 도착할 시간을 조금 준다
	this_thread::sleep_for(2s);

	vector<int64> rest = RelayStats::TakeSamples();
	result.latencyUs.insert(result.latencyUs.end(), rest.begin(), rest.end());
	result.recv = RelayStats::RecvCount();
	return result;
}

static void ReportRelay(RelayResult& relay, ostream& out)
{
	out << "\n========== RELAY REPORT ==========" << endl;
	out << "  Senders: " << relay.senders << endl;
	out << "  Total sent: " << relay.sent << " / Total recv: " << relay.recv << " (expected " << relay.expected << ")" << endl;
	out << "  Latency (all): " << RelayStats::FormatLatency(relay.latencyUs) << endl;
	if (relay.recv < relay.expected)
		out << "  * Received " << (relay.expected - relay.recv) << " fewer relay packets than expected." << endl;
}

// =========================================================
// [전체 흐름 테스트]
// =========================================================
void GameFlowTest::Run()
{
	// 패킷 핸들러들의 cout 로그가 클라이언트를 느리게 만들기 때문에 꺼두고, 통계는 out으로만 출력
	streambuf* consoleBuf = cout.rdbuf();
	ostream out(consoleBuf);
	if (VERBOSE == false)
		cout.rdbuf(nullptr);

	out << "\n========== GAME FLOW TEST START ==========" << endl;
	out << "  Clients: " << CLIENT_COUNT << " / Group size: " << GROUP_SIZE << " / Relay: " << RELAY_MIN_PER_SEC << "~" << RELAY_MAX_PER_SEC
		<< "/s x " << RELAY_DURATION_SEC << "s" << endl;

	// 1~3. 접속 -> Login -> Create -> Join
	vector<PhaseResult> results;
	int32 allGroupCount = 0;
	auto rooms = InviteFlowTest::SetupRooms(CLIENT_COUNT, GROUP_SIZE, results, allGroupCount, out);

	if (WAIT_ENTER_BEFORE_GAME)
		WaitForEnter("[GameFlow] " + to_string(rooms.size()) + " rooms ready. Press Enter to start Ready -> Start -> StageSelect -> StageStart -> Relay...", out);

	// 4. Ready
	results.push_back(RunPhase("Ready", rooms, ReadyAll, IsAllReady, out));
	auto readyRooms = FilterGroups(rooms, IsAllReady);

	// 5. Start (스테이지 선택 화면 진입)
	results.push_back(RunPhase("Start", readyRooms, StartRoom, IsAllRoomStarted, out));
	auto startedRooms = FilterGroups(readyRooms, IsAllRoomStarted);

	// 6. StageList -> StageSelect (ShowStage, HostShow)
	results.push_back(RunPhase("StageList", startedRooms, LoadStageList, IsAllStageListLoaded, out));
	auto listedRooms = FilterGroups(startedRooms, IsAllStageListLoaded);

	vector<PhaseResult> selectResults = RunStageSelect(listedRooms, out);
	results.insert(results.end(), selectResults.begin(), selectResults.end());
	auto chosenRooms = FilterGroups(listedRooms, IsStageChosen);

	// 7. StageStart (C_START_STAGE -> C_ENTER_GAME -> S_GAME_READY_TO_START)
	results.push_back(RunPhase("StageStart", chosenRooms, StartStage, IsAllInGame, out));
	auto playingRooms = FilterGroups(chosenRooms, IsAllInGame);

	// 인게임 시작 전 (Login ~ StageStart) 요청 개수를 찍고, 인게임 구간은 이 시점부터 따로 집계
	PacketStats::Report(out, "PRE-GAME READ / WRITE (Login ~ StageStart)");
	PacketStats::Snapshot preGame = PacketStats::TakeSnapshot();

	// 8. Relay
	RelayResult relay;
	if (playingRooms.empty())
		out << "[GameFlow] No room started the game, skipping relay." << endl;
	else
		relay = RunRelay(playingRooms, RELAY_MIN_PER_SEC, RELAY_MAX_PER_SEC, RELAY_PAYLOAD_SIZE, RELAY_DURATION_SEC, out);

	Report(results, out);
	out << "  (ShowStage is per request, the others are per group)" << endl;
	ReportRelay(relay, out);
	PacketStats::Report(out, "IN-GAME READ / WRITE (Relay)", &preGame);
	PacketStats::Report(out, "TOTAL READ / WRITE (Login ~ Relay)");

	// 바로 종료하면 콘솔 창이 닫혀서 리포트를 못 보므로 엔터 입력까지 대기
	WaitForEnter("Press Enter to exit...", out);

	bool success = static_cast<int32>(playingRooms.size()) == allGroupCount && relay.recv >= relay.expected;

	// IOCP 워커 스레드가 무한 루프라 프로세스를 바로 종료
	::ExitProcess(success ? 0 : 1);
}
// =========================================================
