#pragma once

#include <windows.h>
#include <psapi.h>

#include "pch.h"
#include "GameSessionManager.h"
#include "RoomManager.h"

class AdminSession : public PacketSession
{
private:
	atomic<bool> _connected = false;
public:
	virtual void OnConnected() override
	{
		_connected = true;
		cout << "[Admin] Thread Loop start" << endl;

		// 1. 연결되는 순간 기존에 쌓였던 누적 통계치를 0으로 리셋 (첫 데이터 튐 방지)
		g_inboundPacketCount.store(0);
		g_outboundPacketCount.store(0);

		// 목적을 달성하고 바로 끊는 부분(Disconnect) 삭제
		auto self = static_pointer_cast<AdminSession>(shared_from_this());

		std::thread([self]() {
			// 클라이언트와 연결이 유지되는 동안 무한 루프
			while (self->_connected)
			{
				self->SendServerStatus();
				// 1초 대기
				std::this_thread::sleep_for(1s);
			}
			cout << "[Admin] Thread Loop Ended." << endl;
			}).detach(); // 메인 스레드와 분리되어 백그라운드에서 실행
	}

	virtual void OnDisconnected() override
	{
		// 끊어지면 타이머 루프도 종료되도록 플래그 변경
		cout << "[Admin] Disconnected. Stopping status updates." << endl;
		_connected = false;
	}

	virtual void OnRecvPacket(BYTE* buffer, int32 len) override
	{
		// 패킷 처리 로직 작성
	}

	// 기존 OnConnected에 있던 전송 로직을 별도 함수로 분리
	void SendServerStatus()
	{
		// 1초마다 호출되는 구간
		int32 userCount = GSessionManager.GetSessionCount();
		uint64 memUsage = GetMemoryUsage();
		int roomCount = GetRoomCount();

		// exchange(0)를 통해 현재까지 쌓인 값을 가져오고 0으로 리셋!
		// 1초마다 실행되므로 완벽한 PPS(Packet Per Second)가 됩니다.
		int64 inPPS = g_inboundPacketCount.exchange(0);
		int64 outPPS = g_outboundPacketCount.exchange(0);

		// 정보를 하나의 문자열로 조합 (마지막에 \n 같은 구분자를 넣어주면 클라이언트에서 끊어 읽기 편합니다)
		string status = to_string(userCount) +
			"|" + to_string(memUsage) +
			"|" + to_string(roomCount) +
			"|" + to_string(inPPS) +
			"|" + to_string(outPPS);

		// 데이터 전송
		SendBufferRef sendBuffer = GSendBufferManager->Open(4096);
		memcpy(sendBuffer->Buffer(), status.c_str(), status.length());
		sendBuffer->Close(status.length());

		Send(sendBuffer);
	}

	uint64 GetMemoryUsage() {
		PROCESS_MEMORY_COUNTERS_EX pmc;
		GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
		return pmc.PrivateUsage / 1024 / 1024; // MB 단위 변환
	}

	int GetRoomCount() {
		return RoomManager::Instance().RoomCount();
	}
};

