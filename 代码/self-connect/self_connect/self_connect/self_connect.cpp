// self_connect.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <WinSock2.h>

#pragma comment(lib, "WS2_32.lib")

void ConnectServer(char *ip_addr, int port)
{
	//����SOCKET
	SOCKET sClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sClient == INVALID_SOCKET)
	{
		WSACleanup();
		printf("�����׽���ʧ��\n");
		return;
	}

	//���ӷ����
	sockaddr_in addrServer;
	addrServer.sin_family = AF_INET;
	addrServer.sin_addr.S_un.S_addr = inet_addr(ip_addr);
	addrServer.sin_port = htons(port);

	int test_count = 65536;
	while (test_count--)
	{
		if (connect(sClient, (SOCKADDR*)&addrServer, sizeof(addrServer)) != SOCKET_ERROR)
		{
			printf("���ӷ������ɹ�\n");
			closesocket(sClient);
			break;
		}
	}

	WSACleanup();
}

int main()
{
	WSADATA VersionData;
	WSAStartup(MAKEWORD(2, 2), &VersionData);

	ConnectServer("127.0.0.1", 55555);
    return 0;
}

