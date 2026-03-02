#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "GameSession.h"
#include "ClientPacketHandler.h"

shared_ptr<Room> GRoom = make_shared<Room>();

Room::Room(uint64 roomId, const string& roomName, uint64 ownerId)
	: _roomId(roomId), _roomName(roomName), _ownerId(ownerId)
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
	_isPlaying = true;
	std::cout << "Game started in room " << _roomId << endl;

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(gameStartPkt);
	Broadcast(sendBuffer);
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
	BroadcastExcept(newPlayerBuffer, player->playerId);
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
