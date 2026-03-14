#pragma once
#include "Session.h"

class GameSession : public PacketSession
{
public:
	~GameSession()
	{
		cout << "~GameSession()" << endl;
	}

	virtual void OnConnected() override;
	virtual void OnDisconnected() override;
	virtual void OnRecvPacket(BYTE* buffer, int32 len) override;
	virtual void OnSend(int32 len) override;

public:
	PlayerRef GetPlayer() { return _player; }
	void SetPlayer(PlayerRef player) { _player = player; }
	
	PlayerInfoRef GetPlayerInfo() { return _playerInfo; }
	void SetPlayerInfo(PlayerInfoRef info) { _playerInfo = info; }

	weak_ptr<class Room> GetRoom() { return _room; }
	void SetRoom(weak_ptr<class Room> room) { _room = room; }

private:
	PlayerRef				_player;
	PlayerInfoRef			_playerInfo;
	weak_ptr<class Room>	_room;
};

