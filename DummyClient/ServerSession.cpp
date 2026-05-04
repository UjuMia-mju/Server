#include "pch.h"
#include "ServerSession.h"
#include "ServerPacketHandler.h"

ServerSession::ServerSession(const string& email, const string& password)
	: _email(email), _password(password)
{
}

ServerSession::~ServerSession()
{
	cout << "~ServerSession()" << endl;
}

void ServerSession::OnConnected()
{
	this_thread::sleep_for(200ms);

	SendLoginPacket(_email, _password);
}

void ServerSession::OnRecvPacket(BYTE* buffer, int32 len)
{
	PacketSessionRef session = GetPacketSessionRef();
	ServerPacketHandler::HandlePacket(session, buffer, len);
}

void ServerSession::OnSend(int32 len)
{
	// Optional: Add logging if needed
}

void ServerSession::OnDisconnected()
{
	// Optional: Add cleanup if needed
}

void ServerSession::SetPlayerInfo(const string& name, int32 tag, int32 playerId)
{
	_playerName = name;
	_playerTag = tag;
	_playerId = playerId;
	_isLoggedIn = true;
}

bool ServerSession::IsLoginCompleted() const
{
	return _isLoggedIn;
}

const string& ServerSession::GetPlayerName() const
{
	return _playerName;
}

int32 ServerSession::GetPlayerTag() const
{
	return _playerTag;
}

void ServerSession::SendGetDbDataPacket()
{
	Protocol::C_GET_DB_DATA pkt;
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendLoginPacket(string id, string psw)
{
	Protocol::C_LOGIN loginPkt;
	loginPkt.set_userid(id);
	loginPkt.set_psw(psw);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(loginPkt);
	Send(sendBuffer);

	cout << "[Client] Login packet sent - Email: " << id << endl;
}

void ServerSession::SendCreateRoom()
{
	Protocol::C_CREATE_ROOM pkt;
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);

	cout << "[Client] Create room packet sent" << endl;
}

void ServerSession::SendInvitePacket(string name, int32 tag)
{
	Protocol::C_INVITE_PLAYER invitePkt;
	invitePkt.set_player_name(name);
	invitePkt.set_player_tag(tag);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(invitePkt);
	Send(sendBuffer);

	cout << "[Client] Invite packet sent - Target: " << name << "#" << tag << endl;
}

void ServerSession::SendLeaveRoom()
{
	Protocol::C_LEAVE_ROOM pkt;
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendEnterRoom()
{
	Protocol::C_ENTER_ROOM pkt;
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
	cout << "[Client] Show members packet sent" << endl;
}

void ServerSession::SendStageData(int32 map_id, int32 chapter, int32 stageIndex)
{
	Protocol::C_SHOW_STAGE pkt;

	pkt.set_chapter(chapter);
	pkt.set_map_id(map_id);
	pkt.set_stageindex(stageIndex);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendMyClearStageData()
{
	Protocol::C_GET_CLEAR_INFO pkt;

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendGachaPool()
{
	Protocol::C_GACHA_POOL_LIST pkt;
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendHostStageSelect(int32 map_id)
{
	Protocol::C_HOST_SHOW_STAGE pkt;
	pkt.set_map_id(map_id);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendStartStage(int32 map_id)
{
	Protocol::C_START_STAGE pkt;

	pkt.set_map_id(map_id);
	pkt.set_chapter(1);
	pkt.set_stageindex(1);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendStageEnter()
{
	Protocol::C_ENTER_GAME pkt;

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendStageClear(int32 map_id, int32 star, int32 clearTime)
{
	Protocol::C_GAME_CLEAR pkt;
	pkt.set_map_id(map_id);
	pkt.set_star(star);
	pkt.set_clear_time_seconds(clearTime);
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendReady(bool isReady)
{
	Protocol::C_READY readyPkt;
	readyPkt.set_is_ready(isReady);
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(readyPkt);
	Send(sendBuffer);
	cout << "[Client] Ready packet sent - Ready: " << (isReady ? "Yes" : "No") << endl;
}

void ServerSession::SendStartRoom()
{
	Protocol::C_START_ROOM startPkt;
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(startPkt);
	Send(sendBuffer);
	cout << "[Client] Start room packet sent" << endl;
}


void ServerSession::SendShowStage(int32 chapter, int32 stage)
{
	Protocol::C_SHOW_STAGE pkt;
	pkt.set_chapter(chapter);
	pkt.set_stageindex(stage);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);

	cout << "[Client] Show stage packet sent - Stage " << chapter << "-" << stage << endl;
}

