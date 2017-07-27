#include "stdafx.h"
#include "stdio.h"
#include "winsock2.h" 
#include "ws2tcpip.h" 
#include "mswsock.h"
#pragma comment(lib,"ws2_32.lib") 

DWORD WINAPI workThread(LPVOID lpParam)
{
	SOCKET client = (SOCKET)lpParam;
	// 循环处理请求
	while (true)
	{
		char recData[4096];
		int ret = recv(client, recData, 4096, 0);
		if (ret > 0 && recData[0] != '\0') {
			printf("%s \n", recData);
		}
	}


	sockaddr_in ServerAddress;
	WSADATA wsdata;

	if (WSAStartup(MAKEWORD(2, 2), &wsdata) != 0)
	{
		return 1;
	}
	SOCKET sock = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	//然后赋值给地址，用来从网络上的广播地址接收消息；  
	ServerAddress.sin_family = AF_INET;
	ServerAddress.sin_addr.s_addr = INADDR_BROADCAST;
	ServerAddress.sin_port = htons(9000);
	bool opt = true;
	//设置该套接字为广播类型，  
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char FAR *)&opt, sizeof(opt));
	while (true)
	{
		Sleep(1000);
		//从广播地址发送消息  
		char *smsg = "我还活着";
		int ret = sendto(sock, smsg, 256, 0, (sockaddr*)&ServerAddress, sizeof(ServerAddress));
		if (ret == SOCKET_ERROR)
		{
			printf("%d \n", WSAGetLastError());
		}
		else
		{
			printf("发送心跳.\n");
		}
	}
	printf_s("线程退出.\n");
	return 0;
}