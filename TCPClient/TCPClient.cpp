#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h> //_getch() 용

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE    4096

#define MAP_X 20	//소코반 맵의 가로
#define MAP_Y 11	//소코반 맵의 세로

//클라이언트 내부 프로토콜
enum RESULT
{
	NODATA = -1,
	SOCOBAN_PLAY = 1,
	GAME_OUT,
	NEXT_GAME
};

//서버에서 받아온 프로토콜
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

char buf[BUFSIZE];				//정보를 담을 버퍼
int blank = 0;					//남은 구멍 횟수 확인
int sand_stage[MAP_Y][MAP_X];	//받아온 맵을 저장할 배열



void err_quit(char* msg);		// 소켓 함수 오류 출력 후 종료
void err_display(char* msg);	// 소켓 함수 오류 출력

int recvn(SOCKET s, char* buf, int len, int flags);// 사용자 정의 데이터 수신 함수
bool PacketRecv(SOCKET _sock, char* _buf);// recvn

PROTOCOL GetProtocol(char* _ptr);// 프로토콜 분리
int PackPacket(char* _buf, PROTOCOL _protocol);
int PackPacket(char* _buf, PROTOCOL _protocol, int _data);

void UnPackPacket(char* _buf, RESULT& _result, char* _str1);
void UnPackPacket(char* _buf, RESULT& _result, char* _str1, int& _stage, int& _move);
void UnPackPacket(char* _buf, char* _str1);

void DrawScreen();	//받아온 맵 그리기





int main(int argc, char *argv[])
{
	int retval;

	char msg[BUFSIZE];		//메세지를 담을 배열
	int move_count;			//움직인 횟수를 담을 정수
	int stage_num;			//스테이지 번호를 담을 정수
	bool endflag = false;	//종료 확인 bool값
	char value[BUFSIZE];	//선택한 메뉴 담을 배열
	int player_move;		//클라이언트가 누른 키보드 아스키 코드값을 넣을 정수


	// 윈속 초기화
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR) err_quit("connect()");

	//서버 연결요청
	int size = PackPacket(buf, PROTOCOL::REQ);
	retval = send(sock, buf, size, 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("value send()");
	}
	
	// 서버와 데이터 통신
	while(1)
	{
		//서버에서 데이터 받아오기
		if (!PacketRecv(sock, buf))
		{
			break;
		}

		//프로토콜 분리
		PROTOCOL protocol = GetProtocol(buf);
		//빈값으로 초기화
		RESULT result=NODATA;
	

		//내부 플로토콜 확인
		switch (protocol)
		{
			//메뉴 출력일 때
		case PROTOCOL::MENU_SET:
			ZeroMemory(msg, sizeof(msg));
			UnPackPacket(buf, msg);
			printf("%s\n", msg);
			break;

			//게임 플레이 중일 때
		case PROTOCOL::GAME_RESULT:
			ZeroMemory(msg, sizeof(msg));
			UnPackPacket(buf, result, msg);
			printf("%s\n", msg);
			break;

			//게임을 클리어 했을 때
		case PROTOCOL::GAME_CLEAR:
			ZeroMemory(msg, sizeof(msg));
			move_count = 0;
			stage_num = 0;
			UnPackPacket(buf, result, msg, stage_num, move_count);

			printf("\n%d번째 스테이지를 클리어 하였습니다.\n", stage_num);
			printf("이동 횟수: %d\n", move_count);
			printf("\n%s\n", msg);
			break;
		}
		
		
		//받아온 프로토콜 구분
		switch (result)
		{
		case NEXT_GAME://다음게임 진행여부 확인
		case NODATA://시작메뉴 확인
			{
				while (1)
				{
					printf(">>");
					scanf("%s", &value);


					//재대로 입력 했을 경우
					if (strcmp("1", value) == 0 || strcmp("2", value) == 0)
					{
						break; // while문 탈출
					}
					else//아니면
					{
						printf("다시 입력해 주세요...\n");

					}
				}

				//게임 시작을 눌렀을 때
				if (strcmp("1", value) == 0 )
				{
					//게임 시작 전송
					int size = PackPacket(buf, PROTOCOL::PLAY_GAME);
					retval = send(sock, buf, size, 0);
					if (retval == SOCKET_ERROR)
					{
						err_display("value send()");
					}
				}

				//종료를 눌렀을 때
				else if (strcmp("2", value) == 0)
				{
					//클라이언트 종료 확인 전송
					int size = PackPacket(buf, PROTOCOL::PLAYER_OUT);
					retval = send(sock, buf, size, 0);
					if (retval == SOCKET_ERROR)
					{
						err_display("value send()");
					}
				}

			}			
			break;

		case SOCOBAN_PLAY://소코반 게임 플레이 중
			
			//받아온 맵 출력
			DrawScreen();

			//모든 구멍에 공을 넣었다면
			if (blank == 0)
			{
				//게임 클리어 전송
				int size = PackPacket(buf, PROTOCOL::GAME_CLEAR);
				retval = send(sock, buf, size, 0);
				if (retval == SOCKET_ERROR)
				{
					err_display("value send()");
				}
				break;
			}

			//재대로된 키만 입력할 때까지 반복
			while (1)
			{
				//입력된 키보드의 아스키 코드를 받아옴
				//_getch 사용시 엔터 없이도 바로 진행 가능
				player_move = _getch();


				//재대로 입력 했을 경우
				if (player_move == 97)// A입력 : 왼쪽으로 움직임
				{
					int size = PackPacket(buf, PROTOCOL::LEFT_MOVE);
					retval = send(sock, buf, size, 0);
					if (retval == SOCKET_ERROR)
					{
						err_display("value send()");
					}
					break;
				}
				else if (player_move == 119)// W입력 : 위로 움직임
				{
					int size = PackPacket(buf, PROTOCOL::UP_MOVE);
					retval = send(sock, buf, size, 0);
					if (retval == SOCKET_ERROR)
					{
						err_display("value send()");
					}
					break;
				}
				else if (player_move == 115)// S입력 : 아래로 움직임
				{
					int size = PackPacket(buf, PROTOCOL::DOWN_MOVE);
					retval = send(sock, buf, size, 0);
					if (retval == SOCKET_ERROR)
					{
						err_display("value send()");
					}
					break;
				}
				else if (player_move == 100)// D입력 : 오른쪽으로 움직임
				{
					int size = PackPacket(buf, PROTOCOL::RIGHT_MOVE);
					retval = send(sock, buf, size, 0);
					if (retval == SOCKET_ERROR)
					{
						err_display("value send()");
					}
					break;
				}
				else if (player_move == 114)// R입력 : 맵 초기화 버튼
				{
					int size = PackPacket(buf, PROTOCOL::INITIALIZATION);
					retval = send(sock, buf, size, 0);
					if (retval == SOCKET_ERROR)
					{
						err_display("value send()");
					}
					break;
				}
				else if (player_move == 112)// P입력 : 종료버튼
				{
					int size = PackPacket(buf, PROTOCOL::PLAYER_OUT);
					retval = send(sock, buf, size, 0);
					if (retval == SOCKET_ERROR)
					{
						err_display("value send()");
					}
					break;
				}
			}
			break;


		case GAME_OUT://서버에서 클라이언트를 종료하는걸 확인 받았을 때
			//클라이언트 종료
			endflag = true;
			break;
		}		


		//클라이언트 종료일때
		if (endflag)
		{
			break;//main while문 나가기
		}

		
	}//end while

	// closesocket()
	closesocket(sock);

	// 윈속 종료
	WSACleanup();
	system("pause");
	return 0;
}



//맵 그리기
void DrawScreen()
{
	//구멍 갯수 체크 초기화
	blank = 0;
	printf("\n");

	for (int y = 0; y < 11; y++)
	{
		for (int x = 0; x < 20; x++)
		{

			switch (sand_stage[y][x])
			{
			case 0://빈칸
				printf(" ");
				break;
			case 1://벽
				printf("■");
				break;
			case 2://공
				printf("●");
				break;
			case 3://구멍
				printf("○");
				blank++;//구멍 갯수 확인
				break;
			case 4://플레이어
				printf("☆");
				break;
			case 5://채워진 구멍
				printf("◎");
				break;
			case 9://구멍위에 들어간 플레이어
				printf("★");
				blank++;//구멍 갯수 확인
				break;
			}

		}
		printf("\n");
	}

	printf("\n이  동: [A:←,W:↑,S:↓,D:→]\n\n초기화: R\n종  료: P\n");
}


// 소켓 함수 오류 출력 후 종료
void err_quit(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}


// 소켓 함수 오류 출력
void err_display(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}


// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}


// recvn
bool PacketRecv(SOCKET _sock, char* _buf)
{
	int size;

	int retval = recvn(_sock, (char*)&size, sizeof(size), 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("recv error()");
		return false;
	}
	else if (retval == 0)
	{
		return false;
	}

	retval = recvn(_sock, _buf, size, 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("recv error()");
		return false;

	}
	else if (retval == 0)
	{
		return false;
	}

	return true;
}


// 프로토콜 분리
PROTOCOL GetProtocol(char* _ptr)
{
	PROTOCOL protocol;
	memcpy(&protocol, _ptr, sizeof(PROTOCOL));

	return protocol;
}


int PackPacket(char* _buf, PROTOCOL _protocol)
{
	int size = 0;

	char* ptr = _buf;

	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);

	ptr = _buf;
	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);

	return size;
}


int PackPacket(char* _buf, PROTOCOL _protocol, int _data)
{
	int size = 0;

	char* ptr = _buf;

	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);

	memcpy(ptr, &_data, sizeof(_data));
	ptr = ptr + sizeof(_data);
	size = size + sizeof(_data);

	ptr = _buf;
	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);

	return size;
}


void UnPackPacket(char* _buf, RESULT& _result, char* _str1)
{
	int strsize1;

	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&_result, ptr, sizeof(_result));
	ptr = ptr + sizeof(_result);

	memcpy(&strsize1, ptr, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);

	memcpy(_str1, ptr, strsize1);
	ptr = ptr + strsize1;


	// 서버에서 맵 받아오기
	for (int y = 0; y < MAP_Y; y++)
	{
		for (int x = 0; x < MAP_X; x++)
		{
			memcpy(&sand_stage[y][x], ptr, sizeof(int));
			ptr = ptr + sizeof(int);

		}
	}

	system("cls"); // 콘솔 화면 초기화
}


void UnPackPacket(char* _buf, RESULT& _result, char* _str1, int& _stage, int& _move)
{
	int strsize1;

	system("cls"); // 콘솔 화면 초기화

	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&_result, ptr, sizeof(_result));
	ptr = ptr + sizeof(_result);

	memcpy(&strsize1, ptr, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);

	memcpy(_str1, ptr, strsize1);
	ptr = ptr + strsize1;


	memcpy(&_stage, ptr, sizeof(_stage));
	ptr = ptr + sizeof(int);

	memcpy(&_move, ptr, sizeof(_move));
	ptr = ptr + sizeof(_move);
}


void UnPackPacket(char* _buf, char* _str1)
{
	int strsize1;

	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&strsize1, ptr, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);

	memcpy(_str1, ptr, strsize1);
	ptr = ptr + strsize1;
}



