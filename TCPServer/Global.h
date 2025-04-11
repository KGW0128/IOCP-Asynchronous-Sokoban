#pragma once
#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>


#define SERVERPORT 9000
#define BUFSIZE    4096

//�������� ������ �÷��̾� ��
#define PLAYER_COUNT 100


//Ŭ���̾�Ʈ���� ���� �޼�����
#define INTRO_MSG "���ڹ� ���� �Դϴ�.\n1.���ӽ���\n2.����\n"
#define GAME_CLEAR_MSG "���� �ܰ踦 �Ͻðڽ��ϱ�?\n1.����\n2.����\n"

#define OUT_MSG "\n������ �����մϴ�...\n"
#define ALL_CLEAR_MSG "\n��� ���������� Ŭ���� �ϼ̽��ϴ�!!!\n������ �����մϴ�...\n"
#define GAME_START_MSG "\n���ڹ� ������ ���۵Ǿ����ϴ�."
#define INITIALIZATION_MSG "\n���� �ʱ�ȭ �Ͽ����ϴ�."

#define LEFT_MOVE_MSG "\n�÷��̾� �������� �̵�"
#define RIGHT_MOVE_MSG "\n�÷��̾� ���������� �̵�"
#define UP_MOVE_MSG "\n�÷��̾� �������� �̵�"
#define DOWN_MOVE_MSG "\n�÷��̾� �Ʒ������� �̵�"


#define MAP_X 20		//���ڹ� ���� ����
#define MAP_Y 11		//���ڹ� ���� ����
#define STAGE_COUNT 3	//���ڹ� ���� ����

//Ŭ���̾�Ʈ ���� ��������
enum RESULT
{
	NODATA = -1,
	SOCOBAN_PLAY = 1,
	GAME_OUT,
	NEXT_GAME
};

//�������� �������� Ŭ���̾�Ʈ �ൿ ��������
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


//���� ���� enum
enum
{
	SOC_ERROR = 1,
	SOC_TRUE,
	SOC_FALSE
};

//�������� state
enum STATE
{
	INIT_STATE=1,
	MENU_STATE,
	GAME_STATE
};

//IOCP Ÿ�� ����
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

//Ŭ���̾�Ʈ ����
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

	int stage_num = 0;				//�������� ��ȣ
	int move_count = 0;				//���� ���۽� ������ Ƚ��
	int sand_stage[MAP_Y][MAP_X];	//���� ��
	int p_x=0, p_y=0;				//���� �÷��̾� ��ǥ
	int standing_position;			//���� �÷��̾� ���ִ� ��ġ ����
};

void err_quit(const char* msg);		// ���� �Լ� ���� ��� �� ����
void err_display(const char* msg);	// ���� �Լ� ���� ���

void ErrorPostQueuedCompletionStatus(SOCKET _sock);		//��Ʈ ����
void AcceptPostQueuedCompletionStatus(SOCKET _sock);	//IO_ACCEPT ����


PROTOCOL GetProtocol(const char* _ptr);	// �������� �и�
int PackPacket(char* _buf, PROTOCOL _protocol, const char* _str1);
int PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, const char* _str1);
int PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, const char* _str1, _ClientInfo* _ptr);
int PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, const char* _str1, int _stage, int _move);

void UnPackPacket(const char* _buf, int& _data);

_ClientInfo* AddClientInfo(SOCKET _sock); //Ŭ���̾�Ʈ ���ӽ� ù ���� ����
void RemoveClientInfo(_ClientInfo* _ptr); //Ŭ���̾�Ʈ ����� ��������

void Accepted(SOCKET _sock);					//�������� ����

void IntroProcess(_ClientInfo* _ptr);			//��Ʈ�� ����
void LeftMoveProcess(_ClientInfo* _ptr);		//���� ������
void RightMoveProcess(_ClientInfo* _ptr);		//������ ������
void UpMoveProcess(_ClientInfo* _ptr);			//���� ������
void DownMoveProcess(_ClientInfo* _ptr);		//�Ʒ��� ������
void InitializationProcess(_ClientInfo* _ptr);	//�ʱ�ȭ
void GameClearMenuProcess(_ClientInfo* _ptr);	//�������� Ŭ����
void GameStartProcess(_ClientInfo* _ptr);		//���� ����
void PlayerOutProcess(_ClientInfo* _ptr);		//Ŭ���̾�Ʈ ����

bool Recv(_ClientInfo* _ptr);//����� ���� ������ ���� �Լ�
int CompleteRecv(_ClientInfo* _ptr, int _completebyte);//recvn

bool Send(_ClientInfo* _ptr, int _size);//����� ���� ������ �۽� �Լ�
int CompleteSend(_ClientInfo* _ptr, int _completebyte);//sand

void CompleteRecvProcess(_ClientInfo*);//���� �������� ����
void CompleteSendProcess(_ClientInfo*);//���� �������� ����

DWORD WINAPI WorkerThread(LPVOID arg);//���� ������


#ifdef MAIN

//������ Ŭ���̾�Ʈ ���� ����info
_ClientInfo* ClientInfo[PLAYER_COUNT];

//���� ������ �÷��̾��
int Count = 0;

//���ڹ� ��
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