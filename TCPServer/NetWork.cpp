#include "Global.h"





void err_quit(const char* msg)
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
void err_display(const char* msg)
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

PROTOCOL GetProtocol(const char* _ptr)
{
	PROTOCOL protocol;
	memcpy(&protocol, _ptr, sizeof(PROTOCOL));

	return protocol;
}

int PackPacket(char* _buf, PROTOCOL _protocol, const char* _str1)
{
	int size = 0;
	char* ptr = _buf;
	int strsize1 = strlen(_str1);

	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);

	memcpy(ptr, &strsize1, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);
	size = size + sizeof(strsize1);

	memcpy(ptr, _str1, strsize1);
	ptr = ptr + strsize1;
	size = size + strsize1;

	ptr = _buf;
	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);

	return size;
}

int PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, const char* _str1)
{
	int size = 0;
	char* ptr = _buf;
	int strsize1 = strlen(_str1);

	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);

	memcpy(ptr, &_result, sizeof(_result));
	ptr = ptr + sizeof(_result);
	size = size + sizeof(_result);


	memcpy(ptr, &strsize1, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);
	size = size + sizeof(strsize1);

	memcpy(ptr, _str1, strsize1);
	ptr = ptr + strsize1;
	size = size + strsize1;

	ptr = _buf;
	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);

	return size;
}

int PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, const char* _str1, _ClientInfo* _ptr)
{
	int size = 0;
	char* ptr = _buf;
	int strsize1 = strlen(_str1);

	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);

	memcpy(ptr, &_result, sizeof(_result));
	ptr = ptr + sizeof(_result);
	size = size + sizeof(_result);


	memcpy(ptr, &strsize1, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);
	size = size + sizeof(strsize1);

	memcpy(ptr, _str1, strsize1);
	ptr = ptr + strsize1;
	size = size + strsize1;

	if (_result != NODATA)
	{
		//처음 맵을 보낼때
		if (_ptr->p_x == -1 && _ptr->p_y == -1)
		{
			for (int y = 0; y < MAP_Y; y++)
			{
				for (int x = 0; x < MAP_X; x++)
				{
					memcpy(ptr, &arStage[_ptr->stage_num][y][x], sizeof(int));
					ptr = ptr + sizeof(int);
					size = size + sizeof(int);

					//맵 저장
					_ptr->sand_stage[y][x] = arStage[_ptr->stage_num][y][x];

					if (arStage[_ptr->stage_num][y][x] == 4)
					{
						_ptr->p_x = x;
						_ptr->p_y = y;
					}

				}
			}
		}
		else//플레이어가 움직인 후의 맵을 보낼 때
		{

			for (int y = 0; y < MAP_Y; y++)
			{
				for (int x = 0; x < MAP_X; x++)
				{
					memcpy(ptr, &_ptr->sand_stage[y][x], sizeof(int));
					ptr = ptr + sizeof(int);
					size = size + sizeof(int);

				}
			}
		}

	}

	ptr = _buf;
	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);

	return size;
}



int PackPacket(char* _buf, PROTOCOL _protocol, RESULT _result, const char* _str1, int _stage,int _move)
{
	int size = 0;
	char* ptr = _buf;
	int strsize1 = strlen(_str1);



	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);

	memcpy(ptr, &_result, sizeof(_result));
	ptr = ptr + sizeof(_result);
	size = size + sizeof(_result);


	memcpy(ptr, &strsize1, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);
	size = size + sizeof(strsize1);

	memcpy(ptr, _str1, strsize1);
	ptr = ptr + strsize1;
	size = size + strsize1;


	memcpy(ptr, &_stage, sizeof(_stage));
	ptr = ptr + sizeof(_stage);
	size = size + sizeof(_stage);

	memcpy(ptr, &_move, sizeof(_move));
	ptr = ptr + sizeof(_move);
	size = size + sizeof(_move);



	ptr = _buf;
	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);

	return size;
}



void UnPackPacket(const char* _buf, int& _data)
{
	const char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&_data, ptr, sizeof(_data));
	ptr = ptr + sizeof(_data);

}


// 사용자 정의 데이터 수신 함수
bool Recv(_ClientInfo* _ptr)
{
	int retval;
	DWORD recvbytes;
	DWORD flags = 0;

	ZeroMemory(&_ptr->r_overlapped.overlapped, sizeof(_ptr->r_overlapped.overlapped));

	_ptr->r_wsabuf.buf = _ptr->recvbuf + _ptr->comp_recvbytes;

	if (_ptr->r_sizeflag)
	{
		_ptr->r_wsabuf.len = _ptr->recvbytes - _ptr->comp_recvbytes;
	}
	else
	{
		_ptr->r_wsabuf.len = sizeof(int) - _ptr->comp_recvbytes;
	}

	retval = WSARecv(_ptr->sock, &_ptr->r_wsabuf, 1, &recvbytes,
		&flags, &_ptr->r_overlapped.overlapped, nullptr);
	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			err_display("WSARecv()");
			return false;
		}
	}

	return true;
}


//recvn
int CompleteRecv(_ClientInfo* _ptr, int _completebyte)
{

	//2번째 recv
	if (!_ptr->r_sizeflag)
	{
		_ptr->comp_recvbytes += _completebyte;

		if (_ptr->comp_recvbytes == sizeof(int))
		{
			memcpy(&_ptr->recvbytes, _ptr->recvbuf, sizeof(int));
			_ptr->comp_recvbytes = 0;
			_ptr->r_sizeflag = true;
		}

		//내용 받기
		if (!Recv(_ptr))
		{
			ErrorPostQueuedCompletionStatus(_ptr->sock);
			return SOC_ERROR;
		}

		return SOC_FALSE;
	}


	//recv 용량 비교
	_ptr->comp_recvbytes += _completebyte;

	//받을 정보를 전부 받았을 때
	if (_ptr->comp_recvbytes == _ptr->recvbytes)
	{
		_ptr->comp_recvbytes = 0;
		_ptr->recvbytes = 0;
		_ptr->r_sizeflag = false;

		return SOC_TRUE;
	}
	else//아직 덜 받았다면
	{
		if (!Recv(_ptr))
		{
			ErrorPostQueuedCompletionStatus(_ptr->sock);
			return SOC_ERROR;
		}

		return SOC_FALSE;
	}
}


//사용자 정의 데이터 송신 함수
bool Send(_ClientInfo* _ptr, int _size)
{
	int retval;
	DWORD sendbytes;
	DWORD flags;

	ZeroMemory(&_ptr->s_overlapped.overlapped, sizeof(_ptr->s_overlapped.overlapped));
	if (_ptr->sendbytes == 0)
	{
		_ptr->sendbytes = _size;
	}

	_ptr->s_wsabuf.buf = _ptr->sendbuf + _ptr->comp_sendbytes;
	_ptr->s_wsabuf.len = _ptr->sendbytes - _ptr->comp_sendbytes;

	retval = WSASend(_ptr->sock, &_ptr->s_wsabuf, 1, &sendbytes,
		0, &_ptr->s_overlapped.overlapped, nullptr);
	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			err_display("WSASend()");			
			return false;
		}
	}

	return true;
}


//sand
int CompleteSend(_ClientInfo* _ptr, int _completebyte)
{
	_ptr->comp_sendbytes += _completebyte;
	if (_ptr->comp_sendbytes == _ptr->sendbytes)
	{
		_ptr->comp_sendbytes = 0;
		_ptr->sendbytes = 0;

		return SOC_TRUE;
	}
	if (!Send(_ptr, _ptr->sendbytes))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		return SOC_ERROR;
	}

	return SOC_FALSE;
}
