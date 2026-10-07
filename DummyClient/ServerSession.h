#pragma once
#include "pch.h"
#include "Session.h"

class ServerSession : public PacketSession
{
public:
	// autoLogin = false면 접속만 하고, 로그인은 SendLogin()으로 직접 보냄
	ServerSession (const string& email, const string& password, bool autoLogin = true);
	~ServerSession();

	virtual void OnConnected() override;
	virtual void OnRecvPacket(BYTE* buffer, int32 len) override;
	virtual void OnSend(int32 len) override;
	virtual void OnDisconnected() override;

	void SetPlayerInfo(const string& name, int32 tag, int32 playerId);
	bool IsLoginCompleted() const;
	const string& GetPlayerName() const;
	int32 GetPlayerTag() const;

	// 방 생성/입장 성공 시 방 ID 저장 (0 = 방 없음)
	void SetRoomId(uint64 roomId);
	uint64 GetRoomId() const;
	bool IsInRoom() const;

	// Packet senders
	void SendGetDbDataPacket();
	void SendLoginPacket(string id, string psw);
	void SendLogin(); // 생성자에서 받은 계정으로 로그인
	void SendCreateRoom();
	void SendInvitePacket(string name, int32 tag);
	void SendLeaveRoom();
	void SendReady(bool isReady);
	void SendStartRoom();
	void SendShowStage(int32 stage, int32 level);
	void SendEnterRoom();
	void SendStageData(int32 map_id, int32 chapter, int32 stageIndex);
	void SendMyClearStageData();
	void SendGachaPool();
	void SendGacha();
	void SendHostStageSelect(int32 map_id);
	void SendStartStage(int32 map_id);
	void SendStageEnter();
	void SendStageClear(int32 map_id, int32 star, int32 clearTime);

private:
	string _email;
	string _password;
	bool _autoLogin = true;
	string _playerName;
	int32 _playerTag = 0;
	int32 _playerId = 0;
	atomic<bool> _isLoggedIn = false;
	atomic<uint64> _roomId = 0;
};

// 스트레스 테스트 통계용 (서버로부터 받은 패킷 수)
extern atomic<int64> GRecvPacketCount;
// 초대 알림 받고 자동 수락하기 전 대기 시간(ms). 기본 1000, 스트레스 테스트에서는 0
extern atomic<int32> GInviteAcceptDelayMs;

