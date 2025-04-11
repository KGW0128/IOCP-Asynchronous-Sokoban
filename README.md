
# 🧱 Sokoban Game Server - IOCP 기반 비동기 네트워크 게임 서버

> Windows IOCP(입출력 완료 포트) 기반의 고성능 비동기 서버에서 동작하는 콘솔 기반 소코반 게임입니다.  
> 최대 100명의 클라이언트가 동시에 접속하여 각자 독립적으로 게임을 진행할 수 있습니다.

## 제작기간
- 2022.06.10 ~ 2022.06.17(7일)


---

## 📌 프로젝트 개요

- **게임명**: 소코반 (Sokoban)
- **형태**: 콘솔 기반 퍼즐 게임 (서버-클라이언트 모델)
- **목표**: IOCP 기반 네트워크 프로그래밍 학습 및 고성능 서버 구조 설계 실습

---

## ⚙️ 개발 환경 및 기술 스택

| 항목          | 내용                                               |
|---------------|----------------------------------------------------|
| 언어          | C++                                                |
| 네트워크      | WinSock 2, TCP 소켓, IOCP (I/O Completion Port)    |
| 운영체제      | Windows 10                                        |
| 개발 환경     | Visual Studio 2019                                 |
| 멀티스레드    | CreateThread, CriticalSection, Worker Thread       |
| 입력 처리     | `_getch()`를 통한 실시간 키보드 입력               |
| 동기화        | Critical Section을 통한 스레드 간 자원 보호        |

---

## 🕹 주요 기능

- IOCP 기반 비동기 서버 구조
- 최대 **100명 동시 접속 처리**
- 각 유저별로 **맵 상태 독립 관리**
- 게임 내 플레이 방식:
  - 맵 이동 (W, A, S, D)
  - 맵 초기화 (R)
  - 게임 종료 (P)
  - 스테이지 클리어 및 진행
- 클라이언트와 서버 간 **패킷 기반 프로토콜 통신**
- 에러 및 연결 종료에 대한 **안정적인 예외 처리**

---

## 🏗 구조 설명

### 📁 서버 구성

| 파일명           | 설명                                          |
|------------------|-----------------------------------------------|
| `main.cpp`       | 서버 초기화 및 IOCP 스레드 풀 구성             |
| `WorkerThread.cpp`| IOCP 큐에서 이벤트 처리 (RECV, SEND 등)       |
| `Packet.cpp`     | Pack/Unpack 함수로 프로토콜 직렬화 처리        |
| `Global.h`       | 전역 설정, 구조체, enum, 맵 데이터 정의        |
| `SokobanMap`     | 고정된 스테이지 맵 3개 내장                    |
| `generalfunc.cpp`| 클라이언트 등록/삭제, 상태 분기, 게임 처리 로직 |





### 📁 클라이언트 구성

- TCP 소켓을 통해 서버에 연결
- 메뉴 선택 → 게임 시작 → 이동 → 스테이지 클리어 or 종료
- 콘솔 UI 기반 실시간 입력: `WASD` 이동, `R` 초기화, `P` 종료

---


## 🔁 서버 동작 흐름

1. 클라이언트 접속 시 `_ClientInfo` 생성 및 IOCP 등록
2. 이벤트 발생 시 `WorkerThread()` → `CompleteRecvProcess()`로 상태 분기
3. `INIT_STATE → MENU_STATE → GAME_STATE` 순으로 상태 전이
4. 이동 요청 처리 (`LeftMoveProcess()` 등), 맵 정보 전송
5. 게임 종료/클리어 처리 시 상태 초기화 및 클라이언트 해제

---

## 🧠 핵심 설계 요소

### ✅ 1. IOCP 기반 비동기 네트워크 구조
- `CreateIoCompletionPort`, `GetQueuedCompletionStatus`를 활용한 고성능 이벤트 기반 소켓 처리
- Overlapped I/O를 활용하여 블로킹 없이 수신/송신
- CPU 수에 따라 동적으로 스레드 풀 구성 (`WorkerThread`)

### ✅ 2. 상태 기반 FSM (Finite State Machine)
- 클라이언트 상태: `INIT_STATE`, `MENU_STATE`, `GAME_STATE`
- 서버는 상태와 프로토콜에 따라 처리 분기 (`CompleteRecvProcess()`)

### ✅ 3. 사용자 정의 패킷 직렬화
- `PackPacket()`, `UnPackPacket()` 함수로 다양한 프로토콜 구조 처리
- 문자열, 구조체, 맵 데이터까지 안전하게 직렬화/역직렬화

### ✅ 4. 클라이언트 세션 정보 객체화
- `_ClientInfo` 구조체로 클라이언트별 데이터 및 상태 관리
- 클라이언트 배열(`ClientInfo[]`)에 최대 100명까지 관리
- 연결/종료 시 객체 생성 및 정리 (`AddClientInfo`, `RemoveClientInfo`)

### ✅ 5. 스레드 동기화 및 예외 처리
- `CRITICAL_SECTION`을 이용한 동기화
- 연결 종료, 전송 실패, 예외 상황 시 `ErrorPostQueuedCompletionStatus()` 호출로 안전한 해제

### ✅ 6. 모듈화된 게임 처리 로직
- `LeftMoveProcess()`, `RightMoveProcess()` 등 명확히 분리된 이동 로직
- 클리어, 초기화, 종료 시 별도 함수로 로직 관리

### ✅ 7. 콘솔 기반 클라이언트 인터페이스
- `_getch()`를 사용하여 실시간 키 입력 처리
- 콘솔에 수신 맵을 텍스트 기반으로 시각화 (`DrawScreen()`)

---


