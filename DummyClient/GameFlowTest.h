#pragma once

// =========================================================
// 방 생성 -> 초대 -> 레디 -> 방 시작 -> 스테이지 선택 -> 스테이지 시작 -> 릴레이 전체 흐름 테스트
//   - 설정(인원/그룹 크기/릴레이 횟수/시간 등)은 GameFlowTest.cpp 맨 위 상수에서 변경
//   1~3. Login / Create / Join : InviteFlowTest::SetupRooms 그대로 사용
//   (엔터 입력 대기: 방이 다 만들어진 상태를 서버에서 확인 가능)
//   4. Ready : 방 인원 전원(호스트 포함) C_READY -> 호스트가 S_READY를 전원분 받으면 완료
//   5. Start      : 호스트 C_START_ROOM -> 방 전원이 S_START_ROOM 받으면 완료 (스테이지 선택 화면)
//   6. StageList  : 전원 C_GET_DB_DATA -> S_STAGE_INFO로 스테이지 목록 받으면 완료
//      ShowStage  : 호스트만 생각 시간을 두고 랜덤 스테이지를 C_SHOW_STAGE로 N번 확인 (게스트는 보기만 함)
//      HostShow   : 호스트가 하나 골라 C_HOST_SHOW_STAGE -> 방 전원이 받으면 완료
//   7. StageStart : 호스트 C_START_STAGE -> S_START_STAGE -> (자동) C_ENTER_GAME
//                   -> 방 전원이 S_GAME_READY_TO_START 받으면 완료
//   8. Relay      : 게임 시작한 전원이 초당 10~20회(1초마다 다시 뽑음, 간격 ±50%) C_RELAY_PACKET -> 같은 방 나머지에게 전달, 지연 측정
// =========================================================
namespace GameFlowTest
{
	void Run();
}
