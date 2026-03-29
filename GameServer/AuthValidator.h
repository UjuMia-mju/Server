#pragma once
#include "GameSession.h"
#include "PacketResult.h"

// 인증 레벨 정의
enum class AuthLevel
{
    NONE = 0,        // 인증 불필요 (로그인)
    LOGGED_IN = 1,   // 로그인 필요
    IN_ROOM = 2,     // 방에 있어야 함
};

// 인증 검증 헬퍼
class AuthValidator
{
public:
    static PacketResult ValidateAuth(GameSessionRef session, AuthLevel required)
    {
        // 로그인 체크
        if (required >= AuthLevel::LOGGED_IN)
        {
            if (!session->GetPlayer())
            {
                std::cout << "Auth failed: Not logged in" << endl;
				return PacketResult::Fail(PacketResultCode::NOT_LOGGED_IN);
            }
        }

        // 방 입장 체크
        if (required >= AuthLevel::IN_ROOM)
        {
            auto room = session->GetRoom().lock();
            if (!room)
            {
                std::cout << "Auth failed: Not in a room" << endl;
                return PacketResult::Fail(PacketResultCode::NOT_IN_ROOM);
            }
        }

        return PacketResult::Success();
    }

	// 방 객체를 반환하는 인증 검증 함수 (실패 시 nullptr 반환)
    static RoomRef GetRoomIfValid(GameSessionRef session, PacketResult* outResult = nullptr)
    {
        if (!session->GetPlayer())
        {
            std::cout << "Auth failed: Not logged in" << endl;
            if (outResult)
                *outResult = PacketResult::Fail(PacketResultCode::NOT_LOGGED_IN);
            return nullptr;
        }

        auto room = session->GetRoom().lock();
        if (!room)
        {
            std::cout << "Auth failed: Not in a room" << endl;
            if (outResult)
                *outResult = PacketResult::Fail(PacketResultCode::NOT_IN_ROOM);
            return nullptr;
        }

        if (outResult)
            *outResult = PacketResult::Success();
        return room;
    }
};

