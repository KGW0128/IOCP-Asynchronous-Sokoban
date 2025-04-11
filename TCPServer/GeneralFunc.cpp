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
	ptr->game_result = NODATA;	//���� �÷��� ���� ���� �ʱ�ȭ

	ptr->part = nullptr;
	ptr->state = STATE::INIT_STATE;

	//recv, sand�� ����
	ptr->r_overlapped.ptr = ptr;
	ptr->r_overlapped.type = IO_TYPE::IO_RECV;
	ptr->s_overlapped.ptr = ptr;
	ptr->s_overlapped.type = IO_TYPE::IO_SEND;


	ptr->stage_num = 0;			//�������� ��ȣ
	ptr->move_count = 0;		//���� ���۽� ������ Ƚ��
	ptr->p_x = -1;				//�÷��̾� x��ǥ �ʱ�ȭ
	ptr->p_y = -1;				//�÷��̾� y��ǥ	�ʱ�ȭ
	ptr->standing_position = 0;	//�÷��̾� ���ִ� ��ġ ����

	ClientInfo[Count] = ptr;	//Ŭ���̾�Ʈ ���� ����
	Count++;					//���� �÷��̾�� ����

	LeaveCriticalSection(&cs);

	printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
		inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

	return ptr;
}

void RemoveClientInfo(_ClientInfo* _ptr)
{
	printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
		inet_ntoa(_ptr->addr.sin_addr), ntohs(_ptr->addr.sin_port));

	EnterCriticalSection(&cs);

	//�÷��̾� ���� ���� �� �ڸ� �����
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

	Count--;//���� ������ ���� �ο��� ����

	LeaveCriticalSection(&cs);
}



//���� �������� ����
void CompleteRecvProcess(_ClientInfo* _ptr)
{
	PROTOCOL protocol;//�޾ƿ� �������� ���� ����

	//�������� �и�
	protocol = GetProtocol(_ptr->recvbuf);

	//state ����
	switch (_ptr->state)
	{
	case STATE::INIT_STATE://���� ù ����
		switch (protocol)
		{
		case PROTOCOL::REQ:
			//��Ʈ�� ������
			IntroProcess(_ptr);
			break;
		}
		break;

	case STATE::MENU_STATE://�޴� Ȯ��
		
		switch (protocol)
		{
		case PROTOCOL::PLAY_GAME://���ӽ���
			//���� �� ����
			GameStartProcess(_ptr);
			break;

		case PROTOCOL::PLAYER_OUT://����
			//Ŭ���̾�Ʈ ���� Ȯ�� ����
			PlayerOutProcess(_ptr);
			break;

		}
		break;

	case STATE::GAME_STATE://���� ����
		switch (protocol)
		{
		case PROTOCOL::LEFT_MOVE://�������� ������
			LeftMoveProcess(_ptr);
			_ptr->move_count++;	//������ Ƚ�� ����
			break;

		case PROTOCOL::RIGHT_MOVE://���������� ������
			RightMoveProcess(_ptr);
			_ptr->move_count++;	//������ Ƚ�� ����
			break;

		case PROTOCOL::UP_MOVE://���� ������
			UpMoveProcess(_ptr);
			_ptr->move_count++;	//������ Ƚ�� ����
			break;

		case PROTOCOL::DOWN_MOVE://�Ʒ��� ������
			DownMoveProcess(_ptr);
			_ptr->move_count++;	//������ Ƚ�� ����
			break;

		case PROTOCOL::INITIALIZATION://�ʱ�ȭ ��ư
			InitializationProcess(_ptr);
			_ptr->move_count = 0;	//������ Ƚ�� �ʱ�ȭ
			break;	


		case PROTOCOL::GAME_CLEAR://���� Ŭ����
			_ptr->stage_num++;		//�������� ��ȣ ����
			GameClearMenuProcess(_ptr);
			_ptr->move_count = 0;	//������ Ƚ�� �ʱ�ȭ
			break;


		case PROTOCOL::PLAYER_OUT://���� ���� ����
			PlayerOutProcess(_ptr);
			break;
		}		

		break;
	}

}


//���� �������� ����
void CompleteSendProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);

	//state Ȯ��
	switch (_ptr->state)
	{
	case INIT_STATE://ù������ ��
		_ptr->state = STATE::MENU_STATE;//�޴� state�̵�
		break;
	case MENU_STATE://�޴��� ��
		_ptr->state = STATE::GAME_STATE;//���ӽ��� state�̵�
		break;
	case GAME_STATE://�������� ��

		//���� ���� ���� Ȯ���� ��
		if (_ptr->game_result == RESULT::NEXT_GAME)
		{
			_ptr->game_result = RESULT::NODATA;//�ʱ�ȭ
			_ptr->state = STATE::MENU_STATE;//�޴� state�̵�
		}
		break;
	}

	LeaveCriticalSection(&cs);
}


//���� ����
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


//Ŭ���̾�Ʈ ����
void PlayerOutProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1 = 0;
	int retval;

	//���� Ȯ�� ����
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



//�������� Ŭ���� �޴�
void GameClearMenuProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1 = 0;
	int retval;

	//�÷��̾� ��ġ �ʱ�ȭ
	_ptr->p_x = -1;
	_ptr->p_y = -1;


	//��� ���������� Ŭ���� ���� ��
	if (_ptr->stage_num >= STAGE_COUNT)
	{
		//����� ���� Ȯ�� ����
		size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_CLEAR, RESULT::GAME_OUT, ALL_CLEAR_MSG, _ptr->stage_num, _ptr->move_count);
		_ptr->game_result = RESULT::GAME_OUT;
	}
	else//���� ���������� ���Ҵٸ�
	{
		//����� �����޴� ����
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


//�÷��̾� �� �ʱ�ȭ
void InitializationProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1 = 0;
	int retval;

	//�÷��̾� ��ġ �ʱ�ȭ
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


//�÷��̾� �Ʒ������� ������ ��
void DownMoveProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1 = 0;
	int retval;

	//���� �ƴ� ��
	if (_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] != 1)
	{

		//��, ����, ä���� ������ �ƴ� �� == �տ� ��ĭ�� ��
		if (_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] != 2 &&
			_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] != 3 &&
			_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] != 5)
		{
			//�÷��̾� ����
			_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] = 4;


			//���� ���� ���ִ� ���� �����̿��ٸ�
			if (_ptr->standing_position == 3)
			{
				//�÷��̾� �ڷ� ���� ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//���ִ� ��ġ ��ĭ���� ����
				_ptr->standing_position = 0;
			}
			else//������ �ƴ϶�� == ��ĭ�̶��
			{
				//�÷��̾� �ڷ� ��ĭ ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//�÷��̾� �Ʒ������� �̵� ����
			_ptr->p_y += 1;
		}


		//�÷��̾� �տ� ������ ���� ��
		else if (_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] == 3)
		{
			//�÷��̾� ���� ������ ��
			_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] = 9;

			//���� ���� ���ִ� ���� �����̿��ٸ�
			if (_ptr->standing_position == 3)
			{
				//�÷��̾� �ڷ� ���� ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//���ִ� ��ġ �������� �缳��
				_ptr->standing_position = 3;
			}
			else//��ĭ�̿��ٸ�
			{
				//�÷��̾� �ڷ� ��ĭ ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//�÷��̾� �Ʒ������� �̵� ����
			_ptr->p_y += 1;
			//���ִ� ��ġ �������� ����
			_ptr->standing_position = 3;

		}

		//�÷��̾� ���� ���� ��
		else if (_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] == 2)
		{

			//�� �ڷ� ��ĭ�� ��
			if (_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] != 1 &&
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] != 2 &&
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] != 3 &&
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] != 5)
			{
				//���� �÷��̾� ������ �̵�
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] = 2;
				_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] = 4;

				//���� ���� ���ִ� ���� �����̿��ٸ�
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���� ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//���ִ� ��ġ ��ĭ���� ����
					_ptr->standing_position = 0;
				}
				else//��ĭ�̶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}
				//�÷��̾� �Ʒ������� �̵� ����
				_ptr->p_y += 1;
			}

			//���ڷ� ������ ��
			else if (_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] == 3)
			{
				//�÷��̾� ������ �̵� �� ���� �������� ������ ����
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] = 5;
				_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] = 4;

				//���� ���� ���ִ� ���� �����̿��ٸ�
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���� ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//���ִ� ��ġ ��ĭ���� ����
					_ptr->standing_position = 0;
				}
				else//��ĭ�̶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}

				//�÷��̾� �������� �̵� ����
				_ptr->p_y += 1;
			}

		}

		//�÷��̾� ���� ä���� ������ ��
		else if (_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] == 5)
		{
			//ä���� ���� �ڷ� �Ǵٸ� ������ ���� ��
			if (_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] == 3)
			{
				//�ձ��ۿ� ä��� �÷��̾� ���� ������ �� �� �ڷ� ��ĭ ����
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] = 5;
				_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] = 9;

				//���ִ� ���� �����̶��
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���ۻ���
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//�ƴ϶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//���ִ� ��ġ �������� ����
					_ptr->standing_position = 3;
				}

				//�÷��̾� �Ʒ������� �̵� ����
				_ptr->p_y += 1;
			}

			//ä���� ���� �ڷ� ��ĭ�� ��
			else if (_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] == 0)
			{
				//���ۿ��� ���� ������ �÷��̾� ���� ������ �� �� �ڷα��ۻ���
				_ptr->sand_stage[_ptr->p_y + 2][_ptr->p_x] = 2;
				_ptr->sand_stage[_ptr->p_y + 1][_ptr->p_x] = 9;
				
				//���ִ� ���� �����̶��
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���ۻ���
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//�ƴ϶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//���ִ� ��ġ �������� ����
					_ptr->standing_position = 3;
				}

				//�÷��̾� �Ʒ������� �̵� ����
				_ptr->p_y += 1;
				//���ִ� ��ġ �������� ����
				_ptr->standing_position = 3;
			}
		}
	}



	//�̵��� ���� ����
	size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_RESULT, RESULT::SOCOBAN_PLAY, DOWN_MOVE_MSG, _ptr);
	_ptr->game_result = RESULT::SOCOBAN_PLAY;

	//������
	if (!Send(_ptr, size1))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}


	LeaveCriticalSection(&cs);
}


//�÷��̾� �������� ������ ��
void UpMoveProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1 = 0;
	int retval;

	//���� �ƴ� ��
	if (_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] != 1)
	{

		//��, ����, ä���� ������ �ƴ� �� == �տ� ��ĭ�� ��
		if (_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] != 2 &&
			_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] != 3 &&
			_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] != 5)
		{
			//�÷��̾� ����
			_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] = 4;


			//���� ���� ���ִ� ���� �����̿��ٸ�
			if (_ptr->standing_position == 3)
			{
				//�÷��̾� �ڷ� ���� ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//���ִ� ��ġ ��ĭ���� ����
				_ptr->standing_position = 0;
			}
			else//������ �ƴ϶�� == ��ĭ�̶��
			{
				//�÷��̾� �ڷ� ��ĭ ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//�÷��̾� �������� �̵� ����
			_ptr->p_y -= 1;
		}


		//�÷��̾� �տ� ������ ���� ��
		else if (_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] == 3)
		{
			//�÷��̾� ���� ������ ��
			_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] = 9;

			//���� ���� ���ִ� ���� �����̿��ٸ�
			if (_ptr->standing_position == 3)
			{
				//�÷��̾� �ڷ� ���� ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//���ִ� ��ġ �������� �缳��
				_ptr->standing_position = 3;
			}
			else//��ĭ�̿��ٸ�
			{
				//�÷��̾� �ڷ� ��ĭ ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//�÷��̾� �������� �̵� ����
			_ptr->p_y -= 1;
			//���ִ� ��ġ �������� ����
			_ptr->standing_position = 3;

		}

		//�÷��̾� ���� ���� ��
		else if (_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] == 2)
		{

			//�� �ڷ� ��ĭ�� ��
			if (_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] != 1 &&
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] != 2 &&
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] != 3 &&
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] != 5)
			{
				//���� �÷��̾� ������ �̵�
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] = 2;
				_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] = 4;

				//���� ���� ���ִ� ���� �����̿��ٸ�
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���� ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//���ִ� ��ġ ��ĭ���� ����
					_ptr->standing_position = 0;
				}
				else//��ĭ�̶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}
				//�÷��̾� �������� �̵� ����
				_ptr->p_y -= 1;
			}

			//���ڷ� ������ ��
			else if (_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] == 3)
			{
				//�÷��̾� ������ �̵� �� ���� �������� ������ ����
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] = 5;
				_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] = 4;

				//���� ���� ���ִ� ���� �����̿��ٸ�
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���� ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//���ִ� ��ġ ��ĭ���� ����
					_ptr->standing_position = 0;
				}
				else//��ĭ�̶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}

				//�÷��̾� �������� �̵� ����
				_ptr->p_y -= 1;
			}

		}

		//�÷��̾� ���� ä���� ������ ��
		else if (_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] == 5)
		{
			//ä���� ���� �ڷ� �Ǵٸ� ������ ���� ��
			if (_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] == 3)
			{
				//�ձ��ۿ� ä��� �÷��̾� ���� ������ �� �� �ڷ� ��ĭ ����
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] = 5;
				_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] = 9;

				//���ִ� ���� �����̶��
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���ۻ���
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//�ƴ϶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//���ִ� ��ġ �������� ����
					_ptr->standing_position = 3;
				}

				//�÷��̾� �������� �̵� ����
				_ptr->p_y -= 1;
			}

			//ä���� ���� �ڷ� ��ĭ�� ��
			else if (_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] == 0)
			{
				//���ۿ��� ���� ������ �÷��̾� ���� ������ �� �� �ڷ� ���ۻ���
				_ptr->sand_stage[_ptr->p_y - 2][_ptr->p_x] = 2;
				_ptr->sand_stage[_ptr->p_y - 1][_ptr->p_x] = 9;
				
				//���ִ� ���� �����̶��
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���ۻ���
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//�ƴ϶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//���ִ� ��ġ �������� ����
					_ptr->standing_position = 3;
				}

				//�÷��̾� �������� �̵� ����
				_ptr->p_y -= 1;
				//���ִ� ��ġ �������� ����
				_ptr->standing_position = 3;
			}
		}
	}



	//�̵��� ���� ����
	size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_RESULT, RESULT::SOCOBAN_PLAY, UP_MOVE_MSG, _ptr);
	_ptr->game_result = RESULT::SOCOBAN_PLAY;

	//������
	if (!Send(_ptr, size1))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}


	LeaveCriticalSection(&cs);
}


//�÷��̾� ���������� ������ ��
void RightMoveProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1 = 0;
	int retval;

	//���� �ƴ� ��
	if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] != 1)
	{

		//��, ����, ä���� ������ �ƴ� �� == �տ� ��ĭ�� ��
		if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] != 2 && 
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] != 3 && 
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] != 5)
		{
			//�÷��̾� ����
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] = 4;


			//���� ���� ���ִ� ���� �����̿��ٸ�
			if (_ptr->standing_position == 3)
			{
				//�÷��̾� �ڷ� ���� ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//���ִ� ��ġ ��ĭ���� ����
				_ptr->standing_position = 0;
			}
			else//������ �ƴ϶�� == ��ĭ�̶��
			{
				//�÷��̾� �ڷ� ��ĭ ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//�÷��̾� ���������� �̵� ����
			_ptr->p_x += 1;
		}


		//�÷��̾� �տ� ������ ���� ��
		else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] == 3)
		{
			//�÷��̾� ���� ������ ��
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] = 9;

			//���� ���� ���ִ� ���� �����̿��ٸ�
			if (_ptr->standing_position == 3)
			{
				//�÷��̾� �ڷ� ���� ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//���ִ� ��ġ �������� �缳��
				_ptr->standing_position = 3;
			}
			else//��ĭ�̿��ٸ�
			{
				//�÷��̾� �ڷ� ��ĭ ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//�÷��̾� ���������� �̵� ����
			_ptr->p_x += 1;
			//���ִ� ��ġ �������� ����
			_ptr->standing_position = 3;

		}

		//�÷��̾� ���� ���� ��
		else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] == 2)
		{

			//�� �ڷ� ��ĭ�� ��
			if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] != 1 && 
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] != 2 && 
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] != 3 && 
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] != 5)
			{
				//���� �÷��̾� ������ �̵�
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] = 2;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] = 4;

				//���� ���� ���ִ� ���� �����̿��ٸ�
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���� ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//���ִ� ��ġ ��ĭ���� ����
					_ptr->standing_position = 0;
				}
				else//��ĭ�̶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}
				//�÷��̾� ���������� �̵� ����
				_ptr->p_x += 1;
			}

			//���ڷ� ������ ��
			else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] == 3)
			{
				//�÷��̾� ������ �̵� �� ���� �������� ������ ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] = 5;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] = 4;

				//���� ���� ���ִ� ���� �����̿��ٸ�
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���� ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//���ִ� ��ġ ��ĭ���� ����
					_ptr->standing_position = 0;
				}
				else//��ĭ�̶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}

				//�÷��̾� ���������� �̵� ����
				_ptr->p_x += 1;
			}

		}

		//�÷��̾� ���� ä���� ������ ��
		else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] == 5)
		{
			//ä���� ���� �ڷ� �Ǵٸ� ������ ���� ��
			if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] == 3)
			{
				//�ձ��ۿ� ä��� �÷��̾� ���� ������ �� �� �ڷ� ��ĭ ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] = 5;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] = 9;

				//���ִ� ���� �����̶��
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���ۻ���
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//�ƴ϶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//���ִ� ��ġ �������� ����
					_ptr->standing_position = 3;
				}

				//�÷��̾� ���������� �̵� ����
				_ptr->p_x += 1;
			}

			//ä���� ���� �ڷ� ��ĭ�� ��
			else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] == 0)
			{
				//���ۿ��� ���� ������ �÷��̾� ���� ������ �� �� �ڷα��ۻ���
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 2] = 2;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x + 1] = 9;


				//���ִ� ���� �����̶��
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���ۻ���
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//�ƴ϶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//���ִ� ��ġ �������� ����
					_ptr->standing_position = 3;
				}
				

				//�÷��̾� ���������� �̵� ����
				_ptr->p_x += 1;
				//���ִ� ��ġ �������� ����
				_ptr->standing_position = 3;
			}
		}
	}



	//�̵��� ���� ����
	size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_RESULT, RESULT::SOCOBAN_PLAY, RIGHT_MOVE_MSG, _ptr);
	_ptr->game_result = RESULT::SOCOBAN_PLAY;

	//������
	if (!Send(_ptr, size1))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}


	LeaveCriticalSection(&cs);
}


//�÷��̾� �������� ������ ��
void LeftMoveProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	int size1=0;
	int retval;

	//���� �ƴ� ��
	if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] != 1)
	{

		//��, ����, ä���� ������ �ƴ� �� == �տ� ��ĭ�� ��
		if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] != 2 && 
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] != 3 && 
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] != 5)
		{
			//�÷��̾� ����
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] = 4;


			//���� ���� ���ִ� ���� �����̿��ٸ�
			if (_ptr->standing_position == 3)
			{
				//�÷��̾� �ڷ� ���� ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//���ִ� ��ġ ��ĭ���� ����
				_ptr->standing_position = 0;
			}
			else//������ �ƴ϶�� == ��ĭ�̶��
			{
				//�÷��̾� �ڷ� ��ĭ ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//�÷��̾� �������� �̵� ����
			_ptr->p_x -= 1;
		}


		//�÷��̾� �տ� ������ ���� ��
		else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] == 3)
		{
			//�÷��̾� ���� ������ ��
			_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] = 9;

			//���� ���� ���ִ� ���� �����̿��ٸ�
			if (_ptr->standing_position == 3)
			{
				//�÷��̾� �ڷ� ���� ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				//���ִ� ��ġ �������� �缳��
				_ptr->standing_position = 3;
			}
			else//��ĭ�̿��ٸ�
			{
				//�÷��̾� �ڷ� ��ĭ ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
			}

			//�÷��̾� �������� �̵� ����
			_ptr->p_x -= 1;
			//���ִ� ��ġ �������� ����
			_ptr->standing_position = 3;

		}

		//�÷��̾� ���� ���� ��
		else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] == 2)
		{

			//�� �ڷ� ��ĭ�� ��
			if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] != 1 && 
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] != 2 && 
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] != 3 && 
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] != 5)
			{
				//���� �÷��̾� ������ �̵�
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] = 2;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] = 4;

				//���� ���� ���ִ� ���� �����̿��ٸ�
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���� ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//���ִ� ��ġ ��ĭ���� ����
					_ptr->standing_position = 0;
				}
				else//��ĭ�̶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}
				//�÷��̾� �������� �̵� ����
				_ptr->p_x -= 1;
			}

			//���ڷ� ������ ��
			else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] == 3)
			{
				//�÷��̾� ������ �̵� �� ���� �������� ������ ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] = 5;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] = 4;
				
				//���� ���� ���ִ� ���� �����̿��ٸ�
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���� ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
					//���ִ� ��ġ ��ĭ���� ����
					_ptr->standing_position = 0;
				}
				else//��ĭ�̶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
				}

				//�÷��̾� �������� �̵� ����
				_ptr->p_x -= 1;
			}

		}

		//�÷��̾� ���� ä���� ������ ��
		else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] == 5)
		{
			//ä���� ���� �ڷ� �Ǵٸ� ������ ���� ��
			if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] == 3)
			{
				//�ձ��ۿ� ä��� �÷��̾� ���� ������ �� �� �ڷ� ��ĭ ����
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] = 5;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] = 9;

				//���ִ� ���� �����̶��
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���ۻ���
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//�ƴ϶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//���ִ� ��ġ �������� ����
					_ptr->standing_position = 3;
				}				

				//�÷��̾� �������� �̵� ����
				_ptr->p_x -= 1;				
			}

			//ä���� ���� �ڷ� ��ĭ�� ��
			else if (_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] == 0)
			{
				//���ۿ��� ���� ������ �÷��̾� ���� ������ �� �� �ڷ� ���ۻ���
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 2] = 2;
				_ptr->sand_stage[_ptr->p_y][_ptr->p_x - 1] = 9;
				
				//���ִ� ���� �����̶��
				if (_ptr->standing_position == 3)
				{
					//�÷��̾� �ڷ� ���ۻ���
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 3;
				}
				else//�ƴ϶��
				{
					//�÷��̾� �ڷ� ��ĭ ����
					_ptr->sand_stage[_ptr->p_y][_ptr->p_x] = 0;
					//���ִ� ��ġ �������� ����
					_ptr->standing_position = 3;
				}

				//�÷��̾� �������� �̵� ����
				_ptr->p_x -= 1;
				//���ִ� ��ġ �������� ����
				_ptr->standing_position = 3;
			}
		}
	}



	//�̵��� ���� ����
	size1 = PackPacket(_ptr->sendbuf, PROTOCOL::GAME_RESULT, RESULT::SOCOBAN_PLAY, LEFT_MOVE_MSG, _ptr);
	_ptr->game_result = RESULT::SOCOBAN_PLAY;

	//������
	if (!Send(_ptr, size1))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}


	LeaveCriticalSection(&cs);
}


//��Ʈ�� ����
void IntroProcess(_ClientInfo* _ptr)
{	
	EnterCriticalSection(&cs);

	//��Ʈ�� ����
	int size = PackPacket(_ptr->sendbuf, PROTOCOL::MENU_SET, INTRO_MSG);

	if (!Send(_ptr, size))
	{
		ErrorPostQueuedCompletionStatus(_ptr->sock);
		LeaveCriticalSection(&cs);
		return;
	}

	LeaveCriticalSection(&cs);
}


//�⺻���� ����
void Accepted(SOCKET _sock)
{
	CreateIoCompletionPort ((HANDLE)_sock, hcp, _sock, 0);

	//Ŭ���̾�Ʈ ���� ����
	_ClientInfo* ptr = AddClientInfo(_sock);	

	//Ŭ���̾�Ʈ���� ����Ȯ�� ����
	if (!Recv(ptr))
	{
		ErrorPostQueuedCompletionStatus(_sock);
		return;
	}
}

//��Ʈ ����
void ErrorPostQueuedCompletionStatus(SOCKET _sock)
{
	WSAOVERLAPPED_EX* overlapped = new WSAOVERLAPPED_EX;
	memset(overlapped, 0, sizeof(WSAOVERLAPPED_EX));

	overlapped->type = IO_TYPE::IO_DISCONNECT;
	overlapped->ptr = (_ClientInfo*)_sock;

	PostQueuedCompletionStatus(hcp, IO_TYPE::IO_ERROR, _sock, (LPOVERLAPPED)overlapped);
}


//IO_ACCEPT ����
void AcceptPostQueuedCompletionStatus(SOCKET _sock)
{
	WSAOVERLAPPED_EX* overlapped = new WSAOVERLAPPED_EX;
	memset(overlapped, 0, sizeof(WSAOVERLAPPED_EX));

	overlapped->type = IO_TYPE::IO_ACCEPT;
	overlapped->ptr = (_ClientInfo*)_sock;

	PostQueuedCompletionStatus(hcp, IO_TYPE::IO_ACCEPT, _sock, (LPOVERLAPPED)overlapped);
}