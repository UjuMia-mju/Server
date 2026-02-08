#include "pch.h"
#include "ClientPacketHandler.h"
#include "Player.h"
#include "Room.h"
#include "GameSession.h"

PacketHandleFunc GPacketHandler[UINT16_MAX];

// 컨텐츠는 직접 개발
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
	// TODO : Log
	return false;
}

bool Handle_C_LOGIN(PacketSessionRef& session, Protocol::C_LOGIN& pkt)
{
	GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session);

	//TODO: DB에서 뭔가 계정 확인하는 과정이 있다고 가정

	Protocol::S_LOGIN sLoginPkt;
	sLoginPkt.set_success(true);

	// DB에서 플레이어 정보를 긁어온다.
	// GameSession에 플레이어 정보를 저장(메모리)

	// ID발급
	static Atomic<uint64> idGenerator = 1;

	{
		auto player = sLoginPkt.mutable_player();
		player->set_name(u8"name1");
		player->set_playertype(Protocol::PlayerType::PLAYER_TYPE_MAGE);
		player->set_id(idGenerator++);

		PlayerRef playerRef = MakeShared<Player>();
		playerRef->name = player->name();
		playerRef->playerId = player->id();
		playerRef->type = player->playertype();
		playerRef->ownerSession = gameSession;

		gameSession->_player = playerRef;
	}

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(sLoginPkt);
	session->Send(sendBuffer);


	return true;
}

bool Handle_C_ENTER_GAME(PacketSessionRef& session, Protocol::C_ENTER_GAME& pkt)
{
	GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session);
	
	//uint64 index = pkt.player_index();
	//TODO: Validation

	gameSession->_room = GRoom;
	GRoom->DoAsync(&Room::Enter, gameSession->_player);

	Protocol::S_ENTER_GAME enterGamePkt;
	enterGamePkt.set_success(true);
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(enterGamePkt);
	gameSession->_player->ownerSession->Send(sendBuffer);

	return true;
}

bool Handle_C_CHAT(PacketSessionRef& session, Protocol::C_CHAT& pkt)
{
	cout << pkt.msg() << endl;

	Protocol::S_CHAT chatPkt;
	chatPkt.set_msg(pkt.msg());
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(chatPkt);

	GRoom->DoAsync(&Room::Broadcast, sendBuffer);

	return true;
}