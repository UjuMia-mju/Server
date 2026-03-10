#pragma once
#include "GameSession.h"

struct InviteInfo
{
    uint64 inviteId;
    uint64 roomId;
    string roomName;

    uint64 inviterId;
    string inviterName;
    int32 inviterTag;

    string inviteeName;
    int32 inviteeTag;

    GameSessionRef targetSession;
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

    // name + tag로 플레이어를 찾아서 초대 생성 및 알림 전송
    uint64 CreateInvite(uint64 roomId, const string& roomName,
        uint64 inviterId, const string& inviterName,
        const string& targetName, int32 targetTag);

    // 초대 수락 처리
    bool AcceptInvite(uint64 inviteId, GameSessionRef accepter);

    // 초대 거절 처리
    bool DeclineInvite(uint64 inviteId);

    // name + tag로 플레이어 세션 찾기
    GameSessionRef FindPlayerByNameTag(const string& name, int32 targetTag);


private:
    USE_LOCK;
    unordered_map<uint64, shared_ptr<InviteInfo>> _invites;
    uint64 _nextInviteId = 1;
};

