#pragma once
#include <string>
#include <functional>

// 패킷 처리 결과를 표현하는 타입
enum class PacketResultCode
{
    SUCCESS = 0,
    NOT_LOGGED_IN = 1,
    NOT_IN_ROOM = 2,
    ALREADY_IN_ROOM = 3,
    NOT_ROOM_OWNER = 4,
    NOT_ALL_READY = 5,
    CUSTOM_ERROR = 99,
};

// Result 패턴: 성공/실패 + 에러 메시지
class PacketResult
{
public:
	PacketResult() : _code(PacketResultCode::SUCCESS), _message("") {}
public:
    static PacketResult Success()
    {
        return PacketResult(PacketResultCode::SUCCESS, "");
    }

    static PacketResult Fail(PacketResultCode code, const std::string& message = "")
    {
        return PacketResult(code, message);
    }

    bool IsSuccess() const { return _code == PacketResultCode::SUCCESS; }
    PacketResultCode GetCode() const { return _code; }
    const std::string& GetMessage() const { return _message; }

private:
    PacketResult(PacketResultCode code, const std::string& message)
        : _code(code), _message(message)
    {
        if (_message.empty())
        {
            _message = GetDefaultMessage(code);
        }
    }

    static std::string GetDefaultMessage(PacketResultCode code)
    {
        switch (code)
        {
        case PacketResultCode::NOT_LOGGED_IN:
            return "Not logged in";
        case PacketResultCode::NOT_IN_ROOM:
            return "Not in a room";
        case PacketResultCode::ALREADY_IN_ROOM:
            return "Already in a room";
        case PacketResultCode::NOT_ROOM_OWNER:
            return "Only room owner can perform this action";
        case PacketResultCode::NOT_ALL_READY:
            return "Not all players are ready";
        default:
            return "Unknown error";
        }
    }

private:
    PacketResultCode _code;
    std::string _message;
};

// 에러 응답 전송 콜백 타입
using ErrorResponseSender = std::function<void(const std::string& errorMsg)>;