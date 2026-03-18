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
#include "GachaManager.h"

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
	cout << "Game Server Start!" << endl;

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

	// 뽑기 매니저 초기화 (DB에서 가챠 풀과 아이템 정보 로드)
	if (GGACHA.Init(L"ko") == false)
	{
		cout << "가챠 매니저 초기화 실패! DB를 확인하세요." << endl;
		return -1; // 핵심 데이터가 없으면 서버 부팅을 막아야 함
	}

	ClientPacketHandler::Init();

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