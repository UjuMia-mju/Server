#pragma once

// 0으로 바꾸면 Lock에서 카운트하는 코드가 빠짐 (측정용)
#define USE_LOCK_STATS 1

// =========================================================
// 락 호출 통계 (READ_LOCK / WRITE_LOCK을 쓰는 클래스별)
//   - Lock::ReadLock / WriteLock에서 OnLock 호출 -> 클래스 이름(typeid)별로 카운트
//   - 재진입(같은 스레드가 이미 잡은 락을 다시 잡음)도 호출 1번으로 셈
//   - 카운터는 스레드마다 따로 둬서 집계 자체가 경합을 만들지 않게 함
//   - 로그는 [Lock] 태그 (더미 클라이언트의 요청 분류 로그는 [Packet])
// =========================================================
namespace LockStats
{
	// name = typeid(this).name() (READ_LOCK / WRITE_LOCK 매크로가 넘겨줌)
	void OnLock(const char* name, bool isWrite);

	// 마지막 호출 이후 클래스별 락 호출 수 한 줄
	//   "[Lock] 1s | All R 1500 / W 4600 (R 24.6%) | Room R 1500 / W 0 | Session R 0 / W 4500"
	//   호출할 때마다 기준점을 지금으로 초기화, 워커 루프의 idle 락(JobTimer, LockQueue)만 있으면 빈 문자열
	string TakeRateLine(int32 intervalSec);

	// 서버 시작부터 누적 (같은 형식, "[Lock] total | ...")
	string TotalLine();
}
