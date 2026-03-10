#pragma once
#include "JobQueue.h"

class Room : public JobQueue
{
public:
	Room(uint64 roomId, const string& roomName, uint64 ownerId);
	~Room();

	// ========== 대기실 관련 ==========
	void EnterLobby(PlayerRef player);      // 방 입장 (대기실)
	void LeaveLobby(PlayerRef player);      // 방 퇴장
	void SetReady(uint64 playerId, bool isReady);  // 준비 상태
	bool CanStartGame();
	void StartGame();                       // 게임 시작
	
	// ========== 게임 시작 후, 스테이지 진행 관련 ==========


	// ========== 게임 플레이 관련(실제 이동하는 씬) ==========
	void EnterGame(PlayerRef player);       // 게임 입장 (위치 동기화)
	void LeaveGame(PlayerRef player);       // 게임 퇴장

	// ========== 브로드케스트(공통) ==========
	void Broadcast(SendBufferRef sendBuffer);
	void BroadcastExcept(SendBufferRef sendBuffer, uint64 excludePlayerId);

	// Getter
	uint64 GetRoomId() const { return _roomId; }
	string GetRoomName() const { return _roomName; }
	uint64 GetOwnerId() const { return _ownerId; }
	bool IsPlaying() const { return _isPlaying; }
	int32 GetCurrentCount() const { return static_cast<int32>(_players.size()); }
	int32 GetMaxCount() const { return MAX_ROOM_CAPACITY; }
private:
	USE_LOCK;

	static const int32 MAX_ROOM_CAPACITY = 4;
	uint64 _roomId = 0;
	string _roomName;
	uint64 _ownerId = 0;
	bool _isPlaying = false;

	unordered_map<uint64, PlayerRef> _players;
	unordered_map<uint64, bool> _readyStatus;
};

RoomRef GetGlobalTestRoom();