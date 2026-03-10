#pragma once
#include "pch.h"
#include "Session.h"

class ServerSession : public PacketSession
{
public:
	ServerSession (const string& email, const string& password);
	~ServerSession();

	virtual void OnConnected() override;
	virtual void OnRecvPacket(BYTE* buffer, int32 len) override;
	virtual void OnSend(int32 len) override;
	virtual void OnDisconnected() override;

	void SetPlayerInfo(const string& name, int32 tag, int32 playerId);
	bool IsLoginCompleted() const;
	const string& GetPlayerName() const;
	int32 GetPlayerTag() const;

	void SendLoginPacket(string id, string psw);
	void SendCreateRoom();
	void SendInvitePacket(string name, int32 tag);

private:
	string _email;
	string _password;
	string _playerName;
	int32 _playerTag = 0;
	int32 _playerId = 0;
	bool _isLoggedIn = false;
};

