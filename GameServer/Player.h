#pragma once

class GameSession;

class Player
{
public:
	uint64 playerId = 0;
	string name;
    int32 tag;

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