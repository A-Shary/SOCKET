#include "stdafx.h"
#include "stdio.h"
#include "winsock2.h" 
#include "ws2tcpip.h" 
#include "mswsock.h"
#pragma comment(lib,"ws2_32.lib") 

DWORD WINAPI workThread(LPVOID lpParam)
{
	SOCKET client = (SOCKET)lpParam;
	// ѭ����������
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
	//Ȼ��ֵ����ַ�������������ϵĹ㲥��ַ������Ϣ��  
	ServerAddress.sin_family = AF_INET;
	ServerAddress.sin_addr.s_addr = INADDR_BROADCAST;
	ServerAddress.sin_port = htons(9000);
	bool opt = true;
	//���ø��׽���Ϊ�㲥���ͣ�  
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char FAR *)&opt, sizeof(opt));
	while (true)
	{
		Sleep(1000);
		//�ӹ㲥��ַ������Ϣ  
		char *smsg = "�һ�����";
		int ret = sendto(sock, smsg, 256, 0, (sockaddr*)&ServerAddress, sizeof(ServerAddress));
		if (ret == SOCKET_ERROR)
		{
			printf("%d \n", WSAGetLastError());
		}
		else
		{
			printf("��������.\n");
		}
	}
	printf_s("�߳��˳�.\n");
	return 0;
}