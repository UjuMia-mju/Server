#pragma once

// =========================================================
// 방 릴레이 스트레스 테스트
//   - 설정(인원/간격/payload/시간/verbose)은 RoomStressTest.cpp 맨 위 상수에서 변경
//   1. player1 ~ playerN 전부 로그인
//   2. 4명씩 짝지어 4의 배수(player4, player8, ...)가 방장으로 방 생성
//   3. 나머지 3명이 그 방에 입장
//   4. 모든 인원이 C_RELAY_PACKET을 반복 전송 -> 같은 방 나머지 3명에게 전달
//      payload 앞 8byte에 보낸 시각을 넣어서 받는 쪽에서 지연시간(p50/p99/max) 측정
// =========================================================
namespace RoomStressTest
{
	void Run();

	// S_RELAY_PACKET 수신 시 호출 (ServerPacketHandler)
	void OnRelayReceived(const string& payload);
}
