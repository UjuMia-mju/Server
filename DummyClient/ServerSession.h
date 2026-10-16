#pragma once
#include "pch.h"
#include "Session.h"

// 스테이지 하나를 가리키는 키 (S_STAGE_INFO 목록에서 받음)
struct StageKey
{
	int32 mapId = 0;
	int32 chapter = 0;
	int32 stage = 0;
};

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

	// PacketSession::Send를 가려서 보내는 패킷을 PacketStats에 집계 (Read/Write 비율)
	//   ServerSession 포인터로 호출해야 집계됨 (PacketSessionRef로 호출하면 빠짐)
	void Send(SendBufferRef sendBuffer);

	void SetPlayerInfo(const string& name, int32 tag, int32 playerId);
	bool IsLoginCompleted() const;
	const string& GetPlayerName() const;
	int32 GetPlayerTag() const;

	// 방 생성/입장 성공 시 방 ID 저장 (0 = 방 없음)
	void SetRoomId(uint64 roomId);
	uint64 GetRoomId() const;
	bool IsInRoom() const;

	// S_READY 수신 시 기록 (같은 방에서 레디한 플레이어 ID 목록)
	void OnReadyChanged(uint64 playerId, bool isReady);
	int32 GetReadyCount();
	// S_GAME_READY_TO_START 수신 = 방 전원 게임 로드 완료
	void SetGameReady();
	bool IsGameReady() const;

	// S_START_ROOM 수신 = 스테이지 선택 화면 진입
	void SetRoomStarted();
	bool IsRoomStarted() const;

	// S_STAGE_INFO 수신 (C_GET_DB_DATA 응답) = 선택 가능한 스테이지 목록
	void SetStageList(vector<StageKey> stages);
	vector<StageKey> GetStageList();
	bool HasStageList() const;

	// S_SHOW_STAGE 성공 응답 수
	void OnShowStageReceived();
	int32 GetShowStageCount() const;

	// S_HOST_SHOW_STAGE 수신 = 방장이 고른 스테이지 (0 = 아직 없음)
	void SetHostStage(int32 mapId);
	int32 GetHostStage() const;

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
	void SendStartStage(int32 map_id, int32 chapter, int32 stageIndex);
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

	mutex _readyLock;
	set<uint64> _readyPlayers;
	atomic<bool> _isGameReady = false;
	atomic<bool> _isRoomStarted = false;

	mutex _stageLock;
	vector<StageKey> _stageList;
	atomic<bool> _hasStageList = false;
	atomic<int32> _showStageCount = 0;
	atomic<int32> _hostStage = 0;
};

// 스트레스 테스트 통계용 (서버로부터 받은 패킷 수)
extern atomic<int64> GRecvPacketCount;
// 초대 알림 받고 자동 수락하기 전 대기 시간(ms). 기본 1000, 스트레스 테스트에서는 0
extern atomic<int32> GInviteAcceptDelayMs;

