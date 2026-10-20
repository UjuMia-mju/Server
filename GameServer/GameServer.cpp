#include "pch.h"
#include "ThreadManager.h"
#include "Service.h"
#include "Session.h"
#include "GameSession.h"
#include "GameSessionManager.h"
#include "BufferWriter.h"
#include "ClientPacketHandler.h"
#include <tchar.h>
#include "Protocol.pb.h"
#include "Room.h"
#include "Player.h"
#include "DBConnectionPool.h"
#include "DBBind.h"
#include "TypeCast.h"
#include "DBCacheManager.h"
#include "AdminSession.h"
#include "LockStats.h"

std::atomic<int64> g_inboundPacketCount(0);
std::atomic<int64> g_outboundPacketCount(0);

enum
{
	WORKER_TICK = 64
};

void DoWorkerJob(ServerServiceRef& service)
{
	while (true)
	{
		LEndTickCount = ::GetTickCount64() + WORKER_TICK;
		service->GetIocpCore()->Dispatch(10);

		// 예약된 일감 처리
		ThreadManager::DistributeReservedJobs();

		//글로벌 큐
		ThreadManager::DoGlobalQueueWork();
	}
}

int main()
{
	::SetConsoleOutputCP(CP_UTF8);
	cout << "Game Server Start! - git action CI/CD - ver R." << endl;

	// Config 파일 로드
	WCHAR exePath[MAX_PATH];
	GetModuleFileNameW(NULL, exePath, MAX_PATH);

	// 디렉터리 부분만 추출
	std::wstring exeDir = exePath;
	size_t pos = exeDir.find_last_of(L"\\/");
	if (pos != std::wstring::npos) {
		exeDir = exeDir.substr(0, pos + 1);  // 마지막 '\' 포함
	}

	// config.ini 전체 경로 생성
	std::wstring configPath = exeDir + L"config.ini";

	wcout << L"Exe location: " << exeDir << endl;
	wcout << L"Config path: " << configPath << endl;

	// Config 파일 로드
	if (!GConfigManager->LoadConfig(configPath))
	{
		wcout << L"Failed to load config.ini!" << endl;
		wcout << L"Make sure config.ini is in the same folder as GameServer.exe" << endl;
		return -1;
	}

	std::wstring dbConnectionString = GConfigManager->GetDBConnectionString();
	ASSERT_CRASH(GDBConnectionPool->Connect(4, dbConnectionString.c_str()));

	// DB 정보 미리 로드 (뽑기 풀, 스테이지 정보등..)
	DBCacheManager::GetInstance().InitAsync();
	DBCacheManager::GetInstance().WaitAll();

	ClientPacketHandler::Init();
#ifndef ENABLE_TEST_PACKETS
	// 테스트용 패킷은 정식 빌드에서 받지 않는다 -> Handle_INVALID로 처리
	// (ClientPacketHandler.h는 빌드할 때마다 GenPackets로 다시 생성되므로 등록 해제는 여기서 한다)
	GPacketHandler[PKT_C_TEST_ENTER_ROOM] = Handle_INVALID;
	GPacketHandler[PKT_C_TEST_ENTER_GAME] = Handle_INVALID;
#endif

	// Config에서 서버 설정 가져오기
	std::wstring serverIP = GConfigManager->GetServerIP();
	uint16 serverPort = GConfigManager->GetServerPort();
	int32 workerThreads = GConfigManager->GetWorkerThreadCount();

	ServerServiceRef service = MakeShared<ServerService>(
		NetAddress(serverIP, serverPort),
		MakeShared<IocpCore>(),
		MakeShared<GameSession>,
		100
	);

	ASSERT_CRASH(service->Start());

	// --- WPF 툴 전용 서비스 (9000번 포트) ---
	ServerServiceRef adminService = MakeShared<ServerService>(
		NetAddress(serverIP, 9000),
		service->GetIocpCore(),        // 기존 IOCP Core 재사용 가능
		MakeShared<AdminSession>,      // 관리자 전용 세션 사용
		10
	);

	ASSERT_CRASH(adminService->Start());

	//DB Keep-Alive 전용 스레드 추가
	GThreadManager->Launch([]()
		{
			while (true)
			{
				// 30분마다 실행
				this_thread::sleep_for(std::chrono::minutes(30));

				if (GDBConnectionPool)
				{
					GDBConnectionPool->HandleKeepAlive();
				}
			}
		});

#if USE_LOCK_STATS
	// 락 호출 통계 ([Lock] 태그): 1초마다 클래스별 READ/WRITE 호출 수, 10초마다 누적
	//   끄려면 ServerCore/LockStats.h의 USE_LOCK_STATS를 0으로
	GThreadManager->Launch([]()
		{
			const int32 TOTAL_EVERY_SEC = 10;
			int32 tick = 0;
			bool printedSinceTotal = false;
			while (true)
			{
				this_thread::sleep_for(1s);

				string line = LockStats::TakeRateLine(1);
				if (line.empty() == false)
				{
					cout << line << endl;
					printedSinceTotal = true;
				}

				// 일이 있었던 구간에만 누적 출력 (idle일 때 로그가 쌓이지 않게)
				if (++tick % TOTAL_EVERY_SEC == 0 && printedSinceTotal)
				{
					cout << LockStats::TotalLine() << endl;
					printedSinceTotal = false;
				}
			}
		});
#endif

	for (int32 i = 0; i < workerThreads; i++)
	{
		GThreadManager->Launch([&service]()
			{
				DoWorkerJob(service);
			});
	}

	DoWorkerJob(service);

	GThreadManager->Join();
}