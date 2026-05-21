#include "pch.h"
#include <iostream>
#include "Service.h"
#include "ServerSession.h"

#include "Memory.h"
#include "ThreadManager.h"

#include "BufferReader.h"
#include "ServerPacketHandler.h"

// 전역 접속 카운터
atomic<int32> GConnectedCount = 0;

void RunStressTest(int32 clientCount);

int main()
{
	::SetConsoleOutputCP(CP_UTF8); // 한글 깨짐 방지 

	this_thread::sleep_for(6s); // 서버가 완전히 시작될 때까지 대기
	ServerPacketHandler::Init();

	this_thread::sleep_for(1s);
	
	// =====================================================
	// 옵션 1. 기존의 2인 시나리오 테스트
	// =====================================================
	bool runBasicTest = true;
	if (runBasicTest)
	{
		shared_ptr<ServerSession> player1Session;
		shared_ptr<ServerSession> player2Session;

		IocpCoreRef iocpCore = MakeShared<IocpCore>();

		ClientServiceRef service1 = MakeShared<ClientService>(
			NetAddress(L"127.0.0.1", 7777), MakeShared<IocpCore>(),
			[&]() {
				player1Session = MakeShared<ServerSession>("player1@test.com", "1234");
				return player1Session;
			}, 1);

		ClientServiceRef service2 = MakeShared<ClientService>(
			NetAddress(L"127.0.0.1", 7777), MakeShared<IocpCore>(),
			[&]() {
				player2Session = MakeShared<ServerSession>("player2@test.com", "1234");
				return player2Session;
			}, 1);

		ASSERT_CRASH(service1->Start());
		this_thread::sleep_for(300ms);
		ASSERT_CRASH(service2->Start());
		this_thread::sleep_for(300ms);

		// 각 서비스별 워커 스레드
		for (int32 i = 0; i < 2; i++)
		{
			GThreadManager->Launch([=]()
				{
					while (true)
					{
						service1->GetIocpCore()->Dispatch();
					}
				});
		}

		for (int32 i = 0; i < 2; i++)
		{
			GThreadManager->Launch([=]()
				{
					while (true)
					{
						service2->GetIocpCore()->Dispatch();
					}
				});
		}

		GThreadManager->Launch([&]()
			{
				cout << "\n========== Test Scenario Start ==========\n" << endl;

				// 0. 데이터 받아오기
				player1Session->SendGetDbDataPacket();

				// 1. 로그인 대기
				cout << "[Step 1] Waiting for login..." << endl;
				while (!player1Session || !player2Session ||
					!player1Session->IsLoginCompleted() ||
					!player2Session->IsLoginCompleted())
				{
					this_thread::sleep_for(100ms);
				}

				cout << "[Step 1] Both players logged in successfully!\n" << endl;

				// 뽑기
				this_thread::sleep_for(2s);
				player1Session->SendGacha();

				//// 2. Player1 방 생성
				//cout << "[Step 2 - 1] Player1 creating room..." << endl;
				//player1Session->SendCreateRoom();
				//this_thread::sleep_for(2s); // 방 생성 대기
				//cout << "[Step 2 - 2] Room created!\n" << endl;

				//// 3. Player1이 Player2 초대
				//cout << "[Step 3] Player1 inviting Player2..." << endl;
				//cout << "Target: " << player2Session->GetPlayerName()
				//	<< "#" << player2Session->GetPlayerTag() << endl;

				//player1Session->SendInvitePacket(
				//	player2Session->GetPlayerName(),
				//	player2Session->GetPlayerTag()
				//);
				//this_thread::sleep_for(2s); // 초대 처리 대기
				//cout << "[Step 3] Invitation success!\n" << endl;

				//cout << "[Step 4] Both players getting ready..." << endl;
				//player1Session->SendReady(true);
				//this_thread::sleep_for(500ms);
				//player2Session->SendReady(true);
				//this_thread::sleep_for(1s);
				//cout << "[Step 5] Both players are ready!\n" << endl;

				//// 6. 방장(Player1)이 게임 시작

				//this_thread::sleep_for(2s);
				//cout << "[Step 6] Player1 (room owner) starting game..." << endl;
				//player1Session->SendStartRoom();
				//this_thread::sleep_for(2s); // 게임 시작 대기
				//cout << "[Step 6] Game started!\n" << endl;

				//// 7. 둘다 같은 스테이지 정보 받는지 확인
				////cout << "[Step 7] Requesting stage info for both players..." << endl;
				////player1Session->SendShowStage(1, 1); // Stage 1-1
				////this_thread::sleep_for(500ms);
				////player2Session->SendShowStage(1, 1); // Stage 1-1
				////this_thread::sleep_for(2s);
				////cout << "[Step 7] Stage info received (check above logs)\n" << endl;


				////8. 호스트가 선택하는 맵이 게스트한테도 똑같이 보이는지 확인
				//this_thread::sleep_for(2s);

				//player1Session->SendHostStageSelect(1);

				//this_thread::sleep_for(2s);

				//player1Session->SendHostStageSelect(2);

				//this_thread::sleep_for(2s);
				//// 스테이지 클리어 정보
				//player1Session->SendStageClear(1, 3, 3);

				//cout << "\n========== Test Scenario Complete ==========\n" << endl;
			});

		GThreadManager->Join();
	}

	// =====================================================
	// 옵션 2. 100명 접속 스트레스 테스트 방출 (원할 때 주석 해제)
	// =====================================================
	//bool runStressTest = true;
	//if (runStressTest)
	//{
	//	RunStressTest(100);
	//}

}

// 뽑기 테스트

// =========================================================
// [스트레스 테스트] 100명의 더미 클라이언트를 생성하여 반복 행동 수행
// =========================================================
void RunStressTest(int32 clientCount)
{
	cout << "\n========== STRESS TEST START (" << clientCount << " Clients) ==========\n" << endl;

	IocpCoreRef iocpCore = MakeShared<IocpCore>();
	vector<shared_ptr<ServerSession>> sessions;

	// ClientService 하나에 clientCount만큼의 세션을 붙여서 동시 접속 시도
	ClientServiceRef service = MakeShared<ClientService>(
		NetAddress(L"127.0.0.1", 7777),
		iocpCore,
		[&]() {
			static atomic<int32> idGen = 1000;
			//string dummyEmail = "dummy" + to_string(idGen.fetch_add(1)) + "@test.com";
			string dummyEmail = "player1@test.com";

			// 이메일만 다르게 해서 세션 생성 (DB에 임시 가입되어 있다고 가정 혹은 자동가입 기능 필요)
			auto session = MakeShared<ServerSession>(dummyEmail, "1234");
			{
				// 멀티스레드 환경 대비 (지금은 메인 스레드에서 차례로 만들어지긴 합니다)
				sessions.push_back(session);
			}
			return session;
		},
		clientCount);

	ASSERT_CRASH(service->Start());

	// IOCP 워커 스레드 4개 생성 (100개 세션 커버용)
	for (int32 i = 0; i < 4; i++)
	{
		GThreadManager->Launch([service]()
			{
				while (true)
				{
					service->GetIocpCore()->Dispatch();
				}
			});
	}

	// 난사 봇(Bot) 스크립트 스레드 실행
	GThreadManager->Launch([=, sessions = std::move(sessions)]()
		{
			// 1. 서버에 다 붙고, 로그인 패킷 등 초기화가 완료될 때까지 대기
			this_thread::sleep_for(5s);
			cout << "[StressTest] All users spawned. Beginning relay bombardment!" << endl;

			int32 tickCount = 0;
			while (true)
			{
				// 초당 10번씩 전원 패킷 발송
				this_thread::sleep_for(100ms);
				tickCount++;

				for (auto& session : sessions)
				{
					if (session == nullptr) continue;

					// 예시: 릴레이 패킷 폭격 (이동/상호작용 흉내)
					Protocol::C_RELAY_PACKET relayPkt;
					relayPkt.set_require_host_authority(tickCount % 2 == 0); // 섞어서 테스트
					relayPkt.set_packet_id(1234);
					relayPkt.set_payload("stress_test_data");

					auto sendBuffer = ServerPacketHandler::MakeSendBuffer(relayPkt);
					session->Send(sendBuffer);
				}

				if (tickCount % 10 == 0) // 1초마다 생존 로그
				{
					cout << "  -> [StressTest] " << clientCount << " clients sent "
						<< (clientCount * 10) << " relay packets in the last 1 sec..." << endl;
				}
			}
		});
}
// =========================================================
