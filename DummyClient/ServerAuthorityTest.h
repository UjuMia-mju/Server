#pragma once

// =========================================================
// "서버는 클라이언트도, 시점도 믿지 않는다" 검증 테스트
//   실행: DummyClient.exe authority [시나리오 ...]   (시나리오를 안 적으면 전부 실행)
//   - enter  : 3인 방에 2명이 동시에 C_ENTER_ROOM -> 정확히 1명만 성공, 1명은 "Room is full" (작업 1)
//   - invite : 초대를 받은 뒤 방장이 게임 시작 -> 수락 실패, 원래 있던 방에 그대로 남음 (작업 1)
//   설정(인원/반복 횟수)은 ServerAuthorityTest.cpp 맨 위 상수에서 변경
// =========================================================
namespace ServerAuthorityTest
{
	// 전부 통과하면 0, 하나라도 실패하면 1
	int32 Run(const vector<string>& scenarios);
}
