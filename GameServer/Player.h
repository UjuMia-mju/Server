#pragma once

class Player
{
public:
	uint64 playerId = 0;
	string name;
	Protocol::PlayerType type = Protocol::PlayerType::PLAYER_TYPE_NONE;
	GameSessionRef ownerSession = nullptr; //서로 알게 되면 사이클 발생 가능하긴 함.
};

