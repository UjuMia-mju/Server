#include "pch.h"
#include "RoomManager.h"
#include <random>

uint64 RoomManager::GenerateRoomId()
{
	static random_device rd;
	static mt19937_64 gen(rd());
	uniform_int_distribution<uint64> dis;
	return dis(gen);
}

RoomRef RoomManager::CreateRoom(uint64 hostPlayerId, const string& hostName, int32 hostTag)
{
	uint64 newRoomId = GenerateRoomId();
	string roomName = hostName + "'s Room";
	RoomRef room = MakeShared<Room>(newRoomId, roomName, hostPlayerId);

	WRITE_LOCK;
	_rooms[newRoomId] = room;

	return room;
}
