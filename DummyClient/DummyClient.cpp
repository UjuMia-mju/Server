#include "pch.h"
#include <iostream>
#include "Service.h"
#include "ServerSession.h"

#include "Memory.h"
#include "ThreadManager.h"

#include "BufferReader.h"
#include "ServerPacketHandler.h"
#include "BasicTest.h"
#include "StressTest.h"
#include "RoomStressTest.h"
#include "InviteFlowTest.h"
#include "GameFlowTest.h"
#include "ServerAuthorityTest.h"

// 전역 접속 카운터
atomic<int32> GConnectedCount = 0;

// 실행할 테스트. 여기만 바꾸면 갈아끼울 수 있음
//   GameFlow   -> InviteFlow 후 엔터 -> 레디 -> 방 시작 -> 스테이지 선택 -> 스테이지 시작 -> 릴레이 초당 10~20회 랜덤 (설정은 GameFlowTest.cpp 맨 위)
//   InviteFlow -> 100명 로그인 -> 4명씩 그룹, 첫 번째가 방 생성 -> 나머지 초대/입장 (설정은 InviteFlowTest.cpp 맨 위)
//   RoomStress -> 4명씩 방 만들고 릴레이 패킷 전송 + 지연시간 측정 (설정은 RoomStressTest.cpp 맨 위)
//   Stress     -> 100명 로비 패킷 스트레스 테스트 (설정은 StressTest.cpp 맨 위)
//   Basic      -> 기존 2인 시나리오 테스트 (BasicTest.h 참고)
//   (명령줄) DummyClient.exe authority [시나리오...] -> 서버 권한 검증 테스트 (ServerAuthorityTest.h 참고)
enum class TestType { GameFlow, InviteFlow, RoomStress, Stress, Basic };
const TestType RUN_TEST = TestType::GameFlow;

int main(int argc, char* argv[])
{
	::SetConsoleOutputCP(CP_UTF8); // 한글 깨짐 방지

	this_thread::sleep_for(6s); // 서버가 완전히 시작될 때까지 대기
	ServerPacketHandler::Init();

	this_thread::sleep_for(1s);

	if (argc > 1 && string(argv[1]) == "authority")
	{
		vector<string> scenarios(argv + 2, argv + argc);
		// IOCP 워커 스레드가 무한 루프라 프로세스를 바로 종료
		::ExitProcess(ServerAuthorityTest::Run(scenarios));
	}

	switch (RUN_TEST)
	{
		case TestType::GameFlow:	GameFlowTest::Run();	break;
		case TestType::InviteFlow:	InviteFlowTest::Run();	break;
		case TestType::RoomStress:	RoomStressTest::Run();	break;
		case TestType::Stress:		StressTest::Run();		break;
		case TestType::Basic:		BasicTest::Run();		break;
	}
}
