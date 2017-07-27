/**************************************************************
Module Name:IOCPSocketSever

Module Date:(before)2017/7/20

Module Auth:(Shary)Hou Xiaoyu

Descriiption:IOCP 服务器,允许多个客户端同时访问。

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

/* 允许同时投递accept请求的最大请求条数*/
#define MAX_POST_ACCEPT 15



//主线程
int main()
{
	WSADATA			 wsadata;
	DWORD			 dwThreadId;//worker线程号
	int				 numThread;
	HANDLE			 CompletionPort = INVALID_HANDLE_VALUE;
	SYSTEM_INFO		 systeminfo;//用于获取系统CPU
	lpResetPER_IO_Context  lpPerIOData = NULL;

	/* 启动WSA*/
	WSAStartup(MAKEWORD(2,2),&wsadata);

	/* 创建完成端口*/
	IoCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,0);

	if (IoCompletionPort == NULL)
	{
		printf_s("建立完成端口失败！错误代码：%d!\n",WSAGetLastError());
		return 2;
	}

	/* 创建子线程*/

	/* 根据CPU的核数计算需建立线程数*/
	GetSystemInfo(&systeminfo);
	numThread=2*systeminfo.dwNumberOfProcessors+2;

	/* 初始化线程数组句柄*/
	HANDLE* WorkerThreads = new HANDLE[numThread];

	/*根据计算出来的数量建立线程*/
	for (int i = 0; i < numThread; i++){
		WorkerThreads[i]=CreateThread(0,0,WorkerThread,NULL,0,NULL);
	}
	printf_s("建立WorkerThread %d 个.\n",numThread);
	
	/*服务器的地址，用于绑定Socket*/
	struct sockaddr_in	Local;

	/*生成用于监听的Socket的信息*/
	ListenContext = new PER_SOCKET_CONTEXT;

	/*需要使用重叠IO，
	 *必须得使用WSASocket来建立Socket，才可以支持重叠IO操作
	 */
	ListenContext->C_socket=WSASocket(AF_INET,SOCK_STREAM,0,NULL,0,WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == ListenContext->C_socket)
	{
		printf_s("初始化Socket失败，错误代码：%d.\n",WSAGetLastError());
		return false;
	}else{
		printf_s("WSASocket() succeed!\n");
	}

	/*填充地址信息*/
	ZeroMemory(&Local,sizeof(Local));
	Local.sin_family=AF_INET;
	Local.sin_addr.S_un.S_addr=htonl(INADDR_ANY);
	Local.sin_port=htons(8888);

	/* 绑定套接字*/
	if (bind(ListenContext->C_socket,(struct sockaddr *)&Local,sizeof(Local)) == SOCKET_ERROR)
	{
		printf("BIND ERROR!");
		return 4;
	}

	/*将Listen Socket 绑定至完成端口*/
	if (CreateIoCompletionPort((HANDLE)ListenContext->C_socket,IoCompletionPort,(DWORD)ListenContext,0) == NULL){
		printf_s("绑定Listen Socket 至完成端口失败！错误代码是：%d/n",WSAGetLastError());
		if (ListenContext->C_socket!=INVALID_SOCKET)
		{
			closesocket(ListenContext->C_socket);
			ListenContext->C_socket=INVALID_SOCKET;
		}
		return 3;
	}else{
		printf("Listen Socket绑定完成端口成功！.\n");
	}

	/*开始对ListenContext里面的Socket所绑定的地址端口进行监听*/
	if ( listen(ListenContext->C_socket,SOMAXCONN) == SOCKET_ERROR)
	{
		printf_s("Listen()函数执行错误.\n");
		return 5;
	}

	/* 使用AcceptEx函数，因为这个是属于WinSock2规范之外的微软另外提供的扩展函数
	 * 所以需要额外获取一下函数的指针，获取AcceptEx函数指针
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
			printf_s("WSAIoct1 未能获取AcceptEx函数指针。错误代码：%d\n",WSAGetLastError());
			return 6;
		}

		/* 获取GetAcceptExSockAddrs函数指针，也是同理*/
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
			printf_s("WSAIoct1 未能获取GuidGetAcceptExSockaddrs函数指针。错误代码：%d\n",WSAGetLastError());
			return 7;
		}

		/* 为AcceptEx 准备参数，然后投递AcceptEx I/O请求,//循环15次*/
		for (int i = 0;i < MAX_POST_ACCEPT; i++)

		{	
			/*获得新的网络结构体*/
			PER_IO_Context* newAcceptIoContext = ArrayIoContext.GetNewIOContext();
			if (_PostAccept(newAcceptIoContext)==false)
			{
				ArrayIoContext.RemoveContext(newAcceptIoContext);
				return false;
			}
		}
		printf_s("投递%d 个AcceptEx请求完毕 \n",MAX_POST_ACCEPT);  
		printf("服务器已启动.\n");


		/*主线程已经完成部署工作，其余放入子线程操作，
		 *此时主线程阻塞，输入exit退出
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
/* Worker线程                                                                     */
/************************************************************************/

DWORD WINAPI WorkerThread(LPVOID lpParam)
{
	/* 循环接收数据*/
	while(true){

		/* 通过GetQuededCompletionStatus()从完成端口获取数据*/
		bool bReturn=GetQueuedCompletionStatus(
			IoCompletionPort,
			&cbTransferred,
			(PULONG_PTR)&PlistenContext,
			&pOverlapped,
			INFINITE);

		/*通过Overlapped，得到包含这个网络操作结构体*/
		PER_IO_Context* pIoContext=CONTAINING_RECORD(pOverlapped,PER_IO_Context,m_Overlapped);

		/*判断是否有客户端断开了*/
		if (!bReturn)
		{	
			if (GetLastError() == 64)
			{
			ArrayIoContext.RemoveContext(pIoContext);
			ArraySocketContext.RemoveContext(PlistenContext);
			printf_s("客户端%s:%d 断开连接!\n",inet_ntoa(PlistenContext->Client.sin_addr),ntohs(PlistenContext->Client.sin_port));
		}else{
			printf_s("客户端异常断开 %d",GetLastError());
		}
			continue;
		}else{

			/*判断网络操作的类型*/
			switch (pIoContext->OperationType)
			{
				/************************************************************************/
				/* ACCEPT_POSTED操作                                                                     */
				/************************************************************************/
			case ACCEPT_POSTED:
				{

				/*获取连入客户端的地址信息*/
				SOCKADDR_IN* ClientAddr=NULL;
				SOCKADDR_IN* LocalAddr=NULL ;
				int remoteLen =sizeof(SOCKADDR_IN),localLen=sizeof(SOCKADDR_IN);

				/* 本地和远程地址缓冲区的大小必须比使用传输协议的SOCKADDR地址大16个字节，是因为以内部格式写入。*/
				LPGetAcceptExSockaddrs(pIoContext->m_buffer.buf, pIoContext->m_buffer.len - ((sizeof(SOCKADDR_IN) + 16) * 2),
					sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);
				
				printf_s("客户端%s:%d连接.\n",inet_ntoa(ClientAddr->sin_addr),ntohs(ClientAddr->sin_port));

				/* 获取并保存用户输入用户名*/
				input_username=strtok_s(pIoContext->m_buffer.buf,"#",&input_password);

				/* +1是考虑了结束符*/
				strcpy_s(user,strlen(input_username)+1,input_username);

				/*是否登录成功*/
				bool ok=false;
				if (strlen(input_username) > 0 && strlen(input_password) > 0)
				{
					//查找账号是否存在
					for (int i = 0; i < sizeof(username) / sizeof(username[0]); i++) {
						int j = 0;
						for (j = 0; username[i][j] == input_username[j] && input_username[j]; j++);
						if (username[i][j] == input_username[j] && input_username[j] == 0)
						{
							//账号存在查找密码是否正确
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
					printf_s("客户端 %s(%s:%d) 登陆成功！\n", user, inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port));
					strcpy_s(pIoContext->m_buffer.buf, 11, "登陆成功！");
				}
				else {
					printf_s("客户端 %s(%s:%d) 登陆失败！\n", user, inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port));
					strcpy_s(pIoContext->m_buffer.buf, 11, "登陆失败！");
				}

				/*sever端返回数据结果给客户端
				*通过Socket结构体得到一个新的Socket结构体，并将用户数据保存进去
				*/
				PER_SOCKET_CONTEXT* newSocketContext=ArraySocketContext.GetNewSocketContext(ClientAddr,user);

				/*将Socket结构体保存到Socket结构体数组中新获得的Socket结构体中*/
				newSocketContext->C_socket=pIoContext->C_socket;

				/*将客户端的地址保存到Socket结构体数组中新获得的Socket结构体中*/
				memcpy(&(newSocketContext->Client),ClientAddr,sizeof(SOCKADDR_IN));

				/*将新得到的Socket结构体放到完成端口中，有结果通知*/
				HANDLE hTemp=CreateIoCompletionPort((HANDLE)newSocketContext->C_socket,IoCompletionPort,(DWORD)newSocketContext,0);
				
				if (NULL == hTemp)
				{
					printf_s("执行CreateIoCompletionPort出现错误。错误代码：%d \n",GetLastError());
					break;
				}
				//给这个新得到的Socket结构体绑定一个PostSend操作，将客户端是否登陆成功的结果发送回去，发送操作完成，通知完成端口

				PER_IO_Context* pNewSendIoContext = ArrayIoContext.GetNewIOContext();
				memcpy(&(pNewSendIoContext->m_buffer.buf),&pIoContext->m_buffer.buf,sizeof(pIoContext->m_buffer.len));
				pNewSendIoContext->C_socket=newSocketContext->C_socket;

				/*send投递出去*/
				_PostSend(pNewSendIoContext);

				/*查看是否登录成功*/
				if (ok)  
				{
					/*投递一个Recv()操作，返回客户端登录结果*/
					PER_IO_Context* pNewRecvIoContext = ArrayIoContext.GetNewIOContext();
					pNewRecvIoContext->C_socket=newSocketContext->C_socket;
					if (!_PostRecv(pNewRecvIoContext))
					{
						ArrayIoContext.RemoveContext(pNewRecvIoContext);
					}
				}

				//重置buffer，再次投递PostAccept,让该网络操作继续Accept
				pIoContext->ResetBuffer();
				_PostAccept(pIoContext);
			}
			break;

			/************************************************************************/
			/* RECV_POSTED操作                                                                     */
			/************************************************************************/
			case RECV_POSTED:
				{
					/*执行Recv后，进行接收数据的处理，发给别的客户端，并再进行Recv*/
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
								printf_s("客户端 %s(%s:%d) 向 %s 发送:%s\n",PlistenContext->Cusername,inet_ntoa(PlistenContext->Client.sin_addr), ntohs(PlistenContext->Client.sin_port), sendname, temp);
								sprintf_s(senddata, MSGSIZE, "%s(%s:%d)向你发送:\n%s", PlistenContext->Cusername, inet_ntoa(PlistenContext->Client.sin_addr), ntohs(PlistenContext->Client.sin_port), temp);
							}
						}
						else{
							printf_s("客户端 %s(%s:%d) 向大家发送:%s\n",PlistenContext->Cusername,inet_ntoa(PlistenContext->Client.sin_addr), ntohs(PlistenContext->Client.sin_port), pIoContext->m_buffer.buf);
							sprintf_s(senddata, MSGSIZE, "%s(%s:%d)向大家发送:\n%s", PlistenContext->Cusername, inet_ntoa(PlistenContext->Client.sin_addr), ntohs(PlistenContext->Client.sin_port), pIoContext->m_buffer.buf);
						}

						for (int i = 0; i < ArraySocketContext.num; i++)
						{
							PER_SOCKET_CONTEXT* cSocketContext = ArraySocketContext.getARR(i);
							if (cSocketContext->C_socket==PlistenContext->C_socket)
							{
								continue;
							}

							/* 判断是否是单对信息*/
							if (strlen(sendname)>0 && !strcmp(sendname,cSocketContext->Cusername) && strlen(senddata)>0)
							{
								PER_IO_Context* pNewSendIoContext = ArrayIoContext.GetNewIOContext();
								memcpy(&(pNewSendIoContext->m_buffer.buf),&senddata,sizeof(senddata));
								pNewSendIoContext->C_socket=cSocketContext->C_socket;

								/*send投递出去*/
								_PostSend(pNewSendIoContext);
							}

							/*判断是否不是单对消息，且消息有长度*/
							else if(strlen(sendname)==0 &&strlen(senddata)>0){

								/*给客户端SocketContext绑定一个Recv的计划*/
								PER_IO_Context* pNewSendIoContext = ArrayIoContext.GetNewIOContext();
								memcpy(&(pNewSendIoContext->m_buffer.buf),&senddata,sizeof(senddata));
								pNewSendIoContext->C_socket=cSocketContext->C_socket;

								//send投递出去
								_PostSend(pNewSendIoContext);
							}
						}
					}
					pIoContext->ResetBuffer();
					_PostRecv(pIoContext);
				}
				break;

          /************************************************************************/
          /* SEND_POSTED操作                                                                     */
          /************************************************************************/
			case SEND_POSTED:
				/*发送完消息后，将包含网络操作的结构体删除*/
				ArrayIoContext.RemoveContext(pIoContext);
				break;
			default:
				printf_s("_WorkThread中的 pIoContext->m_OpType 参数异常.\n");
				break;
				}
		}
	}
	printf_s("Work线程退出。\n");
	return 0;
	}

	/************************************************************************/
	/* 定义SEND、RECV、ACCEPT                                                                     */
	/************************************************************************/

	/*定义投递Send请求，发送完消息后会通知完成端口*/
	bool _PostSend(PER_IO_Context* SendIoContext)
	{
		// 初始化变量
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

	/*定义投递Accept请求，收到一个连接请求会通知完成端口*/
	bool _PostAccept(PER_IO_Context* AcceptIoContext)
	{	
		DWORD dwFlags=0;
		DWORD dwBytes=0;
		AcceptIoContext->OperationType=ACCEPT_POSTED;
		WSABUF *wbuf = &AcceptIoContext->m_buffer;
		OVERLAPPED *wol = &AcceptIoContext->m_Overlapped;

		/*为新连入的客户端准备好接待Socket*/
		AcceptIoContext->C_socket=WSASocket(AF_INET,SOCK_STREAM,0,NULL,0,WSA_FLAG_OVERLAPPED);
		if (AcceptIoContext->C_socket==INVALID_SOCKET)
		{
			printf_s("创建用于Accept的Socket失败！错误提示: %d",WSAGetLastError());
			return false;
		}

		/*投递AcceptEx()*/
		if (LPAcceptEx(ListenContext->C_socket,AcceptIoContext->C_socket,wbuf->buf,wbuf->len-((sizeof(SOCKADDR_IN)+16)*2),
			sizeof(SOCKADDR_IN)+16,sizeof(SOCKADDR_IN)+16,&dwBytes,wol)==FALSE)
		{
			if (WSAGetLastError()!=WSA_IO_PENDING)
			{
				printf_s("投递 AcceptEx 请求失败，错误代码: %d", WSAGetLastError());
				return false;
			}
		}
		return true;
	}

	/*定义Recv()*/
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
				printf_s("投递一个WSARecv失败！%d \n", WSAGetLastError());
			}
			return false;
		}
		return true;
	}

