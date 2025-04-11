#include "Global.h"


//메인 쓰레드
DWORD WINAPI WorkerThread(LPVOID arg)
{
	int retval;
	HANDLE hcp = (HANDLE)arg;

	while (1)
	{
		// 비동기 입출력 완료 기다리기
		DWORD cbTransferred;
		SOCKET client_sock;
		WSAOVERLAPPED_EX* overlapped;

		retval = GetQueuedCompletionStatus(hcp, &cbTransferred,
			(LPDWORD)&client_sock, (LPOVERLAPPED*)&overlapped, INFINITE);

		_ClientInfo* ptr = overlapped->ptr;
		IO_TYPE io_type = overlapped->type;
		WSAOVERLAPPED* overlap = &overlapped->overlapped;	

		
		// 비동기 입출력 결과 확인
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
		case IO_TYPE::IO_ACCEPT:// 클라이언트 첫 입장
			//소켓정보 셋팅
			Accepted(client_sock);		
			delete overlapped;
			break;

		case IO_TYPE::IO_RECV:// 받았을 때

			//소켓 확인
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

			//recv 후 전반적인 일처리
			CompleteRecvProcess(ptr);

			//소켓 에러 일 때
			if (!Recv(ptr))
			{
				//클라이언트 정보 종료
				ErrorPostQueuedCompletionStatus(ptr->sock);				
			}

			break;


		case IO_TYPE::IO_SEND://보낼 때

			//소켓 확인
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

			//sand 후 전반적인 일처리
			CompleteSendProcess(ptr);
			break;

		case IO_TYPE::IO_ERROR://소켓 에러일 때
			//내용 삭제
			delete overlapped;

		case IO_TYPE::IO_DISCONNECT://연결 종료일 때
			//해당 클라이언트 정보 종료
			RemoveClientInfo(ptr);
			break;
		}

	}
	return 0;
}
