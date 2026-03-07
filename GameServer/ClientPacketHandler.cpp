#include "pch.h"
#include "ClientPacketHandler.h"
#include "Player.h"
#include "Room.h"
#include "RoomManager.h"
#include "GameSession.h"
#include "AccountDB.h"
#include "AuthValidator.h"

PacketHandleFunc GPacketHandler[UINT16_MAX];

namespace
{
	void SendErrorResponse(PacketSessionRef& session, const PacketResult& result)
	{
		// 패킷 타입별 에러 응답 (함수 오버로딩으로 처리)
	}

	void SendCreateRoomError(PacketSessionRef& session, const std::string& errorMsg)
	{
		Protocol::S_CREATE_ROOM errorPkt;
		errorPkt.set_success(false);
		errorPkt.set_error_msg(errorMsg);
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(errorPkt);
		session->Send(sendBuffer);
	}

	void SendStartRoomError(PacketSessionRef& session, const std::string& errorMsg)
	{
		Protocol::S_START_ROOM errorPkt;
		errorPkt.set_success(false);
		errorPkt.set_error_msg(errorMsg);
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(errorPkt);
		session->Send(sendBuffer);
	}

	void SendEnterGameError(PacketSessionRef& session, const std::string& errorMsg)
	{
		Protocol::S_ENTER_GAME errorPkt;
		errorPkt.set_success(false);
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(errorPkt);
		session->Send(sendBuffer);
	}
}

#define CHECK_AUTH_LOGIN(session, ErrorSender) \
    GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session); \
    PacketResult authResult = AuthValidator::ValidateAuth(gameSession, AuthLevel::LOGGED_IN); \
    if (!authResult.IsSuccess()) { \
        ErrorSender(session, authResult.GetMessage()); \
        return false; \
    }

#define CHECK_AUTH_ROOM(session, room, ErrorSender) \
    GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session); \
    PacketResult authResult; \
    auto room = AuthValidator::GetRoomIfValid(gameSession, &authResult); \
    if (!room) { \
        ErrorSender(session, authResult.GetMessage()); \
        return false; \
    }

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

	std::cout << "Received Login - Email: " << email << ", Password: " << password << endl;

	Protocol::S_LOGIN sLoginPkt;

	// DB에서 플레이어 정보를 긁어온다.
	if (AccountDB::ValidateAccount(email, password))
	{
		int32 dbPlayerId = 0;
		wstring dbPlayerName;
		int32 dbPlayerTag;
		if (AccountDB::GetPlayerInfo(email, dbPlayerId, dbPlayerName, dbPlayerTag))
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
			player->set_tag(dbPlayerTag);

			PlayerRef playerRef = MakeShared<Player>();
			playerRef->name = player->name();
			playerRef->playerId = player->id();
			playerRef->ownerSession = gameSession;

			gameSession->_player = playerRef;
		}
		else
		{
			sLoginPkt.set_success(false);
			std::cout << "Failed to retrieve player info for email: " << email << endl;
		}
	}
	else
	{
		// Fail to Login
		sLoginPkt.set_success(false);
		std::cout << "Failed to validate account for email: " << email << endl;
	}

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(sLoginPkt);
	std::cout << "Sending S_LOGIN packet - Size: " << sendBuffer->WriteSize()
		<< ", Success: " << sLoginPkt.success() << endl;
	session->Send(sendBuffer);

	return true;
}

bool Handle_C_CREATE_ROOM(PacketSessionRef& session, Protocol::C_CREATE_ROOM& pkt)
{
	CHECK_AUTH_LOGIN(session, SendCreateRoomError);

	// 이미 방에 있는지 체크
	if (auto currentRoom = gameSession->_room.lock())
	{
		std::cout << "Create room failed: Already in a room" << endl;
		SendCreateRoomError(session, "Already in a room");
		return false;
	}

	// 방 생성
	int64 hostId = gameSession->_player->playerId;
	string hostName = gameSession->_player->name;
	int32 hostTag = gameSession->_player->tag;
	RoomRef newRoom = RoomManager::Instance().CreateRoom(hostId, hostName, hostTag);
	
	gameSession->_room = newRoom;
	//newRoom->DoAsync(&Room::Enter, gameSession->_player);

	// 패킷 만들기
	Protocol::S_CREATE_ROOM createRoomPkt;
	Protocol::RoomInfo* roomInfo = createRoomPkt.mutable_room();
	roomInfo->set_room_id(newRoom->GetRoomId());
	roomInfo->set_current_count(1);
	roomInfo->set_max_count(4);
	roomInfo->set_is_playing(false);

	// host 정보 세팅
	Protocol::Player* host = roomInfo->mutable_host();
	host->set_id(gameSession->_player->playerId);
	host->set_name(gameSession->_player->name);
	host->set_tag(gameSession->_player->tag);

	// 방 생성 결과 패킷 전송
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(createRoomPkt);
	session->Send(sendBuffer);
	
	return true;
}

bool Handle_C_ROOM_LIST(PacketSessionRef& session, Protocol::C_ROOM_LIST& pkt)
{
	return false;
}

bool Handle_C_ENTER_ROOM(PacketSessionRef& session, Protocol::C_ENTER_ROOM& pkt)
{
	return false;
}

bool Handle_C_LEAVE_ROOM(PacketSessionRef& session, Protocol::C_LEAVE_ROOM& pkt)
{
	return false;
}

bool Handle_C_INVITE_PLAYER(PacketSessionRef& session, Protocol::C_INVITE_PLAYER& pkt)
{
	return false;
}

bool Handle_C_INVITE_RESPONSE(PacketSessionRef& session, Protocol::C_INVITE_RESPONSE& pkt)
{
	return false;
}

bool Handle_C_READY(PacketSessionRef& session, Protocol::C_READY& pkt)
{
	CHECK_AUTH_ROOM(session, OUT room, SendStartRoomError);

	// 준비 상태 변경
	bool isReady = pkt.is_ready();
	room->DoAsync(&Room::SetReady, gameSession->_player->playerId, isReady);

	std::cout << "Player " << gameSession->_player->name
		<< (isReady ? " is ready" : " cancelled ready") << endl;

	return true;
}

bool Handle_C_START_ROOM(PacketSessionRef& session, Protocol::C_START_ROOM& pkt)
{
	// 방에 있는지 체크
	CHECK_AUTH_ROOM(session, OUT room, SendStartRoomError);

	// 방장인지 확인
	if (room->GetOwnerId() != gameSession->_player->playerId)
	{
		std::cout << "Start room failed: Only room owner" << endl;
		SendStartRoomError(session, "Only room owner can start game");
		return false;
	}

	// 모두 준비되었는지 확인
	if (!room->CanStartGame())
	{
		std::cout << "Start room failed: Not all players are ready" << endl;
		SendStartRoomError(session, "Not all players are ready");
		return false;
	}

	// 게임 시작
	room->DoAsync(&Room::StartGame);

	std::cout << "Room " << room->GetRoomId() << " game started by "
		<< gameSession->_player->name << endl;

	return true;
}

bool Handle_C_ENTER_GAME(PacketSessionRef& session, Protocol::C_ENTER_GAME& pkt)
{
	GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session);

	// ------
	//auto room = gameSession->_room.lock();

	auto room = GetGlobalTestRoom(); // 테스트용 임시 코드
	gameSession->_room = room; // 테스트용 임시 코드

	// ------

	if (!room)
	{
		std::cout << "Enter game failed: Room not available" << endl;
		SendEnterGameError(session, "Room not available");
		return false;
	}

	room->DoAsync(&Room::EnterGame, gameSession->_player);

	Protocol::S_ENTER_GAME enterGamePkt;
	enterGamePkt.set_success(true);
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(enterGamePkt);
	session->Send(sendBuffer);

	return true;
}

bool Handle_C_SHOW_STAGE(PacketSessionRef& session, Protocol::C_SHOW_STAGE& pkt)
{
	return false;
}

bool Handle_C_START_STAGE(PacketSessionRef& session, Protocol::C_START_STAGE& pkt)
{
	return false;
}

bool Handle_C_CHAT(PacketSessionRef& session, Protocol::C_CHAT& pkt)
{
	// 방에 있는지 체크
	CHECK_AUTH_ROOM(session, OUT room, SendStartRoomError);

	std::cout << pkt.msg() << endl;

	Protocol::S_CHAT chatPkt;
	chatPkt.set_msg(pkt.msg());
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(chatPkt);

	room->DoAsync(&Room::Broadcast, sendBuffer);

	return true;
}

bool Handle_C_MOVE(PacketSessionRef& session, Protocol::C_MOVE& pkt)
{
	GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session);

	// 플레이어가 로그인되어 있는지 확인
	if (gameSession->_player == nullptr)
	{
		std::cout << "Move packet from non-logged in player!" << endl;
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
		std::cout << "Room not found for player: " << gameSession->_player->name << endl;
	}

	return true;
}

bool Handle_C_ANIMATION(PacketSessionRef& session, Protocol::C_ANIMATION& pkt)
{
	GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session);

	gameSession->_player->animState = pkt.state();
	Protocol::S_ANIMATION animPkt;
	animPkt.set_playerid(gameSession->_player->playerId);
	animPkt.set_state(pkt.state());

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(animPkt);
	auto roomPtr = gameSession->_room.lock();

	if (roomPtr != nullptr)
	{
		roomPtr->DoAsync(&Room::Broadcast, sendBuffer);
	}

	return true;
}
