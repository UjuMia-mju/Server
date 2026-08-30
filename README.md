# 우주픽스 서버 프로젝트

## 🎬 Gameplay Video

<a href="https://www.youtube.com/watch?v=Z0QKo3amG5g">
  <img src="https://img.youtube.com/vi/Z0QKo3amG5g/hqdefault.jpg" alt="유튜브 영상 썸네일" width="600">
</a>

## 🖼️ Screenshots / Gallery

<table>
  <tr>
    <td align="center">
      <img src="https://github.com/user-attachments/assets/ea4018a2-40e8-4749-89bf-ae67dcf46d13" width="500" height="300" style="object-fit: cover;" alt="이미지 1"><br>
      <sub><b>시작 화면</b></sub>
    </td>
    <td align="center">
      <img src="https://github.com/user-attachments/assets/fc5e575f-325b-4202-b9f0-052a47ca3c52" width="500" height="300" style="object-fit: cover;" alt="이미지 2"><br>
      <sub><b>스테이지 선택 화면</b></sub>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="https://github.com/user-attachments/assets/1e756228-4f4a-4a27-911f-f111cd0ac5b0" width="500" height="300" style="object-fit: cover;" alt="이미지 1"><br>
      <sub><b>인게임 화면</b></sub>
    </td>
    <td align="center">
      <img src="https://github.com/user-attachments/assets/28d5df6e-848b-463d-8abb-5e3b6d8aba39" width="500" height="300" style="object-fit: cover;" alt="이미지 2"><br>
      <sub><b>클리어 화면</b></sub>
    </td>
  </tr>
</table>

## 1. 프로젝트 소개

이 프로젝트는 협동형 우주선 수리 게임인 "우주픽스"의 서버 백엔드입니다.

게임의 핵심 콘셉트는 다음과 같습니다.

- 2~4명의 플레이어가 한 팀을 이루어 우주선을 수리
- 행성 기반의 우주 정거장/행성 환경에서 자원 채집
- 다양한 도구와 제작 시스템을 활용해 자원 조합
- 상위 재료와 고급 부품을 제작해 우주선의 손상 부위 수리
- 제한 시간 안에 우주선을 복구하고 탈출하는 것이 목표

서버는 플레이어 인증, 방 생성/입장, 로비 준비, 게임 시작, 자원/재료/스킨 관리, 스테이지 정보 로드, DB 기반 계정/프로필 관리 등 게임 전반의 핵심 로직을 처리합니다.

---

## 2. 게임 개요

### 핵심 플레이 루프

1. 계정 로그인
2. 플레이어 정보 로드
3. 방 생성 또는 입장
4. 멤버 준비 상태 확인
5. 방장이 시작 시 게임 시작
6. 게임 내 동기화 및 패킷 릴레이
7. 리소스 획득, 상위 재료 제작, 우주선 수리
8. 스테이지 클리어 기록 저장 및 재접속 시 반영

### 주요 게임 요소

- 로그인 기반 플레이어 계정 시스템
- 방 생성/입장/준비/시작 시스템
- 멀티플레이어 동기화
- 스테이지/맵 메타데이터 로드
- 뽑기(Gacha) 시스템 및 스킨 보상
- 코인, 젬, 소유 스킨 데이터 관리
- 관리자 전용 서버 상태 모니터링

---

## 3. 서버 아키텍처 개요

이 서버는 단순히 패킷만 처리하는 구조가 아니라, 네트워크 IO, 세션 관리, 비즈니스 로직, DB 캐시, 관리자 모니터링이 분리된 계층형 구조를 갖습니다.

### 전체 구성

- ServerCore: 네트워크/IOCP 기반 서버 엔진
- GameServer: 실제 게임 로직과 프로토콜 핸들러
- Common/Protocol: Protobuf 기반 프로토콜 정의
- Database: MySQL 연결을 통한 계정, 플레이어 정보, 재화, 스킨 저장

### 아키텍처 다이어그램

<img width="1441" height="844" alt="image" src="https://github.com/user-attachments/assets/0d2fcd32-26b5-4e6a-835f-48a1e68d08f4" />

### WPF 모니터링 도구

<img width="817" height="515" alt="image" src="https://github.com/user-attachments/assets/ab3e5d9f-834a-47e5-973d-14aeb2d1856b" />


---

## 4. 서버 코어 구조

### 4.1 ServerCore

ServerCore는 네트워크 I/O와 기본 서버 운영에 필요한 핵심 기능을 담당합니다.

주요 구성 요소:

- IocpCore
  - IOCP 기반의 네트워크 이벤트 루프
  - 세션 등록 및 Dispatch 처리
- Service
  - 서버/클라이언트 서비스 추상화
  - Session 생성과 관리
- Listener
  - TCP 리스너 역할
  - 연결 수락 및 세션 생성
- Session
  - 개별 클라이언트 연결 컨텍스트
  - 패킷 수신/송신 관리
- BufferReader / BufferWriter
  - 패킷 데이터 직렬화/역직렬화 처리
- DBConnectionPool
  - MySQL 데이터베이스 커넥션 풀 관리
  - 동시 접근 제어 및 KeepAlive 처리
- ConfigManager
  - config.ini 기반 서버 설정 로드
- ThreadManager / JobQueue
  - 작업 스레드 분산 및 비동기 처리

### 4.2 서버 시작 흐름

서버는 GameServer.cpp의 main()에서 초기화됩니다.

초기화 절차:

1. config.ini 로드
2. DB 연결 풀 초기화
3. DB 캐시 데이터 로드
4. 패킷 핸들러 초기화
5. TCP ServerService 생성
6. 관리자 서버 포트 추가
7. 워커 스레드 실행
8. 메인 루프에서 IOCP 디스패치

실제 실행 구조는 다음과 같습니다.

- 메인 스레드: 서버 시작과 설정 로드
- 워커 스레드: IOCP Dispatch, 글로벌 작업 큐 및 예약 작업 처리
- DB 작업 스레드: 캐시 로딩 및 유지 관리
- 관리자 세션: 서버 상태 정기 전송

---

## 5. 게임 컨텐츠 구조

### 5.1 GameServer

GameServer 디렉터리는 실제 게임 로직을 구현하는 영역입니다.

주요 모듈:

- GameSession
  - 각 클라이언트 연결에 대응하는 세션 객체
  - Player, PlayerInfo, Room 참조 보유
- Player / PlayerInfo
  - 플레이어 상태와 프로필 정보 관리
  - 코인, 젬, 소유 스킨 상태 보관
- Room / RoomManager
  - 로비 방 생성, 멤버 관리, 준비 상태 관리
  - 최대 4명 방 구조
  - 방장 권한 및 시작 조건 처리
- GameSessionManager
  - 전체 연결 세션 추적
  - 브로드캐스트 전송용 관리
- ClientPacketHandler
  - 수신 패킷 분배
  - C_ / S_ 프로토콜 핸들러 연결
- AccountDB
  - 계정 검증 및 사용자 정보 조회
- AuthValidator
  - 로그인, 방 입장, 권한 검증
- GachaManager
  - 뽑기 풀, 스킨 메타데이터, 보상 처리
- StageManager
  - 스테이지 목록, 난이도, 클리어 기록 관리
- DBCacheManager
  - 스테이지, 뽑기 데이터 초기 캐싱
- AdminSession
  - 9000 포트에서 서버 상태 모니터링

### 5.2 플레이어와 세션 모델

GameSession은 커넥션 단위 객체이고, 실제 플레이어 상태는 Player 구조체와 PlayerInfo 구조체로 분리됩니다.

- Player
  - playerId, name, tag
  - 위치/회전 정보
  - ownerSession 참조
- PlayerInfo
  - DB 유저 ID
  - coin, gem
  - 소유 스킨 목록
  - 장착 여부 및 획득 시각 저장

이 구조를 통해 네트워크 세션 정보와 실제 게임 프로필 정보를 분리하여 관리하고 있습니다.

---

## 6. 방 시스템 구조

방 로직은 Room과 RoomManager를 중심으로 동작합니다.

### Room의 역할

- 방 생성 및 고유 ID 발급
- 멤버 추가/제거
- 준비 상태 관리
- 게임 시작 가능 여부 판단
- 방장 교체
- 멤버 브로드캐스트
- 패킷 릴레이

### 방 규칙

- 최대 인원: 4명
- 각 플레이어는 준비 상태를 갖고 있음
- 준비 완료 상태가 모두 true여야 게임 시작 가능
- 방장은 시작 권한을 가짐
- 방장이 퇴장하면 방이 파괴될 수 있음

### RoomManager

RoomManager는 전체 방 목록을 관리하며 다음 역할을 수행합니다.

- 방 생성
- 방 검색
- 방 삭제
- 현재 방 수 확인

---

## 7. 프로토콜 및 패킷 흐름

이 프로젝트는 Protobuf 기반 프로토콜을 사용합니다.

- Protocol.proto로 메시지 정의
- ClientPacketHandler에서 패킷 ID를 기준으로 핸들러 매핑
- 수신 패킷 처리 후 응답 패킷 생성 및 전송

예시 패킷 유형:

- 로그인: C_LOGIN / S_LOGIN
- 방 생성/입장: C_CREATE_ROOM / S_ENTER_ROOM
- 준비: C_READY / S_READY
- 게임 시작: S_START_ROOM
- 재화 조회: C_GET_CURRENCY / S_GET_CURRENCY
- 뽑기: C_GACHA / S_GACHA
- 스테이지 정보: C_GET_DB_DATA / S_STAGE_INFO
- 릴레이 패킷: C_RELAY_PACKET / S_RELAY_PACKET

핸들러는 AuthValidator를 통해 인증 상태를 확인한 뒤 처리합니다.

---

## 8. 인증/권한 시스템

AuthValidator는 서버에서 인증 상태를 강제하는 핵심 모듈입니다.

### 인증 단계

- NONE: 로그인 전
- LOGGED_IN: 로그인 완료
- IN_ROOM: 방 안에 있음

예를 들어 특정 패킷은 다음 조건을 만족해야 처리됩니다.

- 로그인이 되어 있어야 함
- 특정 동작은 방에 있어야 함
- 방장이 아닌 경우 일부 행동은 제한 가능

이 구조로 인해 비정상적인 패킷 또는 세션 상태에서의 비즈니스 로직 호출을 방지합니다.

---

## 9. 데이터베이스 및 캐시 구조

### 데이터베이스

- MySQL ODBC 연결 사용
- 계정 정보, 프로필 정보, 보유 스킨, 자원(코인/젬) 저장
- config.ini에서 DB 연결 문자열 로드

### 캐시

DBCacheManager를 통해 초기 로딩 시점에 자주 쓰는 데이터를 메모리에 캐싱합니다.

- GachaManager: 뽑기 풀, 스킨 메타데이터
- StageManager: 스테이지 정보

이 방식은 매번 DB를 조회하지 않고 빠르게 게임 로직을 처리할 수 있도록 합니다.

---

## 10. 관리자 모니터링

AdminSession은 별도 9000 포트에서 관리자용 상태 정보를 전송합니다.

관리자 모니터링 정보:

- 현재 연결 사용자 수
- 메모리 사용량
- 활성 방 수
- 패킷 입/출력량

이 기능은 실시간 서버 상태 확인과 운영 편의를 위한 구조입니다.

---

## 11. 코드 구조

```text
Server/
├── CMakeLists.txt
├── README.md
├── Server.sln
├── GameServer/
│   ├── AccountDB.cpp/.h
│   ├── AdminSession.cpp/.h
│   ├── AuthValidator.cpp/.h
│   ├── ClientPacketHandler.cpp/.h
│   ├── GameServer.cpp
│   ├── GameSession.cpp/.h
│   ├── GameSessionManager.cpp/.h
│   ├── GachaManager.cpp/.h
│   ├── InviteManager.cpp/.h
│   ├── Player.cpp/.h
│   ├── Room.cpp/.h
│   ├── RoomManager.cpp/.h
│   ├── StageManager.cpp/.h
│   ├── config.ini
│   ├── config.example.ini
│   └── Protocol.proto
├── ServerCore/
│   ├── IocpCore.cpp/.h
│   ├── Service.cpp/.h
│   ├── Listener.cpp/.h
│   ├── Session.cpp/.h
│   ├── BufferReader.cpp/.h
│   ├── BufferWriter.cpp/.h
│   ├── DBConnectionPool.cpp/.h
│   ├── ConfigManager.cpp/.h
│   ├── CoreGlobal.cpp/.h
│   └── JobQueue.cpp/.h
├── Common/
│   └── protoc-21.12-win64/
├── DummyClient/
├── Libraries/
├── build/
├── Binary/
└── docs/
    ├── architecture/
    │   └── README.md
    └── media/
        └── README.md
```

---

## 12. 실행 및 설정

### config.ini 예시

```ini
[Database]
Driver=MySQL ODBC 9.6 Unicode Driver
Server=localhost
Port=3306
Database=UjuMia
UID=root
PWD=your_password

[Server]
IP=127.0.0.1
Port=7777
WorkerThreads=5
```

### 실행 방식

- 서버 빌드 후 GameServer 실행
- 기본 포트는 7777
- 관리자 포트는 9000
- DB 연결 정보와 서버 IP/포트는 config.ini에서 조정

---

## 13. 기대 효과

이 서버 구조는 다음과 같은 장점을 가집니다.

- IOCP 기반 고성능 멀티플레이어 네트워크 처리
- 세션/게임 로직 분리로 유지보수 용이
- DB 캐싱으로 자주 쓰는 메타데이터 로드 속도 개선
- 방/플레이어/인증 구조를 통해 안정적인 게임 로직 구성
- 관리자용 모니터링으로 운영 중 서버 상태 확인 가능

---

## 14. 첨부 자료 위치

아래 폴더에 이후 게임 아키텍처 이미지, 영상, 스크린샷을 넣으면 됩니다.

- docs/architecture/
- docs/media/

예시:

- docs/architecture/architecture.png
- docs/media/gameplay_1.png
- docs/media/gameplay_2.png
- docs/media/trailer.mp4

---

## 15. 요약

우주픽스 서버는 단순한 네트워크 서버가 아니라, 멀티플레이어 협동 게임 운영을 위해 필요한 인증, 로비, 방, 동기화, 자원 시스템, 스테이지 관리, DB 연동, 관리자 모니터링이 함께 구성된 구조입니다.

특히 이 프로젝트는 다음 두 축으로 설계되어 있습니다.

- ServerCore: IOCP 기반 네트워크 엔진과 운영 인프라
- GameServer: 실제 게임 로직과 컨텐츠 처리

이 두 계층을 결합해 안정적이고 확장 가능한 온라인 게임 서버를 구성하고 있습니다.

---
