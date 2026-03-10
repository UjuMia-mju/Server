#include "pch.h"
#include "InviteManager.h"
#include "GameSession.h"
#include "ClientPacketHandler.h"
#include "RoomManager.h"
#include "Room.h"
#include "Player.h"

GameSessionRef InviteManager::FindPlayerByNameTag(const string& name, int32 targetTag)
{
	WRITE_LOCK;

	// TODO: 전역 세션 관리자 구현 필요
	// 현재는 임시로 nullptr 반환
	// 실제로는 SessionManager::Instance().FindSession(name, tag) 같은 방식으로 구현

	std::cout << "[InviteManager] FindPlayerByNameTag not fully implemented yet" << endl;
	std::cout << "  Looking for: " << name << "#" << targetTag << endl;

	return nullptr;
}

uint64 InviteManager::CreateInvite(uint64 roomId, const string& roomName,
	uint64 inviterId, const string& inviterName,
	const string& targetName, int32 targetTag)
{
	WRITE_LOCK;

	// 대상 플레이어 찾기
	GameSessionRef targetSession = FindPlayerByNameTag(targetName, targetTag);
	if (!targetSession || !targetSession->_player)
	{
		std::cout << "[InviteManager] Target player not found: " << targetName << "#" << targetTag << endl;
		return 0;
	}

	// 이미 방에 있는지 체크
	if (targetSession->_room.lock())
	{
		std::cout << "[InviteManager] Target player already in a room" << endl;
		return 0;
	}

	// 초대 ID 생성
	uint64 inviteId = _nextInviteId++;

	// 초대 정보 생성 및 저장
	auto inviteInfo = make_shared<InviteInfo>();
	inviteInfo->inviteId = inviteId;
	inviteInfo->roomId = roomId;
	inviteInfo->roomName = roomName;
	inviteInfo->inviterId = inviterId;
	inviteInfo->inviterName = inviterName;
	inviteInfo->inviteeName = targetName;
	inviteInfo->inviteeTag = targetTag;
	inviteInfo->targetSession = targetSession;

	_invites[inviteId] = inviteInfo;

	// 대상에게 초대 알림 전송
	Protocol::S_INVITE_NOTIFICATION notifyPkt;
	notifyPkt.set_room_id(roomId);
	notifyPkt.set_room_name(roomName);
	notifyPkt.set_inviter_name(inviterName);
	notifyPkt.set_invite_id(inviteId);

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(notifyPkt);
	targetSession->Send(sendBuffer);

	std::cout << "[InviteManager] Invite created - ID: " << inviteId
		<< ", Target: " << targetName << "#" << targetTag << endl;

	return inviteId;
}

bool InviteManager::AcceptInvite(uint64 inviteId, GameSessionRef accepter)
{
	WRITE_LOCK;

	auto it = _invites.find(inviteId);
	if (it == _invites.end())
	{
		std::cout << "[InviteManager] Invite not found: " << inviteId << endl;
		return false;
	}

	auto inviteInfo = it->second;

	// 방 찾기
	auto room = RoomManager::Instance().FindRoom(inviteInfo->roomId);
	if (!room)
	{
		std::cout << "[InviteManager] Room not found: " << inviteInfo->roomId << endl;
		_invites.erase(it);
		return false;
	}

	// 방이 가득 찼는지 체크
	if (room->GetCurrentCount() >= room->GetMaxCount())
	{
		std::cout << "[InviteManager] Room is full" << endl;
		_invites.erase(it);
		return false;
	}

	// 방에 입장
	accepter->_room = room;
	room->DoAsync(&Room::EnterLobby, accepter->_player);

	// 초대 정보 삭제
	_invites.erase(it);

	std::cout << "[InviteManager] Player " << accepter->_player->name
		<< " accepted invite " << inviteId << " and joined room " << inviteInfo->roomId << endl;

	return true;
}

bool InviteManager::DeclineInvite(uint64 inviteId)
{
	WRITE_LOCK;

	auto it = _invites.find(inviteId);
	if (it == _invites.end())
	{
		std::cout << "[InviteManager] Invite not found for decline: " << inviteId << endl;
		return false;
	}

	_invites.erase(it);
	std::cout << "[InviteManager] Invite " << inviteId << " declined" << endl;
	return true;
}