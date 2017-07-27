/**************************************************************
Module Name:IOCPSocketSever

Module Date:(before)2017/7/20

Module Auth:(Shary)Hou Xiaoyu

Descriiption:IOCP ������,�������ͻ���ͬʱ���ʡ�

Revision History:
2017/7/12
*********************************************************************/

#include "stdafx.h"
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"
#include "mswsock.h"
#include "IOCPSocketSever.h"


#pragma comment(lib,"ws2_32.lib")

/* ����ͬʱͶ��accept����������������*/
#define MAX_POST_ACCEPT 15



//���߳�
int main()
{
	WSADATA			 wsadata;
	DWORD			 dwThreadId;//worker�̺߳�
	int				 numThread;
	HANDLE			 CompletionPort = INVALID_HANDLE_VALUE;
	SYSTEM_INFO		 systeminfo;//���ڻ�ȡϵͳCPU
	lpResetPER_IO_Context  lpPerIOData = NULL;

	/* ����WSA*/
	WSAStartup(MAKEWORD(2,2),&wsadata);

	/* ������ɶ˿�*/
	IoCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,0);

	if (IoCompletionPort == NULL)
	{
		printf_s("������ɶ˿�ʧ�ܣ�������룺%d!\n",WSAGetLastError());
		return 2;
	}

	/* �������߳�*/

	/* ����CPU�ĺ��������轨���߳���*/
	GetSystemInfo(&systeminfo);
	numThread=2*systeminfo.dwNumberOfProcessors+2;

	/* ��ʼ���߳�������*/
	HANDLE* WorkerThreads = new HANDLE[numThread];

	/*���ݼ�����������������߳�*/
	for (int i = 0; i < numThread; i++){
		WorkerThreads[i]=CreateThread(0,0,WorkerThread,NULL,0,NULL);
	}
	printf_s("����WorkerThread %d ��.\n",numThread);
	
	/*�������ĵ�ַ�����ڰ�Socket*/
	struct sockaddr_in	Local;

	/*�������ڼ�����Socket����Ϣ*/
	ListenContext = new PER_SOCKET_CONTEXT;

	/*��Ҫʹ���ص�IO��
	 *�����ʹ��WSASocket������Socket���ſ���֧���ص�IO����
	 */
	ListenContext->C_socket=WSASocket(AF_INET,SOCK_STREAM,0,NULL,0,WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == ListenContext->C_socket)
	{
		printf_s("��ʼ��Socketʧ�ܣ�������룺%d.\n",WSAGetLastError());
		return false;
	}else{
		printf_s("WSASocket() succeed!\n");
	}

	/*����ַ��Ϣ*/
	ZeroMemory(&Local,sizeof(Local));
	Local.sin_family=AF_INET;
	Local.sin_addr.S_un.S_addr=htonl(INADDR_ANY);
	Local.sin_port=htons(8888);

	/* ���׽���*/
	if (bind(ListenContext->C_socket,(struct sockaddr *)&Local,sizeof(Local)) == SOCKET_ERROR)
	{
		printf("BIND ERROR!");
		return 4;
	}

	/*��Listen Socket ������ɶ˿�*/
	if (CreateIoCompletionPort((HANDLE)ListenContext->C_socket,IoCompletionPort,(DWORD)ListenContext,0) == NULL){
		printf_s("��Listen Socket ����ɶ˿�ʧ�ܣ���������ǣ�%d/n",WSAGetLastError());
		if (ListenContext->C_socket!=INVALID_SOCKET)
		{
			closesocket(ListenContext->C_socket);
			ListenContext->C_socket=INVALID_SOCKET;
		}
		return 3;
	}else{
		printf("Listen Socket����ɶ˿ڳɹ���.\n");
	}

	/*��ʼ��ListenContext�����Socket���󶨵ĵ�ַ�˿ڽ��м���*/
	if ( listen(ListenContext->C_socket,SOMAXCONN) == SOCKET_ERROR)
	{
		printf_s("Listen()����ִ�д���.\n");
		return 5;
	}

	/* ʹ��AcceptEx��������Ϊ���������WinSock2�淶֮���΢�������ṩ����չ����
	 * ������Ҫ�����ȡһ�º�����ָ�룬��ȡAcceptEx����ָ��
	 */
		DWORD lpBytes=0;

		if (SOCKET_ERROR==WSAIoctl(
			ListenContext->C_socket,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidAcceptEx,
			sizeof(GuidAcceptEx),
			&LPAcceptEx,
			sizeof(LPAcceptEx),
			&lpBytes,
			NULL,
			NULL))
		{
			printf_s("WSAIoct1 δ�ܻ�ȡAcceptEx����ָ�롣������룺%d\n",WSAGetLastError());
			return 6;
		}

		/* ��ȡGetAcceptExSockAddrs����ָ�룬Ҳ��ͬ��*/
		if (SOCKET_ERROR==WSAIoctl(
			ListenContext->C_socket,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidGetAcceptExSockaddrs,
			sizeof(GuidGetAcceptExSockaddrs),
			&LPGetAcceptExSockaddrs,
			sizeof(LPGetAcceptExSockaddrs),
			&lpBytes,
			NULL,
			NULL))
		{
			printf_s("WSAIoct1 δ�ܻ�ȡGuidGetAcceptExSockaddrs����ָ�롣������룺%d\n",WSAGetLastError());
			return 7;
		}

		/* ΪAcceptEx ׼��������Ȼ��Ͷ��AcceptEx I/O����,//ѭ��15��*/
		for (int i = 0;i < MAX_POST_ACCEPT; i++)

		{	
			/*����µ�����ṹ��*/
			PER_IO_Context* newAcceptIoContext = ArrayIoContext.GetNewIOContext();
			if (_PostAccept(newAcceptIoContext)==false)
			{
				ArrayIoContext.RemoveContext(newAcceptIoContext);
				return false;
			}
		}
		printf_s("Ͷ��%d ��AcceptEx������� \n",MAX_POST_ACCEPT);  
		printf("������������.\n");


		/*���߳��Ѿ���ɲ�����������������̲߳�����
		 *��ʱ���߳�����������exit�˳�
		 */
		bool run=true;
		while(run){
			char st[40];
			gets_s(st);
			if (!strcmp("exit",st))
			{
				run = false;
			}
		}

		WSACleanup();
		return 0;
}


/************************************************************************/
/* Worker�߳�                                                                     */
/************************************************************************/

DWORD WINAPI WorkerThread(LPVOID lpParam)
{
	/* ѭ����������*/
	while(true){

		/* ͨ��GetQuededCompletionStatus()����ɶ˿ڻ�ȡ����*/
		bool bReturn=GetQueuedCompletionStatus(
			IoCompletionPort,
			&cbTransferred,
			(PULONG_PTR)&PlistenContext,
			&pOverlapped,
			INFINITE);

		/*ͨ��Overlapped���õ����������������ṹ��*/
		PER_IO_Context* pIoContext=CONTAINING_RECORD(pOverlapped,PER_IO_Context,m_Overlapped);

		/*�ж��Ƿ��пͻ��˶Ͽ���*/
		if (!bReturn)
		{	
			if (GetLastError() == 64)
			{
			ArrayIoContext.RemoveContext(pIoContext);
			ArraySocketContext.RemoveContext(PlistenContext);
			printf_s("�ͻ���%s:%d �Ͽ�����!\n",inet_ntoa(PlistenContext->Client.sin_addr),ntohs(PlistenContext->Client.sin_port));
		}else{
			printf_s("�ͻ����쳣�Ͽ� %d",GetLastError());
		}
			continue;
		}else{

			/*�ж��������������*/
			switch (pIoContext->OperationType)
			{
				/************************************************************************/
				/* ACCEPT_POSTED����                                                                     */
				/************************************************************************/
			case ACCEPT_POSTED:
				{

				/*��ȡ����ͻ��˵ĵ�ַ��Ϣ*/
				SOCKADDR_IN* ClientAddr=NULL;
				SOCKADDR_IN* LocalAddr=NULL ;
				int remoteLen =sizeof(SOCKADDR_IN),localLen=sizeof(SOCKADDR_IN);

				/* ���غ�Զ�̵�ַ�������Ĵ�С�����ʹ�ô���Э���SOCKADDR��ַ��16���ֽڣ�����Ϊ���ڲ���ʽд�롣*/
				LPGetAcceptExSockaddrs(pIoContext->m_buffer.buf, pIoContext->m_buffer.len - ((sizeof(SOCKADDR_IN) + 16) * 2),
					sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);
				
				printf_s("�ͻ���%s:%d����.\n",inet_ntoa(ClientAddr->sin_addr),ntohs(ClientAddr->sin_port));

				/* ��ȡ�������û������û���*/
				input_username=strtok_s(pIoContext->m_buffer.buf,"#",&input_password);

				/* +1�ǿ����˽�����*/
				strcpy_s(user,strlen(input_username)+1,input_username);

				/*�Ƿ��¼�ɹ�*/
				bool ok=false;
				if (strlen(input_username) > 0 && strlen(input_password) > 0)
				{
					//�����˺��Ƿ����
					for (int i = 0; i < sizeof(username) / sizeof(username[0]); i++) {
						int j = 0;
						for (j = 0; username[i][j] == input_username[j] && input_username[j]; j++);
						if (username[i][j] == input_username[j] && input_username[j] == 0)
						{
							//�˺Ŵ��ڲ��������Ƿ���ȷ
							int k;
							for (k = 0; password[i][k] == input_password[k] && input_password[k]; k++);
							if (password[i][k] == input_password[k] && input_password[k] == 0)
							{
								ok = true;
							}
							break;
						}
					}
				}
				if (ok){
					printf_s("�ͻ��� %s(%s:%d) ��½�ɹ���\n", user, inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port));
					strcpy_s(pIoContext->m_buffer.buf, 11, "��½�ɹ���");
				}
				else {
					printf_s("�ͻ��� %s(%s:%d) ��½ʧ�ܣ�\n", user, inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port));
					strcpy_s(pIoContext->m_buffer.buf, 11, "��½ʧ�ܣ�");
				}

				/*sever�˷������ݽ�����ͻ���
				*ͨ��Socket�ṹ��õ�һ���µ�Socket�ṹ�壬�����û����ݱ����ȥ
				*/
				PER_SOCKET_CONTEXT* newSocketContext=ArraySocketContext.GetNewSocketContext(ClientAddr,user);

				/*��Socket�ṹ�屣�浽Socket�ṹ���������»�õ�Socket�ṹ����*/
				newSocketContext->C_socket=pIoContext->C_socket;

				/*���ͻ��˵ĵ�ַ���浽Socket�ṹ���������»�õ�Socket�ṹ����*/
				memcpy(&(newSocketContext->Client),ClientAddr,sizeof(SOCKADDR_IN));

				/*���µõ���Socket�ṹ��ŵ���ɶ˿��У��н��֪ͨ*/
				HANDLE hTemp=CreateIoCompletionPort((HANDLE)newSocketContext->C_socket,IoCompletionPort,(DWORD)newSocketContext,0);
				
				if (NULL == hTemp)
				{
					printf_s("ִ��CreateIoCompletionPort���ִ��󡣴�����룺%d \n",GetLastError());
					break;
				}
				//������µõ���Socket�ṹ���һ��PostSend���������ͻ����Ƿ��½�ɹ��Ľ�����ͻ�ȥ�����Ͳ�����ɣ�֪ͨ��ɶ˿�

				PER_IO_Context* pNewSendIoContext = ArrayIoContext.GetNewIOContext();
				memcpy(&(pNewSendIoContext->m_buffer.buf),&pIoContext->m_buffer.buf,sizeof(pIoContext->m_buffer.len));
				pNewSendIoContext->C_socket=newSocketContext->C_socket;

				/*sendͶ�ݳ�ȥ*/
				_PostSend(pNewSendIoContext);

				/*�鿴�Ƿ��¼�ɹ�*/
				if (ok)  
				{
					/*Ͷ��һ��Recv()���������ؿͻ��˵�¼���*/
					PER_IO_Context* pNewRecvIoContext = ArrayIoContext.GetNewIOContext();
					pNewRecvIoContext->C_socket=newSocketContext->C_socket;
					if (!_PostRecv(pNewRecvIoContext))
					{
						ArrayIoContext.RemoveContext(pNewRecvIoContext);
					}
				}

				//����buffer���ٴ�Ͷ��PostAccept,�ø������������Accept
				pIoContext->ResetBuffer();
				_PostAccept(pIoContext);
			}
			break;

			/************************************************************************/
			/* RECV_POSTED����                                                                     */
			/************************************************************************/
			case RECV_POSTED:
				{
					/*ִ��Recv�󣬽��н������ݵĴ���������Ŀͻ��ˣ����ٽ���Recv*/
					if (cbTransferred>1)
					{
						char* senddata = new char[MSGSIZE];
						ZeroMemory(senddata,MSGSIZE);

						char* temp=new char[MSGSIZE];
						ZeroMemory(temp,MSGSIZE);

						char *sendname = new char[40];
						ZeroMemory(sendname,40);
					    if (pIoContext->m_buffer.buf[0] == ' ')
						{
							sendname=strtok_s(pIoContext->m_buffer.buf," ",&temp);
							strtok_s(sendname," ",&temp);
							if (temp != NULL)
							{
								printf_s("�ͻ��� %s(%s:%d) �� %s ����:%s\n",PlistenContext->Cusername,inet_ntoa(PlistenContext->Client.sin_addr), ntohs(PlistenContext->Client.sin_port), sendname, temp);
								sprintf_s(senddata, MSGSIZE, "%s(%s:%d)���㷢��:\n%s", PlistenContext->Cusername, inet_ntoa(PlistenContext->Client.sin_addr), ntohs(PlistenContext->Client.sin_port), temp);
							}
						}
						else{
							printf_s("�ͻ��� %s(%s:%d) ���ҷ���:%s\n",PlistenContext->Cusername,inet_ntoa(PlistenContext->Client.sin_addr), ntohs(PlistenContext->Client.sin_port), pIoContext->m_buffer.buf);
							sprintf_s(senddata, MSGSIZE, "%s(%s:%d)���ҷ���:\n%s", PlistenContext->Cusername, inet_ntoa(PlistenContext->Client.sin_addr), ntohs(PlistenContext->Client.sin_port), pIoContext->m_buffer.buf);
						}

						for (int i = 0; i < ArraySocketContext.num; i++)
						{
							PER_SOCKET_CONTEXT* cSocketContext = ArraySocketContext.getARR(i);
							if (cSocketContext->C_socket==PlistenContext->C_socket)
							{
								continue;
							}

							/* �ж��Ƿ��ǵ�����Ϣ*/
							if (strlen(sendname)>0 && !strcmp(sendname,cSocketContext->Cusername) && strlen(senddata)>0)
							{
								PER_IO_Context* pNewSendIoContext = ArrayIoContext.GetNewIOContext();
								memcpy(&(pNewSendIoContext->m_buffer.buf),&senddata,sizeof(senddata));
								pNewSendIoContext->C_socket=cSocketContext->C_socket;

								/*sendͶ�ݳ�ȥ*/
								_PostSend(pNewSendIoContext);
							}

							/*�ж��Ƿ��ǵ�����Ϣ������Ϣ�г���*/
							else if(strlen(sendname)==0 &&strlen(senddata)>0){

								/*���ͻ���SocketContext��һ��Recv�ļƻ�*/
								PER_IO_Context* pNewSendIoContext = ArrayIoContext.GetNewIOContext();
								memcpy(&(pNewSendIoContext->m_buffer.buf),&senddata,sizeof(senddata));
								pNewSendIoContext->C_socket=cSocketContext->C_socket;

								//sendͶ�ݳ�ȥ
								_PostSend(pNewSendIoContext);
							}
						}
					}
					pIoContext->ResetBuffer();
					_PostRecv(pIoContext);
				}
				break;

          /************************************************************************/
          /* SEND_POSTED����                                                                     */
          /************************************************************************/
			case SEND_POSTED:
				/*��������Ϣ�󣬽�������������Ľṹ��ɾ��*/
				ArrayIoContext.RemoveContext(pIoContext);
				break;
			default:
				printf_s("_WorkThread�е� pIoContext->m_OpType �����쳣.\n");
				break;
				}
		}
	}
	printf_s("Work�߳��˳���\n");
	return 0;
	}

	/************************************************************************/
	/* ����SEND��RECV��ACCEPT                                                                     */
	/************************************************************************/

	/*����Ͷ��Send���󣬷�������Ϣ���֪ͨ��ɶ˿�*/
	bool _PostSend(PER_IO_Context* SendIoContext)
	{
		// ��ʼ������
		DWORD dwFlags=0;
		DWORD dwBytes=0;
		SendIoContext->OperationType = SEND_POSTED;
		WSABUF *p_wbuf = &SendIoContext->m_buffer;
		OVERLAPPED *p_ol = &SendIoContext->m_Overlapped;

		SendIoContext->ResetBuffer();

		if ((WSASend(SendIoContext->C_socket, p_wbuf, 1, &dwBytes, dwFlags, p_ol,
			NULL) == SOCKET_ERROR) && (WSAGetLastError() != WSA_IO_PENDING))
		{
			ArrayIoContext.RemoveContext(SendIoContext);
			return false;
		}
		return true;
	}

	/*����Ͷ��Accept�����յ�һ�����������֪ͨ��ɶ˿�*/
	bool _PostAccept(PER_IO_Context* AcceptIoContext)
	{	
		DWORD dwFlags=0;
		DWORD dwBytes=0;
		AcceptIoContext->OperationType=ACCEPT_POSTED;
		WSABUF *wbuf = &AcceptIoContext->m_buffer;
		OVERLAPPED *wol = &AcceptIoContext->m_Overlapped;

		/*Ϊ������Ŀͻ���׼���ýӴ�Socket*/
		AcceptIoContext->C_socket=WSASocket(AF_INET,SOCK_STREAM,0,NULL,0,WSA_FLAG_OVERLAPPED);
		if (AcceptIoContext->C_socket==INVALID_SOCKET)
		{
			printf_s("��������Accept��Socketʧ�ܣ�������ʾ: %d",WSAGetLastError());
			return false;
		}

		/*Ͷ��AcceptEx()*/
		if (LPAcceptEx(ListenContext->C_socket,AcceptIoContext->C_socket,wbuf->buf,wbuf->len-((sizeof(SOCKADDR_IN)+16)*2),
			sizeof(SOCKADDR_IN)+16,sizeof(SOCKADDR_IN)+16,&dwBytes,wol)==FALSE)
		{
			if (WSAGetLastError()!=WSA_IO_PENDING)
			{
				printf_s("Ͷ�� AcceptEx ����ʧ�ܣ��������: %d", WSAGetLastError());
				return false;
			}
		}
		return true;
	}

	/*����Recv()*/
	bool _PostRecv(PER_IO_Context* RecvIoContext){

		DWORD dwFlags=0;
		DWORD dwBytes=0;
		RecvIoContext->OperationType=RECV_POSTED;
		WSABUF *wbuf=&RecvIoContext->m_buffer;
		OVERLAPPED *wol=&RecvIoContext->m_Overlapped;

		RecvIoContext->ResetBuffer();
		int nBytesRecv=WSARecv(RecvIoContext->C_socket,wbuf,1,&dwBytes,&dwFlags,wol,NULL);

		if (nBytesRecv==SOCKET_ERROR&&(WSAGetLastError()!=WSA_IO_PENDING))
		{
			if (WSAGetLastError()!=10054)
			{
				printf_s("Ͷ��һ��WSARecvʧ�ܣ�%d \n", WSAGetLastError());
			}
			return false;
		}
		return true;
	}

