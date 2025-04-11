#pragma once
#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>


#define SERVERPORT 9000
#define BUFSIZE    4096

//서버입장 가능한 플레이어 수
#define PLAYER_COUNT 100


//클라이언트에게 보낼 메세지들
#define INTRO_MSG "소코반 게임 입니다.\n1.게임시작\n2.종료\n"
#define GAME_CLEAR_MSG "다음 단계를 하시겠습니까?\n1.진행\n2.종료\n"

#define OUT_MSG "\n게임을 종료합니다...\n"
#define ALL_CLEAR_MSG "\n모든 스테이지를 클리어 하셨습니다!!!\n게임을 종료합니다...\n"
#define GAME_START_MSG "\n소코반 게임이 시작되었습니다."
#define INITIALIZATION_MSG "\n맵을 초기화 하였습니다."

#define LEFT_MOVE_MSG "\n플레이어 왼쪽으로 이동"
#define RIGHT_MOVE_MSG "\n플레이어 오른쪽으로 이동"
#define UP_MOVE_MSG "\n플레이어 위쪽으로 이동"
#define DOWN_MOVE_MSG "\n플레이어 아래쪽으로 이동"


#define MAP_X 20		//소코반 맵의 가로
#define MAP_Y 11		//소코반 맵의 세로
#define STAGE_COUNT 3	//소코반 맵의 갯수

//클라이언트 내부 프로토콜
enum RESULT
{
	NODATA = -1,
	SOCOBAN_PLAY = 1,
	GAME_OUT,
	NEXT_GAME
};

//서버에서 지정해줄 클라이언트 행동 프로토콜
enum PROTOCOL
{
	REQ = 1,
	MENU_SET,
	PLAY_GAME,
	GAME_RESULT,
	PLAYER_OUT,
	LEFT_MOVE,
	RIGHT_MOVE,
	UP_MOVE,
	DOWN_MOVE,
	INITIALIZATION,
	GAME_CLEAR
};


//소켓 상태 enum
enum
{
	SOC_ERROR = 1,
	SOC_TRUE,
	SOC_FALSE
};

//서버전용 state
enum STATE
{
	INIT_STATE=1,
	MENU_STATE,
	GAME_STATE
};

//IOCP 타입 구분
enum IO_TYPE
{
	IO_RECV = 1,
	IO_SEND, 
	IO_DISCONNECT,
	IO_ACCEPT = -100,
	IO_ERROR = -200
};

struct _ClientInfo;

struct WSAOVERLAPPED_EX
{
	WSAOVERLAPPED overlapped;
	_ClientInfo* ptr;
	IO_TYPE       type;
};

//클라이언트 정보
struct _ClientInfo
{
	WSAOVERLAPPED_EX	r_overlapped;
	WSAOVERLAPPED_EX	s_overlapped;

	SOCKET			sock;
	SOCKADDR_IN		addr;
	STATE			state;
	bool			r_sizeflag;

	_ClientInfo*	part;
	RESULT			game_result;

	int				recvbytes;
	int				comp_recvbytes;
	int				sendbytes;
	int				comp_sendbytes;

	char			recvbuf[BUFSIZE];
	char			sendbuf[BUFSIZE];

	WSABUF			r_wsabuf;
	WSABUF			s_wsabuf;

	int stage_num = 0;				//스테이지 번호
	int move_count = 0;				//게임 시작시 움직인 횟수
	int sand_stage[MAP_Y][MAP_X];	//현재 맵
	int p_x=0, p_y=0;				//현재 플레이어 좌표
	int standing_position;			//현재 플레이어 서있는 위치 정보
};

void err_quit(const char* msg);		// 소켓 함수 오류 출력 후 종료
void err_display(const char* msg);	// 소켓 함수 오류 출력

void ErrorPostQueuedCompletionStatus(SOCKET _sock);		//포트 에러
void AcceptPostQueuedCompletionStatus(SOCKET _sock);	//IO_ACCEPT 설정


PROTOCOL GetProtocol(const char* _ptr);	// 프로토콜 분리
int PackPacket(char* _buf, PROTOCOL _protocol, const char* _str1);
int PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, const char* _str1);
int PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, const char* _str1, _ClientInfo* _ptr);
int PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, const char* _str1, int _stage, int _move);

void UnPackPacket(const char* _buf, int& _data);

_ClientInfo* AddClientInfo(SOCKET _sock); //클라이언트 접속시 첫 새팅 정보
void RemoveClientInfo(_ClientInfo* _ptr); //클라이언트 종료시 정보삭제

void Accepted(SOCKET _sock);					//소켓정보 셋팅

void IntroProcess(_ClientInfo* _ptr);			//인트로 전송
void LeftMoveProcess(_ClientInfo* _ptr);		//왼쪽 움직임
void RightMoveProcess(_ClientInfo* _ptr);		//오른쪽 움직임
void UpMoveProcess(_ClientInfo* _ptr);			//위로 움직임
void DownMoveProcess(_ClientInfo* _ptr);		//아래로 움직임
void InitializationProcess(_ClientInfo* _ptr);	//초기화
void GameClearMenuProcess(_ClientInfo* _ptr);	//스테이지 클리어
void GameStartProcess(_ClientInfo* _ptr);		//게임 시작
void PlayerOutProcess(_ClientInfo* _ptr);		//클라이언트 종료

bool Recv(_ClientInfo* _ptr);//사용자 정의 데이터 수신 함수
int CompleteRecv(_ClientInfo* _ptr, int _completebyte);//recvn

bool Send(_ClientInfo* _ptr, int _size);//사용자 정의 데이터 송신 함수
int CompleteSend(_ClientInfo* _ptr, int _completebyte);//sand

void CompleteRecvProcess(_ClientInfo*);//받은 프로토콜 구분
void CompleteSendProcess(_ClientInfo*);//보낸 프로토콜 구분

DWORD WINAPI WorkerThread(LPVOID arg);//메인 쓰레드


#ifdef MAIN

//저장할 클라이언트 정보 갯수info
_ClientInfo* ClientInfo[PLAYER_COUNT];

//현재 입장한 플레이어수
int Count = 0;

//소코반 맵
int arStage[STAGE_COUNT][MAP_Y][MAP_X] = {
	{
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,2,0,0,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,0,0,2,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,0,0,2,0,2,0,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,0,1,0,1,1,0,1,1,1,1,1,1,1,1,1,1,1},
		{1,0,0,0,1,0,1,1,0,1,1,1,1,1,0,0,3,3,1,1},
		{1,0,2,0,0,2,0,0,0,4,0,0,0,0,0,0,3,3,1,1},
		{1,1,1,1,1,0,1,1,1,0,1,1,1,1,0,0,3,3,1,1},
		{1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
	},
	{
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,3,3,0,0,1,0,0,0,0,0,1,1,1,1,1,1},
		{1,1,1,1,3,3,0,0,1,0,2,0,0,2,0,0,1,1,1,1},
		{1,1,1,1,3,3,0,0,1,2,1,1,1,1,0,0,1,1,1,1},
		{1,1,1,1,3,3,0,0,0,0,4,0,1,1,0,0,1,1,1,1},
		{1,1,1,1,3,3,0,0,1,0,1,0,0,2,0,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,0,1,1,2,0,2,0,1,1,1,1},
		{1,1,1,1,1,1,0,2,0,0,2,0,2,0,2,0,1,1,1,1},
		{1,1,1,1,1,1,0,0,0,0,1,0,0,0,0,0,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
	},
	{
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,4,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,0,2,1,2,0,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,0,2,0,0,2,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,2,0,2,0,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,0,2,0,1,0,1,1,1,1,1},
		{1,1,3,3,3,3,0,0,1,1,0,2,0,0,2,0,0,1,1,1},
		{1,1,1,3,3,3,0,0,0,0,2,0,0,2,0,0,0,1,1,1},
		{1,1,3,3,3,3,0,0,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
	}
};


CRITICAL_SECTION cs;
HANDLE hcp;

#else

extern _ClientInfo* ClientInfo[PLAYER_COUNT];
extern int Count;

extern CRITICAL_SECTION cs;
extern HANDLE hcp;

extern int arStage[STAGE_COUNT][MAP_Y][MAP_X];

#endif