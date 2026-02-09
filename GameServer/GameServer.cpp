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

using TL = TypeList<class A, class B, class C>;

class A
{
public:
	A()
	{
		INIT_TL(A);
	}
	virtual ~A() {}

	DECLARE_TL;
};

class B : public A
{
public:
	B() { INIT_TL(B); }
};

class C : public A
{
public:
	C() { INIT_TL(C); }
};

int main()
{
	{
		shared_ptr<B> b = MakeShared<B>();
		shared_ptr<A> a = TypeCast<A>(b);
		bool canCast = CanCast<A>(b);
	}

	//
	//ASSERT_CRASH(GDBConnectionPool->Connect(1, L"DRIVER={MySQL ODBC 9.6 Unicode Driver};SERVER=localhost;PORT=3306;DATABASE=UjuMia;UID=root;PWD=Willylee0309!;"));

	////Query Read
	//{		
	//	DBConnection* dbConn = GDBConnectionPool->Pop();

	//	DBBind<0, 2> dbBind(*dbConn, L"SELECT id, username FROM users");
	//	
	//	int id = 0;
	//	WCHAR username[51] = { 0 };

	//	dbBind.BindCol(0, id);
	//	dbBind.BindCol(1, username, sizeof(username));

	//	ASSERT_CRASH(dbBind.Execute());

	//	while (dbConn->Fetch())
	//	{
	//		wstring userStr(username);
	//		wcout << "User Id: " << id << L", Username: " << userStr << endl;
	//	}

		//dbConn->Unbind();

		//int id;
		//SQLLEN outIdLen = 0;
		//dbConn->BindCol(1, &id, &outIdLen);

		//WCHAR username[51] = { 0 };
		//SQLLEN outUsernameLen = 0;
		//dbConn->BindCol(2, username, sizeof(username), &outUsernameLen);

		//ASSERT_CRASH(dbConn->Execute(L"SELECT * FROM users"));

		//while (dbConn->Fetch())
		//{
		//	wstring userStr(username, outUsernameLen / sizeof(WCHAR));
		//	wcout << "User Id: " << id << L", Username: " << userStr << endl;
		//}
		//GDBConnectionPool->Push(dbConn);
	//}

	// ---------------------------- 
	//GRoom->DoTimer(1000, [] {cout << "Hello 1000" << endl;});
	//GRoom->DoTimer(2000, [] {cout << "Hello 1000" << endl;});
	//GRoom->DoTimer(3000, [] {cout << "Hello 1000" << endl;});


	//ClientPacketHandler::Init();

	//ServerServiceRef service = MakeShared<ServerService>(
	//	NetAddress(L"127.0.0.1", 7777),
	//	MakeShared<IocpCore>(),
	//	MakeShared<GameSession>,
	//	100
	//);

	//ASSERT_CRASH(service->Start());

	//for (int32 i = 0; i < 5; i++)
	//{
	//	GThreadManager->Launch([&service]()
	//		{
	//			DoWorkerJob(service);
	//		});
	//}

	//DoWorkerJob(service);

	//GThreadManager->Join();
}