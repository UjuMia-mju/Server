#include "pch.h"
#include "InviteManager.h"

uint64 InviteManager::CreateInvite()
{
	return uint64();
}

shared_ptr<InviteInfo> InviteManager::FindInvite(uint64 inviteId)
{
	return shared_ptr<InviteInfo>();
}

void InviteManager::RemoveInvite(uint64 inviteId)
{
}

void InviteManager::CleanupExpiredInvites()
{
}
