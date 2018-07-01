
#include <WinSock2.h>
#include <Ws2tcpip.h>		//inet_ntop��ͷ�ļ�
#include <time.h>

#include <iomanip>

#pragma comment(lib, "WS2_32.lib")

#pragma pack (1)

enum app_type
{
	app_none_type,
	app_server_type,
	app_client_type
};

struct Optinons 
{
	int port;
	int buffer_length;
	int buffer_count;
	char ip_addr[16];
};

float now()
{
	SYSTEMTIME systemtime;
	GetSystemTime(&systemtime);
	return (float)(systemtime.wDay * 24 * 3600 + 
		systemtime.wHour * 3600 + 
		systemtime.wMinute *60 + 
		systemtime.wSecond + 
		systemtime.wMilliseconds / 1000.0);
}

bool string_compare(char *str1, char *str2)
{
	return strcmp(str1, str2) == 0;
}

app_type parse_command(int argc, char *argv[], Optinons* pOptinons)
{
	// ����ˣ�����
	if (argc == 3)
	{
		if (string_compare(argv[1], "-r"))
		{
			for (int i = 2; i < argc; ++i)
			{
				pOptinons->port = atoi(argv[i]);
			}
			return app_server_type;
		}

		return app_none_type;
	}

	// ttcp -t host
	if (argc == 8 && string_compare(argv[1], "-t"))
	{
		struct hostent *hptr;

		if ((hptr = gethostbyname(argv[2])) == NULL)
		{
			printf("gethostbyname error for host: %s\n", argv[2]);
			return app_none_type;
		}

		if (inet_ntop(hptr->h_addrtype, hptr->h_addr, pOptinons->ip_addr, sizeof(pOptinons->ip_addr)) == NULL)
		{
			return app_none_type;
		}

		pOptinons->port = atoi(argv[3]);

		if (string_compare(argv[4], "-l"))
		{
			pOptinons->buffer_length = atoi(argv[5]);
		}
		else
		{
			return app_none_type;
		}

		if (string_compare(argv[6], "-n"))
		{
			pOptinons->buffer_count = atoi(argv[7]);
		}
		else
		{
			return app_none_type;
		}

		return app_client_type;
	}

	return app_none_type;
}

struct SessionMessage
{
	int32_t number;
	int32_t length;
};

//���ݰ�
struct PayloadMessage
{
	int length;
	char data[0];//ʹ��char[0]����ʾ�����������ݣ����Կ�����const char* �� std::unique_ptr����
};

int read_n(SOCKET s, char *pbuf, int buf_length)
{
	int len = 0;
	while (len != buf_length)
	{
		int nReadLen = recv(s, pbuf + len, buf_length - len, 0);
		if (nReadLen > 0)
		{
			len += nReadLen;
		}
		else if (nReadLen == 0)
		{
			break;
		}
		else
		{
			printf("��ȡ���ݳ���\n");
			exit(1);
		}
	}

	return len;
}

int write_n(SOCKET s, char *pbuf, int buf_length)
{
	int len = 0;
	while (len != buf_length)
	{
		int nWriteLen = send(s, pbuf + len, buf_length - len, 0);
		if (nWriteLen > 0)
		{
			len += nWriteLen;
		}
		else if (nWriteLen == 0)
		{
			break;
		}
		else
		{
			printf("д������ʧ��");
			exit(1);
		}
	}

	return len;
}

SOCKET WaitClient(int port)
{
	//�����׽���
	SOCKET sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);	//������׽���
	if (INVALID_SOCKET == sServer)
	{
		printf("�����׽���ʧ��\n");
		return INVALID_SOCKET;
	}

	//���׽���
	sockaddr_in	addrServer;
	memset(&addrServer, 0, sizeof(sockaddr_in));
	addrServer.sin_family = AF_INET;
	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addrServer.sin_port = htons(port);
	if (bind(sServer, (SOCKADDR*)& addrServer, sizeof(addrServer)) == SOCKET_ERROR)
	{
		printf("���׽��ֳ���\n");
		return INVALID_SOCKET;
	}

	//�����׽���
	if (SOCKET_ERROR == listen(sServer, 1))
	{
		printf("�����׽��ֳ���\n");
		return INVALID_SOCKET;
	}

	//�ȵ������׽���
	SOCKET sAccept;
	sockaddr_in addrClient;
	memset(&addrClient, 0, sizeof(sockaddr_in));
	int addrClientLen = sizeof(addrClient);
	sAccept = accept(sServer, (SOCKADDR*)&addrClient, &addrClientLen);
	if (sAccept == INVALID_SOCKET)
	{
		printf("�����׽���ʧ��\n");
	}
	closesocket(sServer);

	return sAccept;
}

void server_logic(int port)
{
	while (1)
	{
		//�ȵ������׽���
		SOCKET sAccept = WaitClient(port);
		if (sAccept == INVALID_SOCKET)
		{
			printf("�����׽���ʧ��\n");
			return;
		}

		//��������
		SessionMessage session_info;
		float total_mb = 0;
		if (sizeof(session_info) == read_n(sAccept, (char *)&session_info, sizeof(session_info)))
		{
			printf("������յ� SessionMessage: ������=%d ÿ�����Ĵ�С=%d\n", session_info.number, session_info.length);
			total_mb = (float)(1.0 * session_info.number * session_info.length / 1024 / 1024);
			printf("%.3f MiB in total\n", total_mb);
		}
		else
		{
			printf("SessionMessage ����\n");
			return;
		}

		const int total_len = sizeof(int32_t) + session_info.length;
		char* p = new char[total_len];

		float start = now();
		for (int i = 0; i < session_info.number; ++i)
		{
			if (read_n(sAccept, p, total_len) != total_len)
			{
				goto end_point;
			}

			PayloadMessage* payload = (PayloadMessage*)p;

			if (payload->length != session_info.length)
			{
				printf("������յ��İ������ݲ���: ʵ�ʽ��ܵ������ݴ�С==%d Ӧ�ý��յ������ݴ�С=%d\n", payload->length, session_info.length);
				break;
			}

			int ack = payload->length;
			if (write_n(sAccept, (char *)&ack, sizeof(ack)) != sizeof(ack))
			{
				goto end_point;
			}
		}
		float elapsed = now() - start;
		if (elapsed)
		{
			printf("%.3f seconds\n%.3f MiB/s\n\n", elapsed, total_mb / elapsed);
		}
		else
		{
			printf("%.3f seconds\n%.3f MiB/s\n\n", elapsed, total_mb / 0.001);
		}

end_point:
		delete[] p;
		closesocket(sAccept);
	}
}

SOCKET ConnectServer(char *ip_addr, int port)
{
	//����SOCKET
	SOCKET sClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sClient == INVALID_SOCKET)
	{
		WSACleanup();
		printf("�����׽���ʧ��\n");
		return INVALID_SOCKET;
	}

	//���ӷ����
	sockaddr_in addrServer;
	addrServer.sin_family = AF_INET;
	addrServer.sin_addr.S_un.S_addr = inet_addr(ip_addr);
	addrServer.sin_port = htons(port);
	if (connect(sClient, (SOCKADDR*)&addrServer, sizeof(addrServer)) == SOCKET_ERROR)
	{
		WSACleanup();
		closesocket(sClient);
		printf("���ӷ�����ʧ��\n");
		return INVALID_SOCKET;
	}

	BOOL setFalse = FALSE;
	int ret = setsockopt(sClient, IPPROTO_TCP, TCP_NODELAY, (char *)&setFalse, sizeof(BOOL));
	if (ret != 0)
	{
		printf("�����׽��ֲ���ʧ�ܣ�������: %d\n", GetLastError());
		return INVALID_SOCKET;
	}

	return sClient;
}

void client_logic(char *ip_addr, int port, int buffer_count, int buffer_length)
{
	//����SOCKET
	SOCKET sClient = ConnectServer(ip_addr, port);
	if (sClient == INVALID_SOCKET)
	{
		printf("�ͻ��˴����׽���ʧ��\n");
		return;
	}

	int number = buffer_count;
	int length = buffer_length;
	SessionMessage sessionMessage = { 0, 0 };
	sessionMessage.number = (number);
	sessionMessage.length = (length);
	send(sClient, (char *)&sessionMessage, sizeof(sessionMessage), 0);
	if (write_n(sClient, (char *)&sessionMessage, sizeof(sessionMessage) != sizeof(sessionMessage)))
	{
		printf("дSessionMessageʧ��\n");
		closesocket(sClient);
		return;
	}
	printf("�ͻ��˷��� SessionMessage: ������=%d ÿ�����Ĵ�С=%d\n", sessionMessage.number, sessionMessage.length);

	const int total_len = sizeof(int32_t) + length;
	char *p = new char[total_len];
	PayloadMessage* payload = (PayloadMessage*)p;
	payload->length = length;

	for (int i = 0; i < length; ++i)
	{
		payload->data[i] = "0123456789ABCDEF"[i % 16];
	}

	for (int i = 0; i < number; ++i)
	{
		if (write_n(sClient, (char *)payload, total_len) != total_len)
		{
			printf("�ͻ��˷������ݳ���\n");
			break;
		}

		int ack = 0;
		if (read_n(sClient, (char *)&ack, sizeof(ack)) != sizeof(ack))
		{
			printf("�ͻ�����ȡ���ݳ���\n");
			break;
		}
		if (ack != length)
		{
			printf("�ͻ����յ���ȷ�����ݳ���: ʵ���յ������ݴ�С=%d Ӧ���յ������ݴ�С=%d\n", ack, length);
		}

	}

	delete[] payload;
	closesocket(sClient);
}

int main(int argc, char *argv[])
{
	WSADATA VersionData;
	WSAStartup(MAKEWORD(2, 2), &VersionData);

	Optinons opt;
	app_type app_t = parse_command(argc, argv, &opt);

	if (app_t == app_server_type)
	{
		printf("port: %d\naccepting...", opt.port);
		server_logic(opt.port);
	}
	else if (app_t == app_client_type)
	{
		printf("connecting %s:%d\n", opt.ip_addr, opt.port);
		printf("buffer length: %d\n", opt.buffer_length);
		printf("number of buffers: %d\n", opt.buffer_count);
		client_logic(opt.ip_addr, opt.port, opt.buffer_count, opt.buffer_length);
	}
	else if (app_t == app_none_type)
	{
		printf("ttcp usage:\n");
		printf("  ttcp -r port\n");
		printf("  ttcp -t host port -l length -n number\n");
	}
		
	WSACleanup();
	return 0;
}