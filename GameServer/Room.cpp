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

	{
		WRITE_LOCK;

		// 1. 방장인지 확인 (방장이 나가는 경우)
		if (_ownerId == player->playerId)
		{
			isDestroyed = true;

			Protocol::S_ROOM_DESTROY destroyPkt;
			destroyPkt.set_room_id(_roomId);
			sendBuffer = ClientPacketHandler::MakeSendBuffer(destroyPkt);

			// 현재 방에 남은 모든 사람에게 보낼 세션 수집
			for (auto& p : _players)
			{
				if (auto session = static_pointer_cast<GameSession>(p.second->ownerSession.lock()))
					targetSessions.push_back(session);
			}
		}
		// 2. 일반 유저가 나가는 경우
		else
		{
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
			session->Send(sendBuffer);
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

void Room::StartGame()
{
	{
		WRITE_LOCK;
		if (_isPlaying) return;
		_isPlaying = true;
	}

	Protocol::S_START_ROOM gameStartPkt;
	gameStartPkt.set_success(true);

	xvector<StageClearInfo> clearStages; 
	GStageManager.GetMyStageClearInfo(_ownerId, clearStages);

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

void Room::MakeEnterRoomPacket(GameSessionRef gameSession, Protocol::S_ENTER_ROOM& pkt) const
{
	pkt.set_success(true);

	// 방 정보
	Protocol::RoomInfo* roomInfo = pkt.mutable_room();
	roomInfo->set_room_id(GetRoomId());
	roomInfo->set_room_name(GetRoomName());
	roomInfo->set_current_count(GetCurrentCount());
	roomInfo->set_max_count(GetMaxCount());
	roomInfo->set_is_playing(IsPlaying());

	// 방장 정보
	Protocol::Player* host = roomInfo->mutable_host();
	host->set_id(GetOwnerId());
	host->set_name(GetOwnerName());
	host->set_tag(GetOwnerTag());

	// 멤버 리스트
	auto members = GetMembersWithReadyStatus();
	bool hasSelf = false;
	for (const auto& [player, isReady] : members)
	{
		if (player->playerId == gameSession->GetPlayer()->playerId)
		{
			hasSelf = true;
		}
			
		Protocol::RoomMemberInfo* memberInfo = pkt.add_members();
		Protocol::Player* playerInfo = memberInfo->mutable_player();
		playerInfo->set_id(player->playerId);
		playerInfo->set_name(player->name);
		playerInfo->set_tag(player->tag);
		memberInfo->set_is_ready(isReady);
	}
	if (!hasSelf)
	{
		Protocol::RoomMemberInfo* memberInfo = pkt.add_members();
		Protocol::Player* playerInfo = memberInfo->mutable_player();
		playerInfo->set_id(gameSession->GetPlayer()->playerId);
		playerInfo->set_name(gameSession->GetPlayer()->name);
		playerInfo->set_tag(gameSession->GetPlayer()->tag);
		memberInfo->set_is_ready(false);
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

vector<pair<PlayerRef, bool>> Room::GetMembersWithReadyStatus() const
{
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

