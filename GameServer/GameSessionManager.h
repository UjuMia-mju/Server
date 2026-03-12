#pragma once

class GameSession;
using GameSessionRef = shared_ptr<GameSession>;

class GameSessionManager 
{
public:
	void Add(GameSessionRef session);
	void Remove(GameSessionRef session);
	void Broadcast(SendBufferRef sendBuffer);

	GameSessionRef FindPlayerByNameTag(const string& name, int32 tag);
private:
	USE_LOCK;
	xset<GameSessionRef> _sessions;
};

extern GameSessionManager GSessionManager;