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
	WRITE_LOCK;
	_players[player->playerId] = player;
	_readyStatus[player->playerId] = false;

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
	WRITE_LOCK;
	_players.erase(player->playerId);
	_readyStatus.erase(player->playerId);

	// 퇴장 알림
	Protocol::S_ROOM_MEMBER_LEAVE leavePkt;
	leavePkt.set_player_id(player->playerId);
	leavePkt.set_player_name(player->name);

	// 방장이 나갔다면 새 방장 지정
	if (_ownerId == player->playerId && !_players.empty())
	{
		_ownerId = _players.begin()->first;
		leavePkt.set_new_owner_id(_ownerId);
	}
	else if (_players.empty())
	{
		// 방 삭제
		RoomManager::Instance().RemoveRoom(_roomId);
	}
	else
	{
		leavePkt.set_new_owner_id(0); // 0이면 아무런 반응X
	}

	auto leaveBuffer = ClientPacketHandler::MakeSendBuffer(leavePkt);
	Broadcast(leaveBuffer);

	std::cout << "Player " << player->name << " left lobby room " << _roomId << endl;
}

void Room::SetReady(uint64 playerId, bool isReady)
{
	WRITE_LOCK;

	if (_players.find(playerId) == _players.end())
	{
		return;
	}

	_readyStatus[playerId] = isReady;

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
	WRITE_LOCK;

	// 이미 게임 중이면 무시
	if (_isPlaying)
	{
		std::cout << "Cannot start game: Already playing" << endl;
		return;
	}
	// 게임 시작 알림 브로드캐스트
	Protocol::S_START_ROOM gameStartPkt;
	gameStartPkt.set_success(true);

	// 호스트의 클리어 스테이지 정보 가져오기
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

	_isPlaying = true;
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

void Room::EnterGame(PlayerRef player)
{
	WRITE_LOCK;
	_players[player->playerId] = player;

	// 새로 들어온 플레이어에게 기존 플레이어 정보 브로드캐스트
	Protocol::S_PLAYER_LIST playerListPkt;

    for (auto& p : _players)
    {
        if (p.first == player->playerId)
            continue;

        auto playerInfo = playerListPkt.add_players();
        playerInfo->set_player_id(p.second->playerId);
        playerInfo->set_name(p.second->name);

        auto pos = playerInfo->mutable_pos();
        pos->set_x(p.second->posX);
        pos->set_y(p.second->posY);
        pos->set_z(p.second->posZ);

        auto rot = playerInfo->mutable_rot();
        rot->set_x(p.second->rotX);
        rot->set_y(p.second->rotY);
        rot->set_z(p.second->rotZ);
        rot->set_w(p.second->rotW);
    }

    if (auto session = player->ownerSession.lock())
    {
        auto sendBuffer = ClientPacketHandler::MakeSendBuffer(playerListPkt);
        session->Send(sendBuffer);
    }

	// 새로 들어온 플레이어 정보를 기존 플레이어들에게 브로드캐스트
    Protocol::S_PLAYER_ENTER newPlayerPkt;
	auto newPlayerInfo = newPlayerPkt.mutable_player();
	newPlayerInfo->set_player_id(player->playerId);
	newPlayerInfo->set_name(player->name);

	auto pos = newPlayerInfo->mutable_pos();
	pos->set_x(player->posX);
	pos->set_y(player->posY);
	pos->set_z(player->posZ);

	auto rot = newPlayerInfo->mutable_rot();
	rot->set_x(player->rotX);
	rot->set_y(player->rotY);
	rot->set_z(player->rotZ);
	rot->set_w(player->rotW);

	auto newPlayerBuffer = ClientPacketHandler::MakeSendBuffer(newPlayerPkt);

	// 자기 자신을 제외한 나머지 플레이어들에게 새로 들어온 플레이어 정보 브로드캐스트
	Broadcast(newPlayerBuffer);
}

void Room::LeaveGame(PlayerRef player)
{
	WRITE_LOCK;
	_players.erase(player->playerId);

	// 퇴장 알림
	Protocol::S_PLAYER_LEAVE leavePkt;
	auto leavePlayerInfo = leavePkt.mutable_player();
	leavePlayerInfo->set_player_id(player->playerId);
	auto leaveBuffer = ClientPacketHandler::MakeSendBuffer(leavePkt);

	Broadcast(leaveBuffer);
}

void Room::Broadcast(SendBufferRef sendBuffer)
{
	WRITE_LOCK;
	for (auto& p : _players)
	{
		GameSessionRef session = static_pointer_cast<GameSession>(p.second->ownerSession.lock());
		if (session != nullptr)
		{
			session->Send(sendBuffer);
		}
	}
}

void Room::BroadcastExcept(SendBufferRef sendBuffer, uint64 excludePlayerId)
{
	WRITE_LOCK;
	for (auto& p : _players)
	{
		if (p.first == excludePlayerId)
			continue;

		if (auto session = p.second->ownerSession.lock())
		{
			session->Send(sendBuffer);
		}
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
