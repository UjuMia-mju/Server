#include "pch.h"
#include "RoomStressTest.h"
#include "StressTest.h"
#include "ServerSession.h"
#include "ServerPacketHandler.h"
#include "RelayStats.h"
#include "PacketStats.h"

// =========================================================
// 테스트 설정 (여기 값만 바꿔서 실행)
// =========================================================
static const int32 CLIENT_COUNT = 100;		// 접속 인원 (4의 배수, 최대 100)
static const int32 SEND_INTERVAL_MS = 50;	// 세션마다 릴레이 보내는 간격(ms), 50ms = 초당 20번
static const int32 PAYLOAD_SIZE = 64;		// 릴레이 payload 크기(byte), 앞 8byte는 보낸 시각
static const int32 DURATION_SEC = 60;		// 테스트 시간(초), 0 = 무한
static const bool VERBOSE = false;			// true면 패킷 핸들러의 cout 로그도 출력

static const int32 ROOM_SIZE = 4;
static const uint32 RELAY_PACKET_ID = 1;
static const auto ROOM_TIMEOUT = 10s;

static void RunRoomStressTest(int32 clientCount, int32 intervalMs, int32 payloadSize, int32 durationSec, bool verbose);

void RoomStressTest::Run()
{
	RunRoomStressTest(CLIENT_COUNT, SEND_INTERVAL_MS, PAYLOAD_SIZE, DURATION_SEC, VERBOSE);
}

// countFn() == target이 되거나 timeout까지 대기, 마지막 count 반환
template<typename Func>
static int32 WaitUntil(int32 target, chrono::seconds timeout, Func countFn, int64& outElapsedMs)
{
	auto start = chrono::steady_clock::now();
	int32 count = 0;
	while (true)
	{
		count = countFn();
		if (count >= target || chrono::steady_clock::now() - start > timeout)
			break;

		this_thread::sleep_for(50ms);
	}

	outElapsedMs = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start).count();
	return count;
}

// =========================================================
// [방 릴레이 스트레스 테스트]
// =========================================================
static void RunRoomStressTest(int32 clientCount, int32 intervalMs, int32 payloadSize, int32 durationSec, bool verbose)
{
	if (intervalMs < 1) intervalMs = 1;
	if (payloadSize < static_cast<int32>(sizeof(int64))) payloadSize = sizeof(int64);

	// 패킷 핸들러들의 cout 로그가 클라이언트를 느리게 만들기 때문에 꺼두고, 통계는 out으로만 출력
	streambuf* consoleBuf = cout.rdbuf();
	ostream out(consoleBuf);
	if (verbose == false)
		cout.rdbuf(nullptr);

	clientCount = StressTest::ClampClientCount(clientCount, out);
	clientCount -= clientCount % ROOM_SIZE;
	if (clientCount < ROOM_SIZE) clientCount = ROOM_SIZE;

	const int32 roomCount = clientCount / ROOM_SIZE;

	out << "\n========== ROOM RELAY STRESS TEST START ==========" << endl;
	out << "  Clients: " << clientCount << " (" << roomCount << " rooms x " << ROOM_SIZE << ") / Send interval: " << intervalMs
		<< "ms / Payload: " << payloadSize << "B / Duration: "
		<< (durationSec > 0 ? to_string(durationSec) + "s" : string("infinite")) << endl;

	// 1. 전원 접속 + 로그인 (sessions[i] = player(i+1))
	vector<shared_ptr<ServerSession>> sessions = StressTest::ConnectAndLogin(clientCount, out);

	// 2. 4명씩 그룹: player(4k-3) ~ player(4k-1)은 멤버, player(4k)가 방장
	struct RoomGroup
	{
		shared_ptr<ServerSession> owner;
		vector<shared_ptr<ServerSession>> members;
	};

	vector<RoomGroup> groups;
	for (int32 g = 0; g < roomCount; g++)
	{
		RoomGroup group;
		bool allLoggedIn = true;
		for (int32 j = 0; j < ROOM_SIZE; j++)
		{
			auto& session = sessions[g * ROOM_SIZE + j];
			if (session->IsLoginCompleted() == false)
				allLoggedIn = false;

			if (j == ROOM_SIZE - 1)
				group.owner = session;
			else
				group.members.push_back(session);
		}

		// 한 명이라도 로그인 실패한 그룹은 제외
		if (allLoggedIn)
			groups.push_back(group);
	}

	if (groups.size() < static_cast<size_t>(roomCount))
		out << "[RoomStress] " << (roomCount - groups.size()) << " groups skipped (login failed)" << endl;

	// 방장들이 방 생성
	for (auto& group : groups)
	{
		Protocol::C_CREATE_ROOM pkt;
		group.owner->Send(ServerPacketHandler::MakeSendBuffer(pkt));
	}

	int64 elapsedMs = 0;
	int32 created = WaitUntil(static_cast<int32>(groups.size()), ROOM_TIMEOUT, [&]()
		{
			int32 count = 0;
			for (auto& group : groups)
				if (group.owner->IsInRoom())
					count++;
			return count;
		}, elapsedMs);
	out << "[RoomStress] Rooms created " << created << " / " << groups.size() << " (" << elapsedMs << "ms)" << endl;

	// 3. 나머지 3명이 방장의 방에 입장
	int32 expectedMembers = 0;
	for (auto& group : groups)
	{
		if (group.owner->IsInRoom() == false)
			continue;

		for (auto& member : group.members)
		{
			Protocol::C_ENTER_ROOM pkt;
			pkt.set_room_id(group.owner->GetRoomId());
			member->Send(ServerPacketHandler::MakeSendBuffer(pkt));
			expectedMembers++;
		}
	}

	int32 entered = WaitUntil(expectedMembers, ROOM_TIMEOUT, [&]()
		{
			int32 count = 0;
			for (auto& group : groups)
				for (auto& member : group.members)
					if (member->IsInRoom())
						count++;
			return count;
		}, elapsedMs);
	out << "[RoomStress] Members entered " << entered << " / " << expectedMembers << " (" << elapsedMs << "ms)" << endl;

	// 실제로 방에 들어간 세션만 릴레이 전송 (실제 클라이언트처럼 방장은 나머지 전원에게, 멤버는 방장에게만)
	// 방 인원이 n명이면 tick당 기대 수신 = 방장 1회 x (n-1)명 + 멤버 (n-1)회 x 방장 1명 = 2 * (n-1)
	vector<shared_ptr<ServerSession>> active;
	set<shared_ptr<ServerSession>> owners;
	int64 expectedRecvPerTick = 0;
	int32 fullRooms = 0;
	for (auto& group : groups)
	{
		if (group.owner->IsInRoom() == false)
			continue;

		int64 inRoom = 1;
		active.push_back(group.owner);
		owners.insert(group.owner);
		for (auto& member : group.members)
		{
			if (member->IsInRoom())
			{
				active.push_back(member);
				inRoom++;
			}
		}

		expectedRecvPerTick += 2 * (inRoom - 1);
		if (inRoom == ROOM_SIZE)
			fullRooms++;
	}

	out << "[RoomStress] Full rooms " << fullRooms << " / " << roomCount << ", sending clients " << active.size() << endl;

	if (expectedRecvPerTick == 0)
	{
		out << "[RoomStress] No room with 2+ players, exiting." << endl;
		out.flush();
		::ExitProcess(1);
	}

	// 서버에서 EnterLobby가 DoAsync로 처리되므로 방 멤버 등록이 끝나도록 잠깐 대기
	this_thread::sleep_for(500ms);
	RelayStats::Reset();

	// 4. 릴레이 패킷 반복 전송
	out << "[RoomStress] Relay start (C_RELAY_PACKET: owner -> other " << (ROOM_SIZE - 1) << " members, member -> owner)" << endl;

	string payload(payloadSize, 'x');
	vector<int64> allLatency;
	int64 totalSent = 0;
	int64 expectedRecv = 0;
	int64 sentThisSec = 0;
	int64 lastRecv = 0;

	auto testStart = chrono::steady_clock::now();
	auto lastReport = testStart;
	PacketStats::TakeRate();	// 이전 단계에서 보낸 개수는 버림
	auto nextTick = testStart;

	while (true)
	{
		nextTick += chrono::milliseconds(intervalMs);

		for (auto& session : active)
		{
			// payload 앞 8byte = 보낸 시각(us)
			RelayStats::StampSendTime(payload);

			Protocol::C_RELAY_PACKET pkt;
			pkt.set_packet_id(RELAY_PACKET_ID);
			// 방장: false = 보낸 사람 제외 방 전체에게 / 멤버: true = 방장에게만 (서버는 멤버가 false를 보내도 방장에게만 전달)
			pkt.set_require_host_authority(owners.count(session) == 0);
			pkt.set_payload(payload);
			session->Send(ServerPacketHandler::MakeSendBuffer(pkt));
		}
		sentThisSec += active.size();
		totalSent += active.size();
		expectedRecv += expectedRecvPerTick;

		auto now = chrono::steady_clock::now();

		// 1초마다 통계 출력
		if (now - lastReport >= 1s)
		{
			int64 recv = RelayStats::RecvCount();
			int64 elapsed = chrono::duration_cast<chrono::seconds>(now - testStart).count();
			vector<int64> samples = RelayStats::TakeSamples();

			out << "  [" << elapsed << "s] Sent " << sentThisSec << " pkt/s, Recv " << (recv - lastRecv)
				<< " pkt/s | Latency " << RelayStats::FormatLatency(samples) << endl;

			out << "        " << PacketStats::TakeRate() << endl;

			allLatency.insert(allLatency.end(), samples.begin(), samples.end());
			sentThisSec = 0;
			lastRecv = recv;
			lastReport = now;
		}

		if (durationSec > 0 && now - testStart >= chrono::seconds(durationSec))
			break;

		this_thread::sleep_until(nextTick);
	}

	// 마지막 응답이 도착할 시간을 조금 준다
	this_thread::sleep_for(2s);

	vector<int64> rest = RelayStats::TakeSamples();
	allLatency.insert(allLatency.end(), rest.begin(), rest.end());
	int64 totalRecv = RelayStats::RecvCount();

	out << "\n========== ROOM RELAY STRESS TEST END ==========" << endl;
	out << "  Rooms: " << fullRooms << " full / " << roomCount << ", sending clients: " << active.size() << endl;
	out << "  Total sent: " << totalSent << " / Total recv: " << totalRecv << " (expected " << expectedRecv << ")" << endl;
	out << "  Latency (all): " << RelayStats::FormatLatency(allLatency) << endl;
	if (totalRecv < expectedRecv)
		out << "  * Received " << (expectedRecv - totalRecv) << " fewer relay packets than expected." << endl;
	PacketStats::Report(out);
	out.flush();

	// IOCP 워커 스레드가 무한 루프라 프로세스를 바로 종료
	::ExitProcess(0);
}
// =========================================================
