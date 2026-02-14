#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "GameSession.h"
#include "ClientPacketHandler.h"

shared_ptr<Room> GRoom = make_shared<Room>();

void Room::Enter(PlayerRef player)
{
	WRITE_LOCK;
	_players[player->playerId] = player;

	//// 새로 들어온 플레이어에게 기존 플레이어 정보 브로드캐스트
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

void Room::Leave(PlayerRef player)
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
