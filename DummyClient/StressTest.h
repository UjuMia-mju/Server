#pragma once

// =========================================================
// 스트레스 테스트
//   - 설정(인원/간격/시간/verbose)은 StressTest.cpp 맨 위 상수에서 변경
//   - 기본값: 100명 / 100ms / 60초 (시간 0 = 무한)
//   - 계정: player1@test.com ~ player100@test.com / 1234
//   - 로그인 완료 후 로비 패킷을 반복 전송하고 1초마다 송수신 통계 출력
// =========================================================
class ServerSession;

namespace StressTest
{
	const int32 MAX_ACCOUNT = 100; // DB에 있는 테스트 계정 수

	void Run();

	// [공용] 다른 스트레스 테스트에서도 사용
	// clientCount명 동시 접속 + 로그인 대기. i번째 = player(i+1) 세션 (로그인 실패 포함)
	vector<shared_ptr<ServerSession>> ConnectAndLogin(int32 clientCount, ostream& out);
	// 1 ~ MAX_ACCOUNT 범위로 보정
	int32 ClampClientCount(int32 clientCount, ostream& out);
}
