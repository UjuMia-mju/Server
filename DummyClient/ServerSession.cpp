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