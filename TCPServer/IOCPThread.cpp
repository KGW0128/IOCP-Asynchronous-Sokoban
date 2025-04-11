#include "Global.h"


//���� ������
DWORD WINAPI WorkerThread(LPVOID arg)
{
	int retval;
	HANDLE hcp = (HANDLE)arg;

	while (1)
	{
		// �񵿱� ����� �Ϸ� ��ٸ���
		DWORD cbTransferred;
		SOCKET client_sock;
		WSAOVERLAPPED_EX* overlapped;

		retval = GetQueuedCompletionStatus(hcp, &cbTransferred,
			(LPDWORD)&client_sock, (LPOVERLAPPED*)&overlapped, INFINITE);

		_ClientInfo* ptr = overlapped->ptr;
		IO_TYPE io_type = overlapped->type;
		WSAOVERLAPPED* overlap = &overlapped->overlapped;	

		
		// �񵿱� ����� ��� Ȯ��
		if (retval == 0 || cbTransferred == 0)
		{
			if (retval == 0)
			{
				DWORD temp1, temp2;
				WSAGetOverlappedResult(client_sock, overlap,
					&temp1, FALSE, &temp2);
				err_display("WSAGetOverlappedResult()");
			}

			io_type = IO_TYPE::IO_DISCONNECT;
		}

		int result;

		switch (io_type)
		{
		case IO_TYPE::IO_ACCEPT:// Ŭ���̾�Ʈ ù ����
			//�������� ����
			Accepted(client_sock);		
			delete overlapped;
			break;

		case IO_TYPE::IO_RECV:// �޾��� ��

			//���� Ȯ��
			result = CompleteRecv(ptr, cbTransferred);
			switch (result)
			{
			case SOC_ERROR:
				continue;
			case SOC_FALSE:
				continue;
			case SOC_TRUE:
				break;
			}

			//recv �� �������� ��ó��
			CompleteRecvProcess(ptr);

			//���� ���� �� ��
			if (!Recv(ptr))
			{
				//Ŭ���̾�Ʈ ���� ����
				ErrorPostQueuedCompletionStatus(ptr->sock);				
			}

			break;


		case IO_TYPE::IO_SEND://���� ��

			//���� Ȯ��
			result = CompleteSend(ptr, cbTransferred);
			switch (result)
			{
			case SOC_ERROR:
				continue;
			case SOC_FALSE:
				continue;
			case SOC_TRUE:
				break;
			}

			//sand �� �������� ��ó��
			CompleteSendProcess(ptr);
			break;

		case IO_TYPE::IO_ERROR://���� ������ ��
			//���� ����
			delete overlapped;

		case IO_TYPE::IO_DISCONNECT://���� ������ ��
			//�ش� Ŭ���̾�Ʈ ���� ����
			RemoveClientInfo(ptr);
			break;
		}

	}
	return 0;
}
