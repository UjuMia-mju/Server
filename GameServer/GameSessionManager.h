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

	int32 GetSessionCount()
	{
		READ_LOCK; // Lock 컨셉에 따라 달라질 수 있음 (혹은 WRITE_LOCK)
		return static_cast<int32>(_sessions.size());
	}
private:
	USE_LOCK;
	xset<GameSessionRef> _sessions;
};

extern GameSessionManager GSessionManager;