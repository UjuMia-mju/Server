#include "pch.h"
#include <iostream>
#include "Service.h"
#include "ServerSession.h"

#include "Memory.h"
#include "ThreadManager.h"

#include "BufferReader.h"
#include "ServerPacketHandler.h"

int main()
{
	::SetConsoleOutputCP(CP_UTF8); // 한글 깨짐 방지 

	this_thread::sleep_for(6s); // 서버가 완전히 시작될 때까지 대기
	ServerPacketHandler::Init();

	this_thread::sleep_for(1s);
	
	shared_ptr<ServerSession> player1Session;
	shared_ptr<ServerSession> player2Session;

	// 공유 IOCP
	IocpCoreRef iocpCore = MakeShared<IocpCore>();

	// Player1 세션 생성
	ClientServiceRef service1 = MakeShared<ClientService>(
		NetAddress(L"127.0.0.1", 7777),
		MakeShared<IocpCore>(),
		[&]() {
			player1Session = MakeShared<ServerSession>("player1@test.com", "1234");
			return player1Session;
		},
		1);

	// Player2 세션 생성
	ClientServiceRef service2 = MakeShared<ClientService>(
		NetAddress(L"127.0.0.1", 7777),
		MakeShared<IocpCore>(),
		[&]() {
			player2Session = MakeShared<ServerSession>("player2@test.com", "1234");
			return player2Session;
		},
		1);

	ASSERT_CRASH(service1->Start());
	this_thread::sleep_for(300ms);
	ASSERT_CRASH(service2->Start());
	this_thread::sleep_for(300ms);

	// ====== 기존 코드 ======
	//ClientServiceRef service = MakeShared<ClientService>(
	//	NetAddress(L"127.0.0.1", 7777),
	//	MakeShared<IocpCore>(),
	//	MakeShared<ServerSession>,
	//	1);
	//ASSERT_CRASH(service->Start());

	//for (int32 i = 0; i < 2; i++)
	//{
	//	GThreadManager->Launch([=]()
	//		{
	//			while (true)
	//			{
	//				service->GetIocpCore()->Dispatch();
	//			}
	//		});
	//}
	// =====================

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

	// 가차 시나리오
	//GThreadManager->Launch([&]()
	//	{
	//		cout << "\n========== Test Scenario Start ==========\n" << endl;

	//		// 1. 로그인 대기
	//		cout << "[Step 1] Waiting for login..." << endl;
	//		while (!player1Session || !player1Session->IsLoginCompleted())
	//		{
	//			this_thread::sleep_for(100ms);
	//		}

	//		cout << "[Step 1] Player1 logged in successfully!\n" << endl;
	//		// Player1이 로그인 완료 후 뽑기 패킷 보냄.
	//		
	//		this_thread::sleep_for(1s); // 로그인 처리 후 잠시 대기
	//		cout << "[Step 2] Player1 sending gacha pull request..." << endl;
	//		Protocol::C_GACHA gachaPkt;
	//		gachaPkt.set_pool_id(1);
	//		gachaPkt.set_pull_count(1);
	//		auto sendBuffer = ServerPacketHandler::MakeSendBuffer(gachaPkt);
	//		player1Session->Send(sendBuffer);

	//		cout << "[Step 2] Show Owned skins\n" << endl;
	//		Protocol::C_MY_SKINS showSkinsPkt;
	//		sendBuffer = ServerPacketHandler::MakeSendBuffer(showSkinsPkt);
	//		player1Session->Send(sendBuffer);

	//		cout << "\n========== Test Scenario Complete ==========\n" << endl;
	//	});

	//테스트 시나리오 실행 (초대, 수락, 시작)
	GThreadManager->Launch([&]()
		{
			cout << "\n========== Test Scenario Start ==========\n" << endl;

			// 1. 로그인 대기
			cout << "[Step 1] Waiting for login..." << endl;
			while (!player1Session || !player2Session ||
				!player1Session->IsLoginCompleted() ||
				!player2Session->IsLoginCompleted())
			{
				this_thread::sleep_for(100ms);
			}
			
			cout << "[Step 1] Both players logged in successfully!\n" << endl;

			// 2. Player1 방 생성
			cout << "[Step 2 - 1] Player1 creating room..." << endl;
			player1Session->SendCreateRoom();
			this_thread::sleep_for(2s); // 방 생성 대기
			cout << "[Step 2 - 2] Room created!\n" << endl;

			// 3. Player1이 Player2 초대
			cout << "[Step 3] Player1 inviting Player2..." << endl;
			cout << "Target: " << player2Session->GetPlayerName()
				<< "#" << player2Session->GetPlayerTag() << endl;

			player1Session->SendInvitePacket(
				player2Session->GetPlayerName(),
				player2Session->GetPlayerTag()
			);
			this_thread::sleep_for(2s); // 초대 처리 대기
			cout << "[Step 3] Invitation success!\n" << endl;

			cout << "[Step 4] 방 참여자 전체 목록 보기\n" << endl;

			cout << "[Step 5] Both players getting ready..." << endl;
			player1Session->SendReady(true);
			this_thread::sleep_for(500ms);
			player2Session->SendReady(true);
			this_thread::sleep_for(1s);
			cout << "[Step 5] Both players are ready!\n" << endl;

			// 6. 방장(Player1)이 게임 시작
			cout << "[Step 6] Player1 (room owner) starting game..." << endl;
			player1Session->SendStartRoom();
			this_thread::sleep_for(2s); // 게임 시작 대기
			cout << "[Step 6] Game started!\n" << endl;

			// 7. 둘다 같은 스테이지 정보 받는지 확인
			cout << "[Step 7] Requesting stage info for both players..." << endl;
			player1Session->SendShowStage(1, 1); // Stage 1-1
			this_thread::sleep_for(500ms);
			player2Session->SendShowStage(1, 1); // Stage 1-1
			this_thread::sleep_for(2s);
			cout << "[Step 7] Stage info received (check above logs)\n" << endl;

			cout << "\n========== Test Scenario Complete ==========\n" << endl;
		});

	GThreadManager->Join();
}