#include "pch.h"
#include "ServerPacketHandler.h"
#include "BufferReader.h"

PacketHandleFunc GPacketHandler[UINT16_MAX];

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
	// TODO Log
	return true;
}

bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt)
{	
	if (pkt.success() == false)
	{
		return true;
	}

	if (pkt.has_player() == false)
	{
		// 캐릭터 처음 시작
		//return true;
	}

	// 입장 UI 버튼 눌러서 게임 입장
	Protocol::C_ENTER_GAME enterGamePkt;
	enterGamePkt.set_playerindex(0); // 첫번째 캐릭터 입장
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(enterGamePkt);
	
	
	//TODO: Session이 삭제 되면서 접근 충돌 문제 해결
	//session->Send(sendBuffer);

	return true;
}

bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt)
{

	return true;
}

bool Handle_S_CHAT(PacketSessionRef& session, Protocol::S_CHAT& pkt)
{
	cout << pkt.msg() << endl;
	return true;
}
