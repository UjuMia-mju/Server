#pragma once
#include "JobQueue.h"
#include "Protocol.pb.h"

// 방 입장 경로 (경로마다 클라이언트에게 보내는 응답 패킷이 다름)
enum class EnterSource
{
	Normal,		// C_ENTER_ROOM   -> S_ENTER_ROOM
	Invite,		// 초대 수락       -> S_ENTER_ROOM + S_INVITE_RESPONSE
};

class Room : public JobQueue
{
public:
	Room(uint64 roomId, const string& roomName, uint64 ownerId, string ownerString, int32 ownerTag);
	~Room();

	// ========== 대기실 관련 ==========
	void EnterLobby(PlayerRef player);      // 방 입장 (대기실, 방 생성 시 방장 입장)
	void TryEnterLobby(PlayerRef player, GameSessionRef session, EnterSource source); // 입장 검사 + 입장 (작업 큐 안에서 실행)
	void LeaveLobby(PlayerRef player);      // 방 퇴장
	void SetReady(uint64 playerId, bool isReady);  // 준비 상태
	bool CanStartGame();
	void StartGame(GameSessionRef requester); // 게임 시작 (방장/전원 준비/중복 시작을 큐 안에서 최종 확인)
	void MakeEnterRoomPacket(Protocol::S_ENTER_ROOM& pkt);
	
	// ========== 게임 시작 후, 스테이지 진행 관련 ==========
	// 테스트용
	void SetOwner(uint64 ownerId, const string& ownerName, int32 ownerTag);

	// ========== 게임 플레이 관련(실제 이동하는 씬) ==========
	void EnterGame(PlayerRef player);       // 게임 입장 (위치 동기화)

	// ========== 브로드케스트(공통) ==========
	void Broadcast(SendBufferRef sendBuffer);
	void BroadcastExcept(SendBufferRef sendBuffer, uint64 excludePlayerId);

	// ========== relay 관련 ==========
	void RelayPacket(PlayerRef sender, SendBufferRef sendBuffer, bool requireHostAuthority);

	// Getter
	uint64 GetRoomId() const { return _roomId; }
	string GetRoomName() const { return _roomName; }
	uint64 GetOwnerId() const { return _ownerId; }
	bool IsPlaying() const { return _isPlaying; }
	int32 GetCurrentCount() { READ_LOCK; return static_cast<int32>(_players.size()); } // 로그용. 이 값으로 입장 여부를 정하지 않는다
	int32 GetMaxCount() const { return MAX_ROOM_CAPACITY; }
	string GetOwnerName() const { return _ownerName; }
	int32 GetOwnerTag() const { return _ownerTag; }

	vector<pair<PlayerRef, bool>> GetMembersWithReadyStatus();
private:
	void BroadcastMemberEnter(PlayerRef player);
	void SendEnterRoomFail(GameSessionRef session, EnterSource source, const string& errorMsg);

private:
	USE_LOCK;

	static const int32 MAX_ROOM_CAPACITY = 4;
	uint64 _roomId = 0;
	string _roomName;
	// _ownerId, _isPlaying는 핸들러(큐 밖)에서도 빠른 실패/방장 확인용으로 읽으므로 atomic
	// (값을 바꾸는 쪽은 락 안, 최종 판정은 작업 큐 안에서 한다)
	atomic<uint64> _ownerId = 0;
	string _ownerName;
	int32 _ownerTag;
	atomic<bool> _isPlaying = false;
	bool _isDestroyed = false; // 방장 퇴장으로 파괴됨 (큐에 남아 있던 입장 요청을 거절하기 위함)

	unordered_map<uint64, PlayerRef> _players;
	unordered_map<uint64, bool> _readyStatus; // 방 준비 상태
	unordered_set<uint64> _loadedPlayers; // 게임 입장 상태 (게임 씬에서 사용)
};

RoomRef GetGlobalTestRoom();