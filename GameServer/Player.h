#pragma once
#include <algorithm>

using namespace std;

class GameSession;

// =================== 인게임 캐릭터 정보 ====================
class Player
{
public:
	uint64 playerId = 0;
	string name;
    int32 tag;

	// 애니메이션 상태
    int32 animState = 0;

    // 위치 정보
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;

    // 회전 정보 (Quaternion)
    float rotX = 0.0f;
    float rotY = 0.0f;
    float rotZ = 0.0f;
    float rotW = 1.0f;

	weak_ptr<GameSession> ownerSession; // 사이클 방지용 약한 참조
};

extern Atomic<uint64> GPlayerId;
using PlayerRef = shared_ptr<Player>;

// =================== 유저 정보 ====================

class PlayerInfo
{
public:
	weak_ptr<GameSession> ownerSession; // 사이클 방지용 약한 참조

	void SetDbUserId(int32 id) { dbUserId = id; }
	int32 GetDbUserId() const { return dbUserId; }

	// ------------------------------------------
	// 1. 재화 (Goods) GET / SET
	// ------------------------------------------
	int32 GetCoin() const { return _coin; }
	int32 GetGem() const { return _gem; }

	void SetCoin(int32 coin) { _coin = max(0, coin); }
	void SetGem(int32 gem) { _gem = max(0, gem); }

	void AddCoin(int32 amount) { _coin = max(0, _coin + amount); }
	void AddGem(int32 amount) { _gem = max(0, _gem + amount); }

	// ------------------------------------------
	// 2. 보유 스킨 (Owned Skins) 확인 및 추가
	// ------------------------------------------
	bool HasSkin(int32 skinId) const
	{
		return _ownedSkins.find(skinId) != _ownedSkins.end();
	}

	void AddSkin(int32 skinId)
	{
		_ownedSkins.insert(skinId);
	}

private:
	int32 dbUserId = 0; // DB의 user_id (PK)
	// 외부에서 재화를 직접 조작(playerInfo->coin = -100)하지 못하도록 캡슐화
	int32 _coin = 0;
	int32 _gem = 0;

	// 유저가 이미 보유한 스킨의 ID 목록을 상수 시간(O(1))으로 찾기 위한 해시 테이블
	std::unordered_set<int32> _ownedSkins;
};