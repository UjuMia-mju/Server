#pragma once

struct InviteInfo
{
    uint64 inviteId;
    uint64 roomId;
    string roomName;

    uint64 inviterId;
    string inviterName;
    int32 inviterTag;

    uint64 inviteeId;
    string inviteeName;
    int32 inviteeTag;

    int64 expireTime;  // 만료 시간 (타임스탬프)
};

class InviteManager
{
private:
    InviteManager() = default;

public:
    static InviteManager& Instance()
    {
        static InviteManager instance;
        return instance;
    }

    uint64 CreateInvite();
    shared_ptr<InviteInfo> FindInvite(uint64 inviteId);
    void RemoveInvite(uint64 inviteId);
    void CleanupExpiredInvites();

private:
    USE_LOCK;
    unordered_map<uint64, shared_ptr<InviteInfo>> _invites;
    uint64 _nextInviteId = 1;
};

