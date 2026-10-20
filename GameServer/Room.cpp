#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "GameSession.h"
#include "ClientPacketHandler.h"
#include "RoomManager.h"
#include "StageManager.h"

RoomRef GTestRoom = nullptr;

Room::Room(uint64 roomId, const string& roomName, uint64 ownerId, string ownerName, int32 ownerTag)
	: _roomId(roomId), _roomName(roomName), _ownerId(ownerId), _ownerName(ownerName), _ownerTag(ownerTag)
{
}

Room::~Room()
{
	cout << "Room " << _roomId << " destroyed." << endl;
}

void Room::EnterLobby(PlayerRef player)
{
	{
		WRITE_LOCK;
		_players[player->playerId] = player;
		_readyStatus[player->playerId] = false;
	}

	BroadcastMemberEnter(player);
}

// 정원/진행 상태 검사와 입장을 방 작업 큐 안에서 한 번에 처리한다.
// 핸들러(워커 스레드)에서 검사하고 큐에서 입장하면, 그 사이에 다른 입장이 끼어들어 정원을 넘길 수 있다.
void Room::TryEnterLobby(PlayerRef player, GameSessionRef session, EnterSource source)
{
	RoomRef currentRoom = session->GetRoom().lock();
	string errorMsg;

	if (source == EnterSource::Normal && currentRoom)
	{
		errorMsg = "Already in a room";
	}
	else if (currentRoom.get() == this)
	{
		errorMsg = "Already in this room";
	}
	else
	{
		WRITE_LOCK;
		if (_isDestroyed)
			errorMsg = "Room not found";
		else if (_isPlaying)
			errorMsg = "Game already started";
		else if (_players.size() >= MAX_ROOM_CAPACITY)
			errorMsg = "Room is full";
		else
		{
			_players[player->playerId] = player;
			_readyStatus[player->playerId] = false;
		}
	}

	if (errorMsg.empty() == false)
	{
		std::cout << "Enter room failed: " << errorMsg << " - Room " << _roomId << ", Player " << player->name << endl;
		SendEnterRoomFail(session, source, errorMsg);
		return;
	}

	// 입장이 반영된 뒤에 세션에 방을 연결한다
	session->SetRoom(static_pointer_cast<Room>(shared_from_this()));

	// 초대 수락: 새 방 입장이 성공한 뒤에 기존 방을 나간다 (실패하면 기존 방에 그대로 남음)
	if (currentRoom)
		currentRoom->DoAsync(&Room::LeaveLobby, player);

	// 입장이 반영된 상태로 응답을 만든다
	Protocol::S_ENTER_ROOM enterRoomPkt;
	MakeEnterRoomPacket(enterRoomPkt);
	session->Send(ClientPacketHandler::MakeSendBuffer(enterRoomPkt));

	if (source == EnterSource::Invite)
	{
		Protocol::S_INVITE_RESPONSE responsePkt;
		responsePkt.set_success(true);
		session->Send(ClientPacketHandler::MakeSendBuffer(responsePkt));
	}

	BroadcastMemberEnter(player);
}

void Room::SendEnterRoomFail(GameSessionRef session, EnterSource source, const string& errorMsg)
{
	if (source == EnterSource::Invite)
	{
		Protocol::S_INVITE_RESPONSE responsePkt;
		responsePkt.set_success(false);
		responsePkt.set_error_msg("Failed to join room (" + errorMsg + ")");
		session->Send(ClientPacketHandler::MakeSendBuffer(responsePkt));
		return;
	}

	Protocol::S_ENTER_ROOM errorPkt;
	errorPkt.set_success(false);
	errorPkt.set_error_msg(errorMsg);
	session->Send(ClientPacketHandler::MakeSendBuffer(errorPkt));
}

void Room::BroadcastMemberEnter(PlayerRef player)
{
	// 방 멤버 입장 알림
	Protocol::S_ROOM_MEMBER_ENTER enterPkt;
	auto member = enterPkt.mutable_member();

	auto playerInfo = member->mutable_player();
	playerInfo->set_id(player->playerId);
	playerInfo->set_name(player->name);
	playerInfo->set_tag(player->tag);

	member->set_is_ready(false);

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(enterPkt);

	BroadcastExcept(sendBuffer, player->playerId);
	std::cout << "Player " << player->name << " entered lobby of Room " << _roomId << endl;
}

void Room::LeaveLobby(PlayerRef player)
{
	vector<GameSessionRef> targetSessions;
	SendBufferRef sendBuffer = nullptr;
	bool isDestroyed = false;

	// 초대 수락으로 이미 다른 방에 연결된 세션은 끊지 않는다 (이 방에 연결된 경우에만 해제)
	auto detachSession = [this](const GameSessionRef& session)
	{
		if (session->GetRoom().lock().get() == this)
			session->SetRoom(weak_ptr<Room>());
	};

	{
		WRITE_LOCK;

		// 1. 방장인지 확인 (방장이 나가는 경우)
		if (_ownerId == player->playerId)
		{
			isDestroyed = true;
			_isDestroyed = true;

			Protocol::S_ROOM_DESTROY destroyPkt;
			destroyPkt.set_room_id(_roomId);
			sendBuffer = ClientPacketHandler::MakeSendBuffer(destroyPkt);

			// 현재 방에 남은 모든 사람에게 보낼 세션 수집
			for (auto& p : _players)
			{
				if (auto session = static_pointer_cast<GameSession>(p.second->ownerSession.lock()))
				{
					targetSessions.push_back(session);
				}
			}

			_players.clear();
			_readyStatus.clear();

			if (auto session = static_pointer_cast<GameSession>(player->ownerSession.lock()))
			{
				detachSession(session);
			}
		}
		// 2. 일반 유저가 나가는 경우
		else
		{
			if (auto session = static_pointer_cast<GameSession>(player->ownerSession.lock()))
			{
				detachSession(session);
			}

			_players.erase(player->playerId);
			_readyStatus.erase(player->playerId);

			Protocol::S_ROOM_MEMBER_LEAVE leavePkt;
			leavePkt.set_player_id(player->playerId);
			leavePkt.set_player_name(player->name);
			sendBuffer = ClientPacketHandler::MakeSendBuffer(leavePkt);

			// 남은 사람들에게만 전송하기 위해 세션 수집
			for (auto& p : _players)
			{
				if (auto session = static_pointer_cast<GameSession>(p.second->ownerSession.lock()))
					targetSessions.push_back(session);
			}
		}
	}

	// 락 밖에서 안전하게 패킷 전송
	if (sendBuffer)
	{
		for (auto& session : targetSessions)
		{
			session->Send(sendBuffer);

			if (isDestroyed)
			{
				detachSession(session);
			}
		}
	}

	// 방이 파괴되었다면 매니저에서 제거
	if (isDestroyed)
	{
		std::cout << "Room " << _roomId << " destroyed by owner exit." << endl;
		RoomManager::Instance().RemoveRoom(_roomId);
	}
	else
	{
		std::cout << "Player " << player->name << " left lobby room " << _roomId << endl;
	}
}

void Room::SetReady(uint64 playerId, bool isReady)
{
	{
		WRITE_LOCK;
		if (_players.find(playerId) == _players.end())
		{
			return;
		}
		_readyStatus[playerId] = isReady;
	}

	// 준비 상태 브로드캐스트
	Protocol::S_READY readyPkt;
	readyPkt.set_player_id(playerId);
	readyPkt.set_is_ready(isReady);

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(readyPkt);
	Broadcast(sendBuffer);
}

bool Room::CanStartGame()
{
	READ_LOCK;

	for (auto& pair : _readyStatus)
	{
		// 한 명이라도 준비 안하면 false
		if (!pair.second)  
		{
			return false;
		}
	}

	return true;
}

// 시작 조건의 최종 판정은 여기(작업 큐 안)에서 한다.
// 핸들러에서 확인한 뒤 큐에 들어오기 전에 SetReady(false)나 새 멤버 입장이 먼저 실행될 수 있기 때문
void Room::StartGame(GameSessionRef requester)
{
	string errorMsg;
	uint64 ownerId = 0;
	{
		WRITE_LOCK;
		PlayerRef player = requester->GetPlayer();
		if (player == nullptr || player->playerId != _ownerId)
			errorMsg = "Only room owner can start game";
		else if (CanStartGame() == false)
			errorMsg = "Not all players are ready";
		else if (_isPlaying)
			errorMsg = "Game already started";
		else
			_isPlaying = true;

		ownerId = _ownerId;
	}

	if (errorMsg.empty() == false)
	{
		std::cout << "Start room failed: " << errorMsg << " - Room " << _roomId << endl;

		Protocol::S_START_ROOM errorPkt;
		errorPkt.set_success(false);
		errorPkt.set_error_msg(errorMsg);
		requester->Send(ClientPacketHandler::MakeSendBuffer(errorPkt));
		return;
	}

	Protocol::S_START_ROOM gameStartPkt;
	gameStartPkt.set_success(true);

	xvector<StageClearInfo> clearStages;
	GStageManager.GetMyStageClearInfo(ownerId, clearStages);

	auto hostClearStages = gameStartPkt.mutable_host_clear_stages();
	for (const auto& clearStage : clearStages)
	{
		Protocol::StageClearInfo* stageInfo = hostClearStages->Add();
		stageInfo->set_map_id(clearStage.stageId);
		stageInfo->set_star(clearStage.star);
		stageInfo->set_clear_time(clearStage.clearTime);
	}

	std::cout << "Game started in room " << _roomId << endl;

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(gameStartPkt);
	Broadcast(sendBuffer);
}

// 입장이 반영된 뒤(작업 큐 안)에서 호출해야 멤버 목록에 본인이 포함된다
void Room::MakeEnterRoomPacket(Protocol::S_ENTER_ROOM& pkt)
{
	READ_LOCK;
	pkt.set_success(true);

	// 방 정보
	Protocol::RoomInfo* roomInfo = pkt.mutable_room();
	roomInfo->set_room_id(_roomId);
	roomInfo->set_room_name(_roomName);
	roomInfo->set_current_count(static_cast<int32>(_players.size()));
	roomInfo->set_max_count(MAX_ROOM_CAPACITY);
	roomInfo->set_is_playing(_isPlaying);

	// 방장 정보
	Protocol::Player* host = roomInfo->mutable_host();
	host->set_id(_ownerId);
	host->set_name(_ownerName);
	host->set_tag(_ownerTag);

	// 멤버 리스트
	auto members = GetMembersWithReadyStatus();
	for (const auto& [player, isReady] : members)
	{
		Protocol::RoomMemberInfo* memberInfo = pkt.add_members();
		Protocol::Player* playerInfo = memberInfo->mutable_player();
		playerInfo->set_id(player->playerId);
		playerInfo->set_name(player->name);
		playerInfo->set_tag(player->tag);
		memberInfo->set_is_ready(isReady);
	}
}

void Room::SetOwner(uint64 ownerId, const string& ownerName, int32 ownerTag)
{
	WRITE_LOCK;
	_ownerId = ownerId;
	_ownerName = ownerName;
	_ownerTag = ownerTag;
}

void Room::EnterGame(PlayerRef player)
{
	Protocol::S_ENTER_GAME enterGamePkt;
	enterGamePkt.set_success(true);
	bool isAllLoaded = false;

	{
		WRITE_LOCK;
		_players[player->playerId] = player;
		_loadedPlayers.insert(player->playerId);

		for (auto& p : _players)
		{
			auto playerInfo = enterGamePkt.add_players();
			playerInfo->set_player_id(p.second->playerId);
			playerInfo->set_name(p.second->name);

			auto pos = playerInfo->mutable_pos();
			pos->set_x(p.second->posX); pos->set_y(p.second->posY); pos->set_z(p.second->posZ);

			auto rot = playerInfo->mutable_rot();
			rot->set_x(p.second->rotX); rot->set_y(p.second->rotY); rot->set_z(p.second->rotZ); rot->set_w(p.second->rotW);
		}
		isAllLoaded = (_loadedPlayers.size() == _players.size());
	}

	if (auto session = player->ownerSession.lock())
	{
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(enterGamePkt);
		session->Send(sendBuffer);
	}

	Protocol::S_PLAYER_ENTER newPlayerPkt;
	auto newPlayerInfo = newPlayerPkt.mutable_player();
	newPlayerInfo->set_player_id(player->playerId);
	newPlayerInfo->set_name(player->name);

	auto pos = newPlayerInfo->mutable_pos();
	pos->set_x(player->posX); pos->set_y(player->posY); pos->set_z(player->posZ);
	auto rot = newPlayerInfo->mutable_rot();
	rot->set_x(player->rotX); rot->set_y(player->rotY); rot->set_z(player->rotZ); rot->set_w(player->rotW);

	auto newPlayerBuffer = ClientPacketHandler::MakeSendBuffer(newPlayerPkt);
	BroadcastExcept(newPlayerBuffer, player->playerId);

	if (isAllLoaded)
	{
		std::cout << "Room " << _roomId << " : All players loaded. Game Ready!" << endl;
		Protocol::S_GAME_READY_TO_START readyStartPkt;
		readyStartPkt.set_start_delay_seconds(3);

		auto now = std::chrono::system_clock::now();
		auto startTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() + 3000;
		readyStartPkt.set_server_start_timestamp(startTimestamp);
		readyStartPkt.set_random_seed(rand());

		auto readyStartBuffer = ClientPacketHandler::MakeSendBuffer(readyStartPkt);
		Broadcast(readyStartBuffer);
	}
}

void Room::RelayPacket(PlayerRef sender, SendBufferRef sendBuffer, bool requireHostAuthority)
{
	// Host 에게만 전송한다. (Peer -> Host)
	if (requireHostAuthority)
	{
		if (sender->playerId != _ownerId)
		{
			GameSessionRef hostSession = nullptr;

			// 1. 필요한 호스트 세션만 빠르게 훔친다 (락 최소화)
			{
				READ_LOCK;
				auto it = _players.find(_ownerId);
				if (it != _players.end())
				{
					hostSession = static_pointer_cast<GameSession>(it->second->ownerSession.lock());
				}
			} // 락 해제

			// 2. 락 밖에서 전송
			if (hostSession != nullptr)
			{
				hostSession->Send(sendBuffer);
			}
			else
			{
				cout << "Host player not found in room " << _roomId << endl;
			}
		}
		else
		{
			Broadcast(sendBuffer);
		}
	}
	// Host가 전체에게 전송한다. (Host -> Peer)
	else
	{
		BroadcastExcept(sendBuffer, sender->playerId);
	}
}

vector<pair<PlayerRef, bool>> Room::GetMembersWithReadyStatus()
{
	READ_LOCK;
	vector<pair<PlayerRef, bool>> members;
	members.reserve(_players.size());

	for (const auto& [playerId, player] : _players)
	{
		bool isReady = false;
		auto it = _readyStatus.find(playerId);
		if (it != _readyStatus.end())
		{
			isReady = it->second;
		}
		members.push_back({ player, isReady });
	}

	return members;
}

RoomRef GetGlobalTestRoom()
{
	static shared_ptr<Room> instance = make_shared<Room>(999999, "Global Test Room", 0, "test", 0000);
	return instance;
}

void Room::Broadcast(SendBufferRef sendBuffer)
{
	vector<GameSessionRef> targetSessions;

	{
		READ_LOCK;
		targetSessions.reserve(_players.size());
		for (auto& p : _players)
		{
			if (auto session = static_pointer_cast<GameSession>(p.second->ownerSession.lock()))
				targetSessions.push_back(session);
		}
	} 

	// 락이 해제된 안전한 상태에서 네트워크 I/O 실행
	for (auto& session : targetSessions)
	{
		session->Send(sendBuffer);
	}
}

void Room::BroadcastExcept(SendBufferRef sendBuffer, uint64 excludePlayerId)
{
	vector<GameSessionRef> targetSessions;

	{
		READ_LOCK;
		targetSessions.reserve(_players.size());
		for (auto& p : _players)
		{
			if (p.first == excludePlayerId)
				continue;

			if (auto session = static_pointer_cast<GameSession>(p.second->ownerSession.lock()))
				targetSessions.push_back(session);
		}
	}

	for (auto& session : targetSessions)
	{
		session->Send(sendBuffer);
	}
}

