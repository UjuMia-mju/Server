#pragma once

// =========================================================
// 보낸 요청 패킷의 Read / Write 비율 집계 (모든 테스트 공용)
//   - ServerSession::Send에서 OnSend 호출 -> 헤더의 패킷 ID로 종류별 카운트
//   - 분류 기준: 서버 상태(메모리 or DB)를 바꾸면 Write, 조회만 하면 Read
//                상태 변경 없이 같은 방에 그대로 전달만 하면 Relay
//   - 테스트 끝날 때 Report로 Read:Write 비율 + 패킷별 비율 출력
//   - 로그는 [Packet] 태그 (서버의 락 호출 통계 로그는 [Lock])
// =========================================================
namespace PacketStats
{
	enum class Category { Read, Write, Relay, Unknown };

	// 보내기 직전 호출 (여러 스레드에서 호출됨)
	void OnSend(const SendBufferRef& sendBuffer);

	// 지금까지 보낸 분류별 누적 개수
	struct Totals
	{
		int64 read = 0;
		int64 write = 0;
		int64 relay = 0;
		int64 unknown = 0;
	};
	Totals GetTotals();

	// 1초 통계용: 마지막 호출 이후 보낸 개수 "[Packet] R 950/s, W 50/s, Relay 0/s"
	//   호출할 때마다 기준점을 지금으로 초기화 (누적 카운트는 그대로라 Report에는 영향 없음)
	//   1초 루프 시작 전에 한 번 호출해서 이전 단계 개수를 버림
	string TakeRate();

	// 누적 카운트 초기화 (특정 구간만 Report로 보고 싶을 때)
	void Reset();

	// 분류별 합계 / 비율 + 패킷별 개수 / 비율 표 출력
	// 지금 시점의 패킷 ID별 누적 개수 (구간별 Report용)
	using Snapshot = vector<int64>;
	Snapshot TakeSnapshot();

	//   title = 표 제목 (구간별로 나눠 찍을 때 구분용)
	//   since = 이 시점(TakeSnapshot) 이후에 보낸 개수만 집계, nullptr이면 전체
	void Report(ostream& out, const string& title = "REQUEST READ / WRITE RATIO", const Snapshot* since = nullptr);
}
