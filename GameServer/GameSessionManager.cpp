#include "pch.h"
#include "GameSessionManager.h"
#include "GameSession.h"
#include "Player.h"

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

void GameSessionManager::Broadcast(SendBufferRef sendBuffer)
{
	WRITE_LOCK;
	for (GameSessionRef session : _sessions)
	{
		session->Send(sendBuffer);
	}
}

GameSessionRef GameSessionManager::FindPlayerByNameTag(const string& name, int32 tag)
{
	READ_LOCK;

	for (const auto& session : _sessions)
	{
		if (session->_player &&
			session->_player->name == name &&
			session->_player->tag == tag)
		{
			return session;
		}
	}

	return nullptr;
}