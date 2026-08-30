#pragma once

#include <windows.h>
#include <psapi.h>

#include "pch.h"
#include "GameSessionManager.h"
#include "RoomManager.h"

// --------------------------------------------------
// 1. 패킷 구조체 정의 (TCP 경계 처리 및 직렬화용)
// --------------------------------------------------
#pragma pack(push, 1)
struct PacketHeader
{
    uint16 size;
    uint16 id; // 예: PKT_S2C_ADMIN_SERVER_STATUS = 0x9001
};

struct PKT_S2C_AdminServerStatus
{
    PacketHeader header;
    int32  userCount;
    uint64 memUsageMB;
    int32  roomCount;
    int64  inPPS;
    int64  outPPS;
};
#pragma pack(pop)

class AdminSession : public PacketSession
{
private:
    atomic<bool> _authenticated = false;
    uint64 _timerId = 0; // 등록된 타이머 ID 관리를 통한 취소 처리

public:
    virtual void OnConnected() override
    {
        cout << "[Admin] Connected from client." << endl;
        // 접속 초기화: 통계치 리셋
        g_inboundPacketCount.store(0);
        g_outboundPacketCount.store(0);
        // 리포팅을 시작하는 타이머 등록
        ScheduleNextStatusUpdate();
    }

    virtual void OnDisconnected() override
    {
        cout << "[Admin] Disconnected. Canceling status updates." << endl;
        _authenticated = false;

        // 세션 종료 시 남아있는 타이머 예약 취소
        if (_timerId != 0)
        {
            _timerId = 0;
        }
    }

    virtual void OnRecvPacket(BYTE* buffer, int32 len) override
    {
        // 관리자 인증 처리 패킷 구현 (예: PKT_C2S_ADMIN_AUTH)
    }

    // --------------------------------------------------
    // 2. 타이머 기반 주기적 갱신 (JobTimer 이용)
    // --------------------------------------------------
    void ScheduleNextStatusUpdate()
    {
        if (IsConnected() == false)
            return;

        auto self = static_pointer_cast<AdminSession>(shared_from_this());

        // 1초 뒤에 SendServerStatus를 실행하도록 JobTimer에 예약
        // (프로젝트의 JobTimer 구현 방식에 맞춰 호출)
        _timerId = GJobTimer->Reserve(1000, [self]() {
            self->SendServerStatus();
            self->ScheduleNextStatusUpdate(); // 다음 1초 예약 (Self-rearming timer)
            });
    }

    void SendServerStatus()
    {
        if (IsConnected() == false)
            return;

        // 메트릭 수집
        int32 userCount = GSessionManager.GetSessionCount();
        uint64 memUsage = GetMemoryUsage();
        int32 roomCount = GetRoomCount();
        int64 inPPS = g_inboundPacketCount.exchange(0);
        int64 outPPS = g_outboundPacketCount.exchange(0);

        // 구조체 패킷 조립
        PKT_S2C_AdminServerStatus pkt;
        pkt.header.size = sizeof(PKT_S2C_AdminServerStatus);
        pkt.header.id = 0x9001; // PKT_S2C_ADMIN_SERVER_STATUS
        pkt.userCount = userCount;
        pkt.memUsageMB = memUsage;
        pkt.roomCount = roomCount;
        pkt.inPPS = inPPS;
        pkt.outPPS = outPPS;

        // 버퍼 복사 및 송신
        SendBufferRef sendBuffer = GSendBufferManager->Open(sizeof(pkt));
        ::memcpy(sendBuffer->Buffer(), &pkt, sizeof(pkt));
        sendBuffer->Close(sizeof(pkt));

        Send(sendBuffer);
    }

    uint64 GetMemoryUsage()
    {
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
        {
            return pmc.PrivateUsage / (1024 * 1024); // MB 단위
        }
        return 0;
    }

    int GetRoomCount()
    {
        return RoomManager::Instance().RoomCount();
    }
};