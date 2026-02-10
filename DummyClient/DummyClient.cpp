#include "pch.h"
#include <iostream>
#include "Service.h"
#include "Session.h"

#include "Memory.h"
#include "ThreadManager.h"

#include "BufferReader.h"
#include "ServerPacketHandler.h"


class ServerSession : public PacketSession
{
public:
	~ServerSession()
	{
		cout << "~ServerSession()" << endl;
	}
	virtual void OnConnected() override
	{
		Protocol::C_LOGIN pkt;
		auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
		Send(sendBuffer);
	}

	virtual void OnRecvPacket(BYTE* buffer, int32 len) override
	{
		PacketSessionRef session = GetPacketSessionRef();
		PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);

		ServerPacketHandler::HandlePacket(session, buffer, len);
	}
	virtual void OnSend(int32 len) override
	{
		//cout << "OnSend Len:" << len << endl;
	}

	virtual void OnDisconnected() override
	{
		//cout << "OnDisconnected()" << endl;
	}
};

int main()
{
	ServerPacketHandler::Init();

	this_thread::sleep_for(1s);

	ClientServiceRef service = MakeShared<ClientService>(
		NetAddress(L"127.0.0.1", 7777),
		MakeShared<IocpCore>(),
		MakeShared<ServerSession>,
		100);

	ASSERT_CRASH(service->Start());

	for (int32 i = 0; i < 2; i++)
	{
		GThreadManager->Launch([=]()
			{
				while (true)
				{
					service->GetIocpCore()->Dispatch();
				}
			});
	}

	cout << "DummyClient Start!" << endl;

	Protocol::C_CHAT chatPkt;
	chatPkt.set_msg(u8"Hello World !");
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(chatPkt);

	while (true)
	{
		service->Brodcast(sendBuffer);
		this_thread::sleep_for(1s);
	}

	GThreadManager->Join();
}