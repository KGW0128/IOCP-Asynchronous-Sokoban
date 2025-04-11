#include "Global.h"

_ClientInfo* AddClientInfo(SOCKET _sock)
{
	SOCKADDR_IN clientaddr;
	int addrlen = sizeof(clientaddr);
	getpeername(_sock, (SOCKADDR*)&clientaddr, &addrlen);

	EnterCriticalSection(&cs);

	_ClientInfo* ptr = new _ClientInfo;
	memset(ptr, 0, sizeof(_ClientInfo));
	ptr->sock = _sock;
	memcpy(&ptr->addr, &clientaddr, sizeof(SOCKADDR_IN));
	ptr->r_sizeflag = false;
	ptr->game_result = NODATA;	//게임 플레이 상태 구분 초기화

	ptr->part = nullptr;
	ptr->state = STATE::INIT_STATE;

	//recv, sand용 구분
	ptr->r_overlapped.ptr = ptr;
	ptr->r_overlapped.type = IO_TYPE::IO_RECV;
	ptr->s_overlapped.ptr = ptr;
	ptr->s_overlapped.type = IO_TYPE::IO_SEND;


	ptr->stage_num = 0;			//스테이지 번호
	ptr->move_count = 0;		//게임 시작시 움직인 횟수
	ptr->p_x = -1;				//플레이어 x좌표 초기화
	ptr->p_y = -1;				//플레이어 y좌표	초기화
	ptr->standing_position = 0;	//플레이어 서있는 위치 정보

	ClientInfo[Count] = ptr;	//클라이언트 정보 저장
	Count++;					//현재 플레이어수 증가

	LeaveCriticalSection(&cs);

	printf("\n[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
		inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

	return ptr;
}

void RemoveClientInfo(_ClientInfo* _ptr)
{
	printf("[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
		inet_ntoa(_ptr->addr.sin_addr), ntohs(_ptr->addr.sin_port));

	EnterCriticalSection(&cs);

	//플레이어 정보 정리 후 자리 땡기기
	for (int i = 0; i < Count; i++)
	{
		if (ClientInfo[i] == _ptr)
		{
			closesocket(ClientInfo[i]->sock);

			if (ClientInfo[i]->part != nullptr)
			{
				ClientInfo[i]->part->part = nullptr;
			}

			delete ClientInfo[i];

			for (int j = i; j < Count - 1; j++)
			{
				ClientInfo[j] = ClientInfo[j + 1];
			}
			ClientInfo[Count-1] = nullptr;
 			break;
		}
	}

	Count--;//정보 삭제후 현재 인원수 감소

	LeaveCriticalSection(&cs);
}



//받은 프로토콜 구분
void CompleteRecvProcess(_ClientInfo* _ptr)
{
	PROTOCOL protocol;//받아온 프로토콜 저장 변수

	//프로토콜 분리
	protocol = GetProtocol(_ptr->recvbuf);

	//state 구분
	switch (_ptr->state)
	{
	case STATE::INIT_STATE://서버 첫 입장
		switch (protocol)
		{
		case PROTOCOL::REQ:
			//인트로 보내기
			IntroProcess(_ptr);
			break;
		}
		break;

	case STATE::MENU_STATE://메뉴 확인
		
		switch (protocol)
		{
		case PROTOCOL::PLAY_GAME://게임시작
			//게임 맵 전송
			GameStartProcess(_ptr);
			break;

		case PROTOCOL::PLAYER_OUT://종료
			//클라이언트 종료 확인 전송
			PlayerOutProcess(_ptr);
			break;

		}
		break;

	case STATE::GAME_STATE://게임 시작
		switch (protocol)
		{
		case PROTOCOL::LEFT_MOVE://왼쪽으로 움직임
			LeftMoveProcess(_ptr);
			_ptr->move_count++;	//움직인 횟수 증가
			break;

		case PROTOCOL::RIGHT_MOVE://오른쪽으로 움직임
			RightMoveProcess(_ptr);
			_ptr->move_count++;	//움직인 횟수 증가
			break;

		case PROTOCOL::UP_MOVE://위로 움직임
			UpMoveProcess(_ptr);
			_ptr->move_count++;	//움직인 횟수 증가
			break;

		case PROTOCOL::DOWN_MOVE://아래로 움직임
			DownMoveProcess(_ptr);
			_ptr->move_count++;	//움직인 횟수 증가
			break;

		case PROTOCOL::INITIALIZATION://초기화 버튼
			InitializationProcess(_ptr);
			_ptr->move_count = 0;	//움직인 횟수 초기화
			break;	


		case PROTOCOL::GAME_CLEAR://게임 클리어
			_ptr->stage_num++;		//스테이지 번호 증가
			GameClearMenuProcess(_ptr);
			_ptr->move_count = 0;	//움직인 횟수 초기화
			break;


		case PROTOCOL::PLAYER_OUT://게임 도중 종료
			PlayerOutProcess(_ptr);
			break;
		}		

		break;
	}

}


//보낸 프로토콜 구분
void CompleteSendProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);

	//state 확인
	switch (_ptr->state)
	{
	case INIT_STATE://첫접속일 때
		_ptr->state = STATE::MENU_STATE;//메뉴 state이동
		break;
	case MENU_STATE://메뉴일 때
		_ptr->state = STATE::GAME_STATE;//게임시작 state이동
		break;
	case GAME_STATE://게임중일 때

		//다음 게임 시작 확인일 때
		if (_ptr->game_result == RESULT::NEXT_GAME)
		{
			_ptr->game_result = RESULT::NODATA;//초기화
			_ptr->state = STATE::MENU_STATE;//메뉴 state이동
		}
		break;
	}

	LeaveCriticalSection(&cs);
}


//게임 시작
void GameStartProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1 = 0;
	int retval;

	size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_RESULT, RESULT::SOCOBAN_PLAY, GAME_START_MSG, _ptr);
	_ptr->game_result = RESULT::SOCOBAN_PLAY;

	if (!Send(_ptr, size1))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}

	LeaveCriticalSection(&cs);
}


//클라이언트 종료
void PlayerOutProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1 = 0;
	int retval;

	//종료 확인 전송
	size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_RESULT, RESULT::GAME_OUT, OUT_MSG);
	_ptr->game_result = RESULT::GAME_OUT;

	
	if (!Send(_ptr, size1))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}

	
	LeaveCriticalSection(&cs);
}



//스테이지 클리어 메뉴
void GameClearMenuProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1 = 0;
	int retval;

	//플레이어 위치 초기화
	_ptr->p_x = -1;
	_ptr->p_y = -1;


	//모든 스테이지를 클리어 했을 때
	if (_ptr->stage_num >= STAGE_COUNT)
	{
		//결과와 종료 확인 전송
		size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_CLEAR, RESULT::GAME_OUT, ALL_CLEAR_MSG, _ptr->stage_num, _ptr->move_count);
		_ptr->game_result = RESULT::GAME_OUT;
	}
	else//아직 스테이지가 남았다면
	{
		//결과와 다음메뉴 전송
		size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_CLEAR, RESULT::NEXT_GAME, GAME_CLEAR_MSG, _ptr->stage_num, _ptr->move_count);
		_ptr->game_result = RESULT::NEXT_GAME;	
	}


	if (!Send(_ptr, size1))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}


	LeaveCriticalSection(&cs);
}


//플레이어 맵 초기화
void InitializationProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1 = 0;
	int retval;

	//플레이어 위치 초기화
	_ptr->p_x = -1;
	_ptr->p_y = -1;

	size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_RESULT, RESULT::SOCOBAN_PLAY, INITIALIZATION_MSG, _ptr);
	_ptr->game_result = RESULT::SOCOBAN_PLAY;


	if (!Send(_ptr, size1))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}

	LeaveCriticalSection(&cs);
}


//플레이어 아래쪽으로 움직일 때
void DownMoveProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1 = 0;
	int retval;

	//벽이 아닐 때
	if (_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] != 1)
	{

		//돌, 구멍, 채워진 구멍이 아닐 때 == 앞에 빈칸일 때
		if (_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] != 2 &&
			_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] != 3 &&
			_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] != 5)
		{
			//플레이어 전진
			_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] = 4;


			//만약 내가 서있던 곳이 구멍이였다면
			if (_ptr->standing_position == 3)
			{
				//플레이어 뒤로 구멍 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//서있는 위치 빈칸으로 설정
				_ptr->standing_position = 0;
			}
			else//구멍이 아니라면 == 빈칸이라면
			{
				//플레이어 뒤로 빈칸 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//플레이어 아래쪽으로 이동 저장
			_ptr->p_y += 1;
		}


		//플레이어 앞에 구멍이 있을 때
		else if (_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] == 3)
		{
			//플레이어 구멍 안으로 들어감
			_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] = 9;

			//만약 내가 서있던 곳이 구멍이였다면
			if (_ptr->standing_position == 3)
			{
				//플레이어 뒤로 구멍 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//서있는 위치 구멍으로 재설정
				_ptr->standing_position = 3;
			}
			else//빈칸이였다면
			{
				//플레이어 뒤로 빈칸 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//플레이어 아래쪽으로 이동 저장
			_ptr->p_y += 1;
			//서있는 위치 구멍으로 설정
			_ptr->standing_position = 3;

		}

		//플레이어 앞이 돌일 때
		else if (_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] == 2)
		{

			//돌 뒤로 빈칸일 때
			if (_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] != 1 &&
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] != 2 &&
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] != 3 &&
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] != 5)
			{
				//돌과 플레이어 앞으로 이동
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] = 2;
				_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] = 4;

				//만약 내가 서있던 곳이 구멍이였다면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//서있는 위치 빈칸으로 설정
					_ptr->standing_position = 0;
				}
				else//빈칸이라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}
				//플레이어 아래쪽으로 이동 저장
				_ptr->p_y += 1;
			}

			//돌뒤로 구멍일 때
			else if (_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] == 3)
			{
				//플레이어 앞으로 이동 후 돌이 구멍으로 들어감으로 설정
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] = 5;
				_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] = 4;

				//만약 내가 서있던 곳이 구멍이였다면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//서있는 위치 빈칸으로 설정
					_ptr->standing_position = 0;
				}
				else//빈칸이라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}

				//플레이어 위쪽으로 이동 저장
				_ptr->p_y += 1;
			}

		}

		//플레이어 앞이 채워진 구멍일 때
		else if (_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] == 5)
		{
			//채워진 구멍 뒤로 또다른 구멍이 있을 때
			if (_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] == 3)
			{
				//앞구멍에 채우고 플레이어 구멍 안으로 들어간 후 뒤로 빈칸 생성
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] = 5;
				_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] = 9;

				//서있던 곳이 구멍이라면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//아니라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//서있는 위치 구멍으로 설정
					_ptr->standing_position = 3;
				}

				//플레이어 아래쪽으로 이동 저장
				_ptr->p_y += 1;
			}

			//채워진 구멍 뒤로 빈칸일 때
			else if (_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] == 0)
			{
				//구멍에서 돌이 나오고 플레이어 구멍 안으로 들어간 후 뒤로구멍생성
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] = 2;
				_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] = 9;
				
				//서있던 곳이 구멍이라면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//아니라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//서있는 위치 구멍으로 설정
					_ptr->standing_position = 3;
				}

				//플레이어 아래쪽으로 이동 저장
				_ptr->p_y += 1;
				//서있는 위치 구멍으로 설정
				_ptr->standing_position = 3;
			}
		}
	}



	//이동한 정보 저장
	size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_RESULT, RESULT::SOCOBAN_PLAY, DOWN_MOVE_MSG, _ptr);
	_ptr->game_result = RESULT::SOCOBAN_PLAY;

	//보내기
	if (!Send(_ptr, size1))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}


	LeaveCriticalSection(&cs);
}


//플레이어 위쪽으로 움직일 때
void UpMoveProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1 = 0;
	int retval;

	//벽이 아닐 때
	if (_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] != 1)
	{

		//돌, 구멍, 채워진 구멍이 아닐 때 == 앞에 빈칸일 때
		if (_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] != 2 &&
			_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] != 3 &&
			_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] != 5)
		{
			//플레이어 전진
			_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] = 4;


			//만약 내가 서있던 곳이 구멍이였다면
			if (_ptr->standing_position == 3)
			{
				//플레이어 뒤로 구멍 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//서있는 위치 빈칸으로 설정
				_ptr->standing_position = 0;
			}
			else//구멍이 아니라면 == 빈칸이라면
			{
				//플레이어 뒤로 빈칸 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//플레이어 위쪽으로 이동 저장
			_ptr->p_y -= 1;
		}


		//플레이어 앞에 구멍이 있을 때
		else if (_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] == 3)
		{
			//플레이어 구멍 안으로 들어감
			_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] = 9;

			//만약 내가 서있던 곳이 구멍이였다면
			if (_ptr->standing_position == 3)
			{
				//플레이어 뒤로 구멍 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//서있는 위치 구멍으로 재설정
				_ptr->standing_position = 3;
			}
			else//빈칸이였다면
			{
				//플레이어 뒤로 빈칸 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//플레이어 위쪽으로 이동 저장
			_ptr->p_y -= 1;
			//서있는 위치 구멍으로 설정
			_ptr->standing_position = 3;

		}

		//플레이어 앞이 돌일 때
		else if (_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] == 2)
		{

			//돌 뒤로 빈칸일 때
			if (_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] != 1 &&
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] != 2 &&
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] != 3 &&
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] != 5)
			{
				//돌과 플레이어 앞으로 이동
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] = 2;
				_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] = 4;

				//만약 내가 서있던 곳이 구멍이였다면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//서있는 위치 빈칸으로 설정
					_ptr->standing_position = 0;
				}
				else//빈칸이라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}
				//플레이어 위쪽으로 이동 저장
				_ptr->p_y -= 1;
			}

			//돌뒤로 구멍일 때
			else if (_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] == 3)
			{
				//플레이어 앞으로 이동 후 돌이 구멍으로 들어감으로 설정
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] = 5;
				_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] = 4;

				//만약 내가 서있던 곳이 구멍이였다면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//서있는 위치 빈칸으로 설정
					_ptr->standing_position = 0;
				}
				else//빈칸이라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}

				//플레이어 위쪽으로 이동 저장
				_ptr->p_y -= 1;
			}

		}

		//플레이어 앞이 채워진 구멍일 때
		else if (_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] == 5)
		{
			//채워진 구멍 뒤로 또다른 구멍이 있을 때
			if (_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] == 3)
			{
				//앞구멍에 채우고 플레이어 구멍 안으로 들어간 후 뒤로 빈칸 생성
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] = 5;
				_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] = 9;

				//서있던 곳이 구멍이라면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//아니라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//서있는 위치 구멍으로 설정
					_ptr->standing_position = 3;
				}

				//플레이어 위쪽으로 이동 저장
				_ptr->p_y -= 1;
			}

			//채워진 구멍 뒤로 빈칸일 때
			else if (_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] == 0)
			{
				//구멍에서 돌이 나오고 플레이어 구멍 안으로 들어간 후 뒤로 구멍생성
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] = 2;
				_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] = 9;
				
				//서있던 곳이 구멍이라면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//아니라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//서있는 위치 구멍으로 설정
					_ptr->standing_position = 3;
				}

				//플레이어 위쪽으로 이동 저장
				_ptr->p_y -= 1;
				//서있는 위치 구멍으로 설정
				_ptr->standing_position = 3;
			}
		}
	}



	//이동한 정보 저장
	size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_RESULT, RESULT::SOCOBAN_PLAY, UP_MOVE_MSG, _ptr);
	_ptr->game_result = RESULT::SOCOBAN_PLAY;

	//보내기
	if (!Send(_ptr, size1))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}


	LeaveCriticalSection(&cs);
}


//플레이어 오른쪽으로 움직일 때
void RightMoveProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1 = 0;
	int retval;

	//벽이 아닐 때
	if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] != 1)
	{

		//돌, 구멍, 채워진 구멍이 아닐 때 == 앞에 빈칸일 때
		if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] != 2 && 
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] != 3 && 
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] != 5)
		{
			//플레이어 전진
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] = 4;


			//만약 내가 서있던 곳이 구멍이였다면
			if (_ptr->standing_position == 3)
			{
				//플레이어 뒤로 구멍 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//서있는 위치 빈칸으로 설정
				_ptr->standing_position = 0;
			}
			else//구멍이 아니라면 == 빈칸이라면
			{
				//플레이어 뒤로 빈칸 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//플레이어 오른쪽으로 이동 저장
			_ptr->p_x += 1;
		}


		//플레이어 앞에 구멍이 있을 때
		else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] == 3)
		{
			//플레이어 구멍 안으로 들어감
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] = 9;

			//만약 내가 서있던 곳이 구멍이였다면
			if (_ptr->standing_position == 3)
			{
				//플레이어 뒤로 구멍 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//서있는 위치 구멍으로 재설정
				_ptr->standing_position = 3;
			}
			else//빈칸이였다면
			{
				//플레이어 뒤로 빈칸 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//플레이어 오른쪽으로 이동 저장
			_ptr->p_x += 1;
			//서있는 위치 구멍으로 설정
			_ptr->standing_position = 3;

		}

		//플레이어 앞이 돌일 때
		else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] == 2)
		{

			//돌 뒤로 빈칸일 때
			if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] != 1 && 
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] != 2 && 
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] != 3 && 
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] != 5)
			{
				//돌과 플레이어 앞으로 이동
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] = 2;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] = 4;

				//만약 내가 서있던 곳이 구멍이였다면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//서있는 위치 빈칸으로 설정
					_ptr->standing_position = 0;
				}
				else//빈칸이라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}
				//플레이어 오른쪽으로 이동 저장
				_ptr->p_x += 1;
			}

			//돌뒤로 구멍일 때
			else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] == 3)
			{
				//플레이어 앞으로 이동 후 돌이 구멍으로 들어감으로 설정
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] = 5;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] = 4;

				//만약 내가 서있던 곳이 구멍이였다면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//서있는 위치 빈칸으로 설정
					_ptr->standing_position = 0;
				}
				else//빈칸이라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}

				//플레이어 오른쪽으로 이동 저장
				_ptr->p_x += 1;
			}

		}

		//플레이어 앞이 채워진 구멍일 때
		else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] == 5)
		{
			//채워진 구멍 뒤로 또다른 구멍이 있을 때
			if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] == 3)
			{
				//앞구멍에 채우고 플레이어 구멍 안으로 들어간 후 뒤로 빈칸 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] = 5;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] = 9;

				//서있던 곳이 구멍이라면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//아니라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//서있는 위치 구멍으로 설정
					_ptr->standing_position = 3;
				}

				//플레이어 오른쪽으로 이동 저장
				_ptr->p_x += 1;
			}

			//채워진 구멍 뒤로 빈칸일 때
			else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] == 0)
			{
				//구멍에서 돌이 나오고 플레이어 구멍 안으로 들어간 후 뒤로구멍생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] = 2;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] = 9;


				//서있던 곳이 구멍이라면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//아니라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//서있는 위치 구멍으로 설정
					_ptr->standing_position = 3;
				}
				

				//플레이어 오른쪽으로 이동 저장
				_ptr->p_x += 1;
				//서있는 위치 구멍으로 설정
				_ptr->standing_position = 3;
			}
		}
	}



	//이동한 정보 저장
	size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_RESULT, RESULT::SOCOBAN_PLAY, RIGHT_MOVE_MSG, _ptr);
	_ptr->game_result = RESULT::SOCOBAN_PLAY;

	//보내기
	if (!Send(_ptr, size1))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}


	LeaveCriticalSection(&cs);
}


//플레이어 왼쪽으로 움직일 때
void LeftMoveProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1=0;
	int retval;

	//벽이 아닐 때
	if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] != 1)
	{

		//돌, 구멍, 채워진 구멍이 아닐 때 == 앞에 빈칸일 때
		if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] != 2 && 
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] != 3 && 
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] != 5)
		{
			//플레이어 전진
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] = 4;


			//만약 내가 서있던 곳이 구멍이였다면
			if (_ptr->standing_position == 3)
			{
				//플레이어 뒤로 구멍 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//서있는 위치 빈칸으로 설정
				_ptr->standing_position = 0;
			}
			else//구멍이 아니라면 == 빈칸이라면
			{
				//플레이어 뒤로 빈칸 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//플레이어 왼쪽으로 이동 저장
			_ptr->p_x -= 1;
		}


		//플레이어 앞에 구멍이 있을 때
		else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] == 3)
		{
			//플레이어 구멍 안으로 들어감
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] = 9;

			//만약 내가 서있던 곳이 구멍이였다면
			if (_ptr->standing_position == 3)
			{
				//플레이어 뒤로 구멍 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//서있는 위치 구멍으로 재설정
				_ptr->standing_position = 3;
			}
			else//빈칸이였다면
			{
				//플레이어 뒤로 빈칸 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//플레이어 왼쪽으로 이동 저장
			_ptr->p_x -= 1;
			//서있는 위치 구멍으로 설정
			_ptr->standing_position = 3;

		}

		//플레이어 앞이 돌일 때
		else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] == 2)
		{

			//돌 뒤로 빈칸일 때
			if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] != 1 && 
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] != 2 && 
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] != 3 && 
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] != 5)
			{
				//돌과 플레이어 앞으로 이동
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] = 2;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] = 4;

				//만약 내가 서있던 곳이 구멍이였다면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//서있는 위치 빈칸으로 설정
					_ptr->standing_position = 0;
				}
				else//빈칸이라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}
				//플레이어 왼쪽으로 이동 저장
				_ptr->p_x -= 1;
			}

			//돌뒤로 구멍일 때
			else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] == 3)
			{
				//플레이어 앞으로 이동 후 돌이 구멍으로 들어감으로 설정
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] = 5;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] = 4;
				
				//만약 내가 서있던 곳이 구멍이였다면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//서있는 위치 빈칸으로 설정
					_ptr->standing_position = 0;
				}
				else//빈칸이라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}

				//플레이어 왼쪽으로 이동 저장
				_ptr->p_x -= 1;
			}

		}

		//플레이어 앞이 채워진 구멍일 때
		else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] == 5)
		{
			//채워진 구멍 뒤로 또다른 구멍이 있을 때
			if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] == 3)
			{
				//앞구멍에 채우고 플레이어 구멍 안으로 들어간 후 뒤로 빈칸 생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] = 5;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] = 9;

				//서있던 곳이 구멍이라면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//아니라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//서있는 위치 구멍으로 설정
					_ptr->standing_position = 3;
				}				

				//플레이어 왼쪽으로 이동 저장
				_ptr->p_x -= 1;				
			}

			//채워진 구멍 뒤로 빈칸일 때
			else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] == 0)
			{
				//구멍에서 돌이 나오고 플레이어 구멍 안으로 들어간 후 뒤로 구멍생성
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] = 2;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] = 9;
				
				//서있던 곳이 구멍이라면
				if (_ptr->standing_position == 3)
				{
					//플레이어 뒤로 구멍생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//아니라면
				{
					//플레이어 뒤로 빈칸 생성
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//서있는 위치 구멍으로 설정
					_ptr->standing_position = 3;
				}

				//플레이어 왼쪽으로 이동 저장
				_ptr->p_x -= 1;
				//서있는 위치 구멍으로 설정
				_ptr->standing_position = 3;
			}
		}
	}



	//이동한 정보 저장
	size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_RESULT, RESULT::SOCOBAN_PLAY, LEFT_MOVE_MSG, _ptr);
	_ptr->game_result = RESULT::SOCOBAN_PLAY;

	//보내기
	if (!Send(_ptr, size1))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}


	LeaveCriticalSection(&cs);
}


//인트로 전송
void IntroProcess(_ClientInfo* _ptr)
{	
	EnterCriticalSection(&cs);

	//인트로 전송
	int size = PackPacket(_ptr->sendbuf, PROTOCOL::MENU_SET, INTRO_MSG);

	if (!Send(_ptr, size))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}

	LeaveCriticalSection(&cs);
}


//기본정보 셋팅
void Accepted(SOCKET _sock)
{
	CreateIoCompletionPort ((HANDLE)_sock, hcp, _sock, 0);

	//클라이언트 정보 셋팅
	_ClientInfo* ptr = AddClientInfo(_sock);	

	//클라이언트에게 연결확인 전송
	if (!Recv(ptr))
	{
		ErrorPostQueuedCompletionStatus(_sock);
		return;
	}
}

//포트 에러
void ErrorPostQueuedCompletionStatus(SOCKET _sock)
{
	WSAOVERLAPPED_EX* overlapped = new WSAOVERLAPPED_EX;
	memset(overlapped, 0, sizeof(WSAOVERLAPPED_EX));

	overlapped->type = IO_TYPE::IO_DISCONNECT;
	overlapped->ptr = (_ClientInfo*)_sock;

	PostQueuedCompletionStatus(hcp, IO_TYPE::IO_ERROR, _sock, (LPOVERLAPPED)overlapped);
}


//IO_ACCEPT 설정
void AcceptPostQueuedCompletionStatus(SOCKET _sock)
{
	WSAOVERLAPPED_EX* overlapped = new WSAOVERLAPPED_EX;
	memset(overlapped, 0, sizeof(WSAOVERLAPPED_EX));

	overlapped->type = IO_TYPE::IO_ACCEPT;
	overlapped->ptr = (_ClientInfo*)_sock;

	PostQueuedCompletionStatus(hcp, IO_TYPE::IO_ACCEPT, _sock, (LPOVERLAPPED)overlapped);
}