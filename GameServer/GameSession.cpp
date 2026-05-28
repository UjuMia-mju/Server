#include "pch.h"
#include "GameSession.h"
#include "GameSessionManager.h"
#include "ClientPacketHandler.h"
#include "Room.h"

void GameSession::OnConnected()
{
	GSessionManager.Add(static_pointer_cast<GameSession>(shared_from_this()));
}

void GameSession::OnDisconnected()
{
	PlayerRef player = _player;
	cout << "GameSession::OnDisconnected Call Player Leave Room" << endl;
	GSessionManager.Remove(static_pointer_cast<GameSession>(shared_from_this()));
	if (auto roomRef = _room.lock())
	{
		roomRef->DoAsync(&Room::LeaveLobby, player);
	}

	_player = nullptr;
}

void GameSession::OnRecvPacket(BYTE* buffer, int32 len)
{
	PacketSessionRef session = GetPacketSessionRef();
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
	// 나중에 헤더의 id의 대역폭을 보고 아 이거는 게임 서버, 이거는 로비 서버 등등을 처리
	ClientPacketHandler::HandlePacket(session, buffer, len);
}
void GameSession::OnSend(int32 len)
{
}