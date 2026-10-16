# 리뷰어를 위한 하이라이트

> 우주픽스 서버는 4인 팀(클라이언트 3, 서버 1)에서 **서버를 혼자 맡아** 만든 C++ IOCP 게임 서버입니다.
> 이 문서는 코드를 처음 보는 분이 **10분 안에 핵심 설계를 확인할 수 있도록** 볼 곳과 순서를 정리한 것입니다.

- 포트폴리오 (노션): [이윤형 포트폴리오](https://political-minnow-95d.notion.site/YunHyeong-Lee-16b076a1d5e98068b985da37e737538e) → Would You Fix 페이지에 항목별 상세 정리
- 플레이 영상: [YouTube](https://www.youtube.com/watch?v=Z0QKo3amG5g)

---

## 1. 무엇이 강의 기반이고, 무엇이 직접 설계인가

| 구분 | 범위 | 비고 |
| --- | --- | --- |
| **강의 기반 (원리 학습 후 구현)** | `ServerCore/` — IOCP 코어, 세션, 송수신 버퍼, RW SpinLock, 메모리 풀, 잡 큐·잡 타이머, DB 커넥션 풀 / `Tools/PacketGenerator` | 아래 2-④처럼 **비정상 경로를 직접 보강**한 지점이 있습니다 |
| **직접 설계·구현** | `GameServer/` 전체 — 계정·방·초대 세션, Relay, 요청별 상태 검증, 관리자 채널, 뽑기·스테이지·DB 캐시 / `DummyClient/` 부하 테스트 | 2-①②③⑥, 2-⑤의 측정 도구와 판단 |

---

## 2. 추천하는 확인 순서

### ① NAT 문제를 푼 자체 Relay — 서버는 내용을 열지 않는다

초기 P2P 구조가 외부 네트워크에서 NAT·방화벽에 막혀 다수 실패해, Unity Relay·UDP 홀 펀칭과 비교한 뒤 기존 C++ 서버와 Protobuf를 유지하는 자체 Relay를 택했습니다. 서버는 payload를 역직렬화하지 않고 **누구에게 보낼지만** 정합니다.

| 볼 곳 | 확인 포인트 |
| --- | --- |
| [`Handle_C_RELAY_PACKET`](../GameServer/ClientPacketHandler.cpp#L140-L158) | 방 소속을 먼저 검증하고, 보낸 사람 ID는 클라이언트 값이 아니라 **세션 기준으로 서버가 붙임** |
| [`Room::RelayPacket`](../GameServer/Room.cpp#L312-L376) | 방장 권한 기준 라우팅 (멤버 입력 → 방장 / 방장 판정 → 전원 / 일반 동기화 → 나 제외 전원). **락 안에서는 받을 세션만 고르고, 전송은 락 밖에서** |

### ② 요청별로 필요한 상태를 한곳에서 검증

핸들러마다 제각각이던 로그인·방 소속 확인을, 요청이 필요로 하는 상태(로그인 전 / 로그인 / 방 안)로 정의하고 핸들러 앞단의 공통 검증으로 통일했습니다. 새 핸들러를 추가할 때 검증이 빠질 여지를 구조로 없앴습니다.

| 볼 곳 | 확인 포인트 |
| --- | --- |
| [`AuthLevel` · `ValidateAuth`](../GameServer/AuthValidator.h#L6-L50) | 상태 단계 정의와 검증 |
| [`CHECK_AUTH_LOGIN` · `CHECK_AUTH_ROOM`](../GameServer/ClientPacketHandler.cpp#L49-L73) | 핸들러 첫 줄에서 쓰는 공통 매크로. 방 안 요청은 방 객체까지 꺼내 줌 |

### ③ 초대 → 수락 → 방 세션

방 상태 변경은 방 단위 잡 큐(`Room : JobQueue`, `DoAsync`)로 직렬화하고, 초대는 **수락하는 시점에 다시 검증**합니다.

| 볼 곳 | 확인 포인트 |
| --- | --- |
| [`InviteManager::AcceptInvite`](../GameServer/InviteManager.cpp#L80-L126) | 초대를 보낸 뒤 방이 사라지거나 가득 찼을 수 있으므로 수락 시점에 재검증. 기존 방 퇴장과 새 방 입장은 각 방의 잡 큐로 |

### ④ 강의 코어에서 직접 보강한 비정상 경로

강의 코어는 정상적인 접속·종료 흐름 기준이라, 실제 게임 서버로 쓰면서 정상이 아닌 입력과 종료를 따라가며 보강했습니다.

| 볼 곳 | 확인 포인트 |
| --- | --- |
| [`PacketSession::OnRecv`](../ServerCore/Session.cpp#L351-L389) | 크기 필드가 헤더보다 작은 패킷(예: 0)이면 조립 루프가 전진하지 못해 워커 스레드가 묶이던 경로를 차단 |
| [`IocpCore::Dispatch`](../ServerCore/IocpCore.cpp#L25-L63) | `GetQueuedCompletionStatus` 실패 시 이벤트를 확인 후 정상 종료 경로로 정리 |
| [`DBConnectionPool::HandleKeepAlive`](../ServerCore/DBConnectionPool.cpp#L111-L130) · [전용 스레드](../GameServer/GameServer.cpp#L106-L119) | 유휴 DB 연결이 끊기지 않도록 30분마다 확인 |

### ⑤ 부하 테스트로 다시 판단한 락 구조

100명 동시 접속 부하 테스트에서 "읽기가 많으니 RW 락"이라는 통념을 패킷 기준과 락 기준으로 각각 재봤고, 두 결과가 정반대로 나왔습니다. 방 멤버 락만 읽기 위주였고, 재진입과 데드락 조기 발견을 이유로 기존 RW SpinLock을 유지했습니다.

| 볼 곳 | 확인 포인트 |
| --- | --- |
| [`Lock::WriteLock`](../ServerCore/Lock.cpp#L7-L54) (강의 기반) | 같은 스레드의 쓰기 재진입 허용, 타임아웃 시 즉시 크래시로 데드락 위치 확인 |
| [`Session::ProcessSend`](../ServerCore/Session.cpp#L294-L317) (강의 기반) | 재진입이 실제로 필요한 경로 (`WRITE_LOCK` 안에서 `RegisterSend`가 같은 락을 다시 잡음) |

**측정 방법 — 더미 클라이언트와 두 가지 통계**

| 볼 곳 | 확인 포인트 |
| --- | --- |
| [`GameFlowTest` 설정](../DummyClient/GameFlowTest.cpp#L14-L27) · [`RunRelay`](../DummyClient/GameFlowTest.cpp#L237-L343) | 100명 로그인 → 4명씩 초대·입장 → 레디 → 스테이지 선택 → 게임 시작 → 클라이언트당 초당 10~20회 Relay를 30초간. 실제 게임 흐름을 그대로 밟음 |
| [`PacketStats`](../DummyClient/PacketStats.h) | **패킷 기준** 통계: 보낸 요청을 서버 상태를 바꾸는지(Write) 조회만 하는지(Read)로 분류 |
| [`LockStats::OnLock`](../ServerCore/LockStats.cpp#L169-L180) · [`Lock`에 연결한 지점](../ServerCore/Lock.cpp#L12-L14) · [1초 출력](../GameServer/GameServer.cpp#L121-L148) | **락 기준** 통계: 락을 쓰는 클래스별 READ/WRITE 호출 수. 카운터를 스레드마다 따로 둬 집계 자체가 경합을 만들지 않게 함 |
| [`RelayStats`](../DummyClient/RelayStats.h) | payload 앞 8바이트에 보낸 시각을 넣어 Relay 구간 지연 측정 |

재현: `DummyClient/DummyClient.cpp`의 `RUN_TEST`를 `GameFlow`로 두고 서버와 함께 실행합니다. 락 통계는 `ServerCore/LockStats.h`의 `USE_LOCK_STATS`로 켜고 끕니다.

**측정 결과 (100명, 방 25개, 30초)**

| 항목 | 값 |
| --- | --- |
| Relay 처리량 | 45,190건 (서버 전체 초당 약 1,500건, 전체 패킷의 98.4%) |
| Relay 구간 지연 | p50 0.3~0.4ms, p99 대부분 1ms 안팎 |
| CPU 사용률 | i5 기준 7% 미만 |
| 방 락 읽기 : 쓰기 | 약 46,000 : 325 (읽기 99% 이상) |

### ⑥ 운영 — 게임 포트와 분리한 관리자 채널

| 볼 곳 | 확인 포인트 |
| --- | --- |
| [관리자 서비스 시작](../GameServer/GameServer.cpp#L96-L104) | 같은 IOCP 코어를 쓰되 게임 포트와 다른 포트로 분리 |
| [`AdminSession`](../GameServer/AdminSession.h#L63-L110) | 잡 타이머로 1초마다 동시 접속·메모리·활성 방 수·초당 송수신 패킷을 WPF 모니터링 툴에 전송 |

---

## 3. 알고 있는 한계

- **인게임 판정은 방장 클라이언트:** 서버는 경로만 정하므로, 방장 조작과 방장 이탈에는 취약합니다. 규모를 키운다면 핵심 판정을 서버로 옮기고 방장 이탈 시 권한을 넘기는 구조가 필요합니다.
- **재진입 락에 의존하는 경로:** `ProcessSend`처럼 같은 락을 다시 잡는 경로가 있어 표준 `shared_mutex`로 바로 바꿀 수 없습니다. 락 범위를 나누면 재진입 의존을 없앨 수 있습니다.
- **부하 테스트 규모:** 100명은 서버 세션 상한(그리고 테스트용 DB 계정 수)에 맞춘 값입니다. 그 이상은 측정하지 않았고, 더미 클라이언트가 서버와 같은 PC에서 돌아 네트워크 지연은 포함되지 않습니다.
