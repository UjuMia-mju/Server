#include "pch.h"
#include "GameSessionManager.h"
#include "GameSession.h"

GameSessionManager GSessionManager;

void GameSessionManager::Add(GameSessionRef session)
{
	WRITE_LOCK;
	_sessions.insert(session);
}

void GameSessionManager::Remove(GameSessionRef session)
{
	WRITE_LOCK;
	_sessions.erase(session);
}

void GameSessionManager::Brodcast(SendBufferRef sendBuffer)
{
	WRITE_LOCK;
	for (GameSessionRef sessoin : _sessions)
	{
		sessoin->Send(sendBuffer);
	}
}
