#include "pch.h"
#include "StressTest.h"
#include "Service.h"
#include "ThreadManager.h"
#include "ServerSession.h"
#include "ServerPacketHandler.h"

// =========================================================
// 테스트 설정 (여기 값만 바꿔서 실행)
// =========================================================
static const int32 CLIENT_COUNT = 100;	// 접속 인원 (최대 100, DB 계정 수)
static const int32 INTERVAL_MS = 100;	// 세션마다 패킷 보내는 간격(ms)
static const int32 DURATION_SEC = 60;	// 테스트 시간(초), 0 = 무한
static const bool VERBOSE = false;		// true면 패킷 핸들러의 cout 로그도 출력

static void RunStressTest(int32 clientCount, int32 intervalMs, int32 durationSec, bool verbose);

void StressTest::Run()
{
	RunStressTest(CLIENT_COUNT, INTERVAL_MS, DURATION_SEC, VERBOSE);
}

// =========================================================
// [공용] clientCount명 동시 접속 + 로그인 완료까지 대기
//   반환 vector의 i번째 = player(i+1)@test.com 세션 (로그인 실패 세션도 포함)
// =========================================================
vector<shared_ptr<ServerSession>> StressTest::ConnectAndLogin(int32 clientCount, ostream& out)
{
	// DB 계정: player1@test.com ~ player100@test.com / 1234
	const string PASSWORD = "1234";
	const auto LOGIN_TIMEOUT = 30s;

	clientCount = ClampClientCount(clientCount, out);

	mutex sessionLock;
	vector<shared_ptr<ServerSession>> sessions;

	// ClientService 하나에 clientCount만큼 세션을 만들어 동시 접속 (세션마다 다른 계정)
	// Start()가 세션을 순서대로 만들기 때문에 sessions[i]는 player(i+1) 계정
	ClientServiceRef service = MakeShared<ClientService>(
		NetAddress(L"127.0.0.1", 7777),
		MakeShared<IocpCore>(),
		[&]() {
			lock_guard<mutex> guard(sessionLock);
			string email = "player" + to_string(sessions.size() + 1) + "@test.com";
			auto session = MakeShared<ServerSession>(email, PASSWORD);
			sessions.push_back(session);
			return session;
		},
		clientCount);

	// IOCP 워커 스레드 (OnConnected 안에서 200ms 대기하므로 넉넉하게)
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

	ASSERT_CRASH(service->Start());

	// 로그인 완료 대기 (접속하면 OnConnected에서 C_LOGIN 자동 전송)
	auto loginStart = chrono::steady_clock::now();
	int32 loggedIn = 0;
	while (true)
	{
		{
			lock_guard<mutex> guard(sessionLock);
			loggedIn = 0;
			for (auto& session : sessions)
				if (session->IsLoginCompleted())
					loggedIn++;
		}

		if (loggedIn == clientCount || chrono::steady_clock::now() - loginStart > LOGIN_TIMEOUT)
			break;

		this_thread::sleep_for(100ms);
	}

	auto loginMs = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - loginStart).count();
	out << "[StressTest] Login " << loggedIn << " / " << clientCount << " succeeded (" << loginMs << "ms)" << endl;

	if (loggedIn == 0)
	{
		out << "[StressTest] No session logged in, exiting. Check the server/DB status." << endl;
		out.flush();
		::ExitProcess(1);
	}

	lock_guard<mutex> guard(sessionLock);
	return sessions;
}

int32 StressTest::ClampClientCount(int32 clientCount, ostream& out)
{
	if (clientCount < 1)
		return 1;

	if (clientCount > MAX_ACCOUNT)
	{
		out << "[StressTest] Only " << MAX_ACCOUNT << " DB accounts exist, limiting clients to " << MAX_ACCOUNT << "." << endl;
		return MAX_ACCOUNT;
	}

	return clientCount;
}

// =========================================================
// [스트레스 테스트] 더미 클라이언트 N명이 로그인 후 패킷을 반복 전송
// =========================================================
static void RunStressTest(int32 clientCount, int32 intervalMs, int32 durationSec, bool verbose)
{
	if (intervalMs < 1) intervalMs = 1;

	// 패킷 핸들러들의 cout 로그가 클라이언트를 느리게 만들기 때문에 꺼두고, 통계는 out으로만 출력
	streambuf* consoleBuf = cout.rdbuf();
	ostream out(consoleBuf);
	if (verbose == false)
		cout.rdbuf(nullptr);

	clientCount = StressTest::ClampClientCount(clientCount, out);

	out << "\n========== STRESS TEST START ==========" << endl;
	out << "  Clients: " << clientCount << " / Send interval: " << intervalMs << "ms / Duration: "
		<< (durationSec > 0 ? to_string(durationSec) + "s" : string("infinite")) << endl;

	// 1. 접속 + 로그인
	vector<shared_ptr<ServerSession>> sessions = StressTest::ConnectAndLogin(clientCount, out);

	// 로그인 성공한 세션만 테스트에 사용
	vector<shared_ptr<ServerSession>> active;
	for (auto& session : sessions)
		if (session->IsLoginCompleted())
			active.push_back(session);
	int32 loggedIn = static_cast<int32>(active.size());

	// 2. 로그인 후 패킷 반복 전송
	//    방이 없어도 처리되는 로비 패킷 위주 (C_RELAY_PACKET, C_CHAT 등은 방이 있어야 해서 제외)
	//    C_GACHA는 실제 DB의 재화를 소모하므로 넣지 않음
	const char* packetNames[] = { "GET_CLEAR_INFO(DB)", "GET_CURRENCY(DB)", "MY_SKINS", "GACHA_POOL_LIST", "SHOW_STAGE" };
	const int32 PACKET_TYPE_COUNT = 5;

	auto makePacket = [](int32 type) -> SendBufferRef
		{
			switch (type)
			{
			case 0: { Protocol::C_GET_CLEAR_INFO pkt; return ServerPacketHandler::MakeSendBuffer(pkt); }
			case 1: { Protocol::C_GET_CURRENCY pkt; return ServerPacketHandler::MakeSendBuffer(pkt); }
			case 2: { Protocol::C_MY_SKINS pkt; return ServerPacketHandler::MakeSendBuffer(pkt); }
			case 3: { Protocol::C_GACHA_POOL_LIST pkt; return ServerPacketHandler::MakeSendBuffer(pkt); }
			default:
			{
				Protocol::C_SHOW_STAGE pkt;
				pkt.set_map_id(1);
				pkt.set_chapter(1);
				pkt.set_stageindex(1);
				return ServerPacketHandler::MakeSendBuffer(pkt);
			}
			}
		};

	out << "[StressTest] Start sending packets: ";
	for (int32 i = 0; i < PACKET_TYPE_COUNT; i++)
		out << packetNames[i] << (i + 1 < PACKET_TYPE_COUNT ? ", " : "\n");

	GRecvPacketCount.store(0);
	int64 totalSent = 0;
	int64 sentThisSec = 0;
	int64 lastRecv = 0;
	int64 tick = 0;

	auto testStart = chrono::steady_clock::now();
	auto lastReport = testStart;
	auto nextTick = testStart;

	while (true)
	{
		nextTick += chrono::milliseconds(intervalMs);

		// 세션마다 다른 종류의 패킷을 돌아가며 전송
		for (size_t i = 0; i < active.size(); i++)
		{
			int32 type = static_cast<int32>((tick + i) % PACKET_TYPE_COUNT);
			active[i]->Send(makePacket(type));
		}
		sentThisSec += active.size();
		totalSent += active.size();
		tick++;

		auto now = chrono::steady_clock::now();

		// 1초마다 통계 출력
		if (now - lastReport >= 1s)
		{
			int64 recv = GRecvPacketCount.load();
			int64 elapsed = chrono::duration_cast<chrono::seconds>(now - testStart).count();
			out << "  [" << elapsed << "s] Sent " << sentThisSec << " pkt/s, Recv " << (recv - lastRecv)
				<< " pkt/s (Total sent " << totalSent << " / recv " << recv << ")" << endl;

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

	int64 totalRecv = GRecvPacketCount.load();
	out << "\n========== STRESS TEST END ==========" << endl;
	out << "  Login: " << loggedIn << " / " << clientCount << endl;
	out << "  Total sent: " << totalSent << " / Total recv: " << totalRecv << endl;
	if (totalRecv < totalSent)
		out << "  * Received " << (totalSent - totalRecv) << " fewer packets than sent. Check if the server is falling behind or some packets have no response." << endl;
	out.flush();

	// IOCP 워커 스레드가 무한 루프라 프로세스를 바로 종료
	::ExitProcess(0);
}
// =========================================================
