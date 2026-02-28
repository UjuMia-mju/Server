#pragma once
#include "Room.h"


class RoomManager
{
private:
	RoomManager() = default;
	RoomManager(const RoomManager&) = delete;
	RoomManager& operator=(const RoomManager&) = delete;
public:
	static RoomManager& Instance()
	{
		static RoomManager instance;
		return instance;
	}

	uint64 GenerateRoomId();
	RoomRef CreateRoom(uint64 hostPlayerId, const string& hostName, int32 hostTag);

private:
	USE_LOCK;
	unordered_map<uint64, RoomRef> _rooms;
};