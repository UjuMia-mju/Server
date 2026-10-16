#pragma once
#include "TestPhase.h"

// =========================================================
// 방 생성 + 초대 흐름 스트레스 테스트
//   - 설정(인원/그룹 크기/verbose)은 InviteFlowTest.cpp 맨 위 상수에서 변경
//   1. Login  : player1 ~ playerN 접속 후 동시에 로그인
//   2. Create : 4명씩 그룹, 그룹 첫 번째(player1, 5, 9, ...)가 호스트로 방 생성
//   3. Join   : 호스트가 나머지 3명을 이름#태그로 초대 -> 초대받은 쪽 자동 수락 -> 입장
//   단계마다 성공 수 / 걸린 시간 / 개별 지연(avg, p99, max)을 모아서 마지막에 리포트
// =========================================================
namespace InviteFlowTest
{
	void Run();

	// [공용] 접속 -> Login -> Create -> Join 까지 진행 (GameFlowTest에서도 사용)
	//   단계 결과는 results에 추가, 전원 입장까지 성공한 그룹만 반환
	//   allGroupCount에는 처음 나눈 전체 그룹 수를 넣어줌
	vector<TestPhase::Group> SetupRooms(int32 clientCount, int32 groupSize, vector<TestPhase::PhaseResult>& results, int32& allGroupCount, ostream& out);
}
