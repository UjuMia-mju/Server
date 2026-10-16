#pragma once

// =========================================================
// 릴레이 패킷 지연시간 측정 (RoomStressTest, GameFlowTest 공용)
//   - 보낼 때 payload 앞 8byte에 보낸 시각(us)을 넣고 (StampSendTime)
//   - 받을 때 현재 시각과 비교해서 지연을 기록 (OnReceived, S_RELAY_PACKET 핸들러에서 호출)
//   - 같은 프로세스에서 보내고 받기 때문에 steady_clock 시각을 그대로 비교 가능
// =========================================================
namespace RelayStats
{
	int64 NowUs();

	// payload 앞 8byte에 현재 시각 기록 (payload는 8byte 이상이어야 함)
	void StampSendTime(string& payload);

	// S_RELAY_PACKET 수신 시 호출 (IOCP 워커 스레드)
	void OnReceived(const string& payload);

	// 지금까지 모인 지연 샘플(us)을 가져가고 비움
	vector<int64> TakeSamples();
	int64 RecvCount();
	// 수신 카운트/샘플 초기화
	void Reset();

	// "p50 0.42ms / p99 1.80ms / max 3.10ms" (samples를 정렬함)
	string FormatLatency(vector<int64>& samples);
}
