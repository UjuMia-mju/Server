#include "pch.h"
#include "ClientPacketHandler.h"
#include "Player.h"
#include "Room.h"
#include "GameSession.h"
#include "AccountDB.h"

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

	std::string email = pkt.userid();
	std::string password = pkt.psw();

	cout << "Received Login - Email: " << email << ", Password: " << password << endl;

	Protocol::S_LOGIN sLoginPkt;

	// DB에서 플레이어 정보를 긁어온다.
	if (AccountDB::ValidateAccount(email, password))
	{
		int32 dbPlayerId = 0;
		wstring dbPlayerName;

		if (AccountDB::GetPlayerInfo(email, dbPlayerId, dbPlayerName))
		{
			sLoginPkt.set_success(true);
			// wstring -> UTF-8 string 변환
			int size = WideCharToMultiByte(CP_UTF8, 0, dbPlayerName.c_str(), -1, nullptr, 0, nullptr, nullptr);
			std::string playerNameUtf8;
			if (size > 0)
			{
				playerNameUtf8.resize(size - 1);
				WideCharToMultiByte(CP_UTF8, 0, dbPlayerName.c_str(), -1, &playerNameUtf8[0], size, nullptr, nullptr);
			}
			// DB에서 플레이어 정보를 세팅한다.
			auto player = sLoginPkt.mutable_player();
			player->set_id(dbPlayerId);
			player->set_name(playerNameUtf8);

			PlayerRef playerRef = MakeShared<Player>();
			playerRef->name = player->name();
			playerRef->playerId = player->id();
			playerRef->ownerSession = gameSession;

			gameSession->_player = playerRef;
		}
		else
		{
			sLoginPkt.set_success(false);
			cout << "Failed to retrieve player info for email: " << email << endl;
		}
	}
	else
	{
		// Fail to Login
		sLoginPkt.set_success(false);
		cout << "Failed to validate account for email: " << email << endl;
	}

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(sLoginPkt);
	cout << "Sending S_LOGIN packet - Size: " << sendBuffer->WriteSize()
		<< ", Success: " << sLoginPkt.success() << endl;
	session->Send(sendBuffer);

	return true;
}

bool Handle_C_ENTER_GAME(PacketSessionRef& session, Protocol::C_ENTER_GAME& pkt)
{
	GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session);

	gameSession->_room = GRoom;
	GRoom->DoAsync(&Room::Enter, gameSession->_player);

	Protocol::S_ENTER_GAME enterGamePkt;
	enterGamePkt.set_success(true);
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(enterGamePkt);
	session->Send(sendBuffer);

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

bool Handle_C_MOVE(PacketSessionRef& session, Protocol::C_MOVE& pkt)
{
	GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session);

	// 플레이어가 로그인되어 있는지 확인
	if (gameSession->_player == nullptr)
	{
		cout << "Move packet from non-logged in player!" << endl;
		return false;
	}

	// 플레이어 위치 업데이트 (메모리)
	gameSession->_player->posX = pkt.pos().x();
	gameSession->_player->posY = pkt.pos().y();
	gameSession->_player->posZ = pkt.pos().z();

	gameSession->_player->rotX = pkt.rot().x();
	gameSession->_player->rotY = pkt.rot().y();
	gameSession->_player->rotZ = pkt.rot().z();
	gameSession->_player->rotW = pkt.rot().w();

	// 같은 방에 있는 플레이어들에게 브로드캐스트
	Protocol::S_MOVE movePkt;
	movePkt.set_playerid(gameSession->_player->playerId);

	auto pos = movePkt.mutable_pos();
	pos->set_x(pkt.pos().x());
	pos->set_y(pkt.pos().y());
	pos->set_z(pkt.pos().z());

	auto rot = movePkt.mutable_rot();
	rot->set_x(pkt.rot().x());
	rot->set_y(pkt.rot().y());
	rot->set_z(pkt.rot().z());
	rot->set_w(pkt.rot().w());

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(movePkt);

	// 같은 방의 모든 플레이어에게 전송
	auto roomPtr = gameSession->_room.lock();
	if (roomPtr != nullptr)
	{
		roomPtr->DoAsync(&Room::Broadcast, sendBuffer);
	}
	else
	{
		cout << "Room not found for player: " << gameSession->_player->name << endl;
	}

	return true;
}
