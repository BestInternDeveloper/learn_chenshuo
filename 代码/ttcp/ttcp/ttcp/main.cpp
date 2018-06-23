
#include <WinSock2.h>
#include <Ws2tcpip.h>		//inet_ntop��ͷ�ļ�

#include <iostream>
#include <iomanip>

using namespace std;
#pragma comment(lib, "WS2_32.lib")

#pragma pack (1)

#define DEFAULT_IP_ADDRESS	"127.0.0.1"
#define DEFAULT_PORT		5000
#define DEFAULT_BUFFER_SIZE		65536
#define DEFAULT_BUFFER_COUNT	1024

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
	return (float)GetTickCount() / 1000.0;
}

bool string_compare(char *str1, char *str2)
{
	return strcmp(str1, str2) == 0;
}

app_type parse_command(int argc, char *argv[], Optinons* pOptinons)
{
	//memcpy(pOptinons->ip_addr, DEFAULT_IP_ADDRESS, sizeof(DEFAULT_IP_ADDRESS));
	pOptinons->port = DEFAULT_PORT;
	pOptinons->buffer_count = DEFAULT_BUFFER_COUNT;
	pOptinons->buffer_length = DEFAULT_BUFFER_SIZE;

	// ����ˣ�����
	if (argc >= 1 && argc <= 3)
	{
		if (argc >= 3 && string_compare(argv[1], "-r"))
		{
			for (int i = 2; i < argc; ++i)
			{
				pOptinons->port = atoi(argv[i]);
			}
			return app_server_type;
		}

		if (argc == 1)
		{
			return app_server_type;
		}
	}

	// ttcp -t host
	if (argc >= 3 && string_compare(argv[1], "-t"))
	{
		struct hostent *hptr;

		if ((hptr = gethostbyname(argv[2])) == NULL)
		{
			cout << "gethostbyname error for host:" << argv[2] << endl;
			return app_none_type;
		}

		if (inet_ntop(hptr->h_addrtype, hptr->h_addr, pOptinons->ip_addr, sizeof(pOptinons->ip_addr)) == NULL)
		{
			return app_none_type;
		}

		for (int i = 3; i < argc;)
		{
			if (string_compare(argv[i], "-l"))
			{
				pOptinons->buffer_length = atoi(argv[i + 1]);
			}
			else if (string_compare(argv[i], "-n"))
			{
				pOptinons->buffer_count = atoi(argv[i + 1]);
			}
			else if (i == 3)
			{
				pOptinons->port = atoi(argv[i]);
				i++;
				continue;
			}
			else
			{
				return app_none_type;
			}

			i += 2;
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

void server_logic(int port)
{
	//�����׽���
	SOCKET sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);	//������׽���
	if (INVALID_SOCKET == sServer)
	{
		WSACleanup();
		cout << "����˴����׽���ʧ��" << endl;
		return;
	}

	//���׽���
	sockaddr_in	addrServer;
	memset(&addrServer, 0, sizeof(sockaddr_in));
	addrServer.sin_family = AF_INET;
	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addrServer.sin_port = htons(port);
	if (bind(sServer, (SOCKADDR*)& addrServer, sizeof(addrServer)) == SOCKET_ERROR)
	{
		WSACleanup();
		cout << "����˰��׽��ֳ���" << endl;
		return;
	}

	//�����׽���
	if (SOCKET_ERROR == listen(sServer, 1))
	{
		WSACleanup();
		cout << "����˼����׽��ֳ���" << endl;
		return;
	}

	while (1)
	{
		//�ȵ������׽���
		SOCKET sAccept;
		sockaddr_in addrClient;
		memset(&addrClient, 0, sizeof(sockaddr_in));
		int addrClientLen = sizeof(addrClient);
		sAccept = accept(sServer, (SOCKADDR*)&addrClient, &addrClientLen);
		if (sAccept == INVALID_SOCKET)
		{
			closesocket(sServer);
			WSACleanup();
			cout << "�����׽���ʧ��" << endl;
			return;
		}

		//��������
		char bufRead[128];
		SessionMessage session_info;
		int nReadLen = recv(sAccept, bufRead, sizeof(session_info), 0);
		session_info.number = (((SessionMessage *)bufRead)->number);
		session_info.length = (((SessionMessage *)bufRead)->length);
		cout << "������յ� SessionMessage: ������=" << session_info.number << " ÿ�����Ĵ�С=" << session_info.length << endl;
		double total_mb = 1.0 * session_info.number * session_info.length / 1024 / 1024;
		//cout << total_mb << " MiB in total" << endl;
		printf("%.3f MiB in total\n", total_mb);

		const int total_len = sizeof(int32_t) + session_info.length;
		char* p = new char[total_len];

		float start = now();
		int index = 0;
		for (int i = 0; i < session_info.number; ++i)
		{
			int len = 0;
			while (len != total_len)
			{
				char *tmp = p + len;
				int nReadLen = recv(sAccept, tmp, total_len - len, 0);
				len += nReadLen;
			}
			
			if (0 && nReadLen != total_len)
			{
				cout << "������յ������ݲ���: ʵ�ʽ��ܵ������ݴ�С=" << nReadLen << " Ӧ�ý��յ������ݴ�С=" << total_len << endl << endl;
				goto end_point;
			}

			PayloadMessage* payload = (PayloadMessage*)p;

			if (payload->length != session_info.length)
			{
				cout << "������յ��İ������ݲ���: ʵ�ʽ��ܵ������ݴ�С=" << payload->length << " Ӧ�ý��յ������ݴ�С=" << session_info.length << endl;
				break;
			}

			//int ack = htonl(payload->length);
			int ack = payload->length;
			send(sAccept, (char *)&ack, sizeof(ack), 0);
			//cout << index++;
		}
		float elapsed = now() - start;
		printf("%.3f seconds\n%.3f MiB/s\n\n", elapsed, total_mb / elapsed);

end_point:
		delete[] p;
		closesocket(sAccept);
	}
	closesocket(sServer);
	WSACleanup();
}

void client_logic(char *ip_addr, int port, int buffer_count, int buffer_length)
{
	//����SOCKET
	SOCKET sClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sClient == INVALID_SOCKET)
	{
		WSACleanup();
		cout << "�ͻ��˴����׽���ʧ��" << endl;
		return;
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
		cout << "�ͻ������ӷ�����ʧ��" << endl;
		return;
	}

	BOOL setFalse = FALSE;
	int ret = setsockopt(sClient, IPPROTO_TCP, TCP_NODELAY, (char *)&setFalse, sizeof(BOOL));
	if (ret != 0)
	{
		cout << "fck " << GetLastError() << endl;
	}

	int number = buffer_count;
	int length = buffer_length;
	SessionMessage sessionMessage = { 0, 0 };
	sessionMessage.number = (number);
	sessionMessage.length = (length);
	send(sClient, (char *)&sessionMessage, sizeof(sessionMessage), 0);
	cout << "�ͻ��˷��� SessionMessage: ������=" << sessionMessage.number << " ÿ�����Ĵ�С=" << sessionMessage.length << endl;

	const int total_len = sizeof(int32_t) + length;
	char *p = new char[total_len];
	PayloadMessage* payload = (PayloadMessage*)p;
	payload->length = length;

	for (int i = 0; i < length; ++i)
	{
		payload->data[i] = "0123456789ABCDEF"[i % 16];
	}

	cout << "�ͻ���׼����ʼ���� PayloadMessage..." << endl;
	for (int i = 0; i < number; ++i)
	{
		int send_size = send(sClient, (char *)payload, total_len, 0);
		if (send_size != total_len)
		{
			cout << "�ͻ��˷������ݳ���: ʵ�ʷ��͵����ݴ�С=" << send_size << " Ӧ�÷��͵����ݴ�С=" << total_len << endl;
			break;
		}

		int ack = 0;
		int nReadLen = recv(sClient, (char *)&ack, sizeof(ack), 0);
		//ack = ntohl(ack);
		if (ack != length)
		{
			cout << "�ͻ����յ���ȷ�����ݳ���: ʵ���յ������ݴ�С=" << ack << " Ӧ���յ������ݴ�С=" << length << endl;
			break;
		}

		//cout << index++;
	}

	delete[] payload;
	closesocket(sClient);
	WSACleanup();
}

int main(int argc, char *argv[])
{
	WSADATA VersionData;
	WSAStartup(MAKEWORD(2, 2), &VersionData);

	Optinons opt;
	app_type app_t = parse_command(argc, argv, &opt);

	if (app_t == app_server_type)
	{
		cout << "port: " << opt.port << "\naccepting..." << endl;
		server_logic(opt.port);
	}
	else if (app_t == app_client_type)
	{
		cout << "connecting " << opt.ip_addr << ":" << opt.port << endl;
		cout << "buffer length: " << opt.buffer_length << endl;
		cout << "number of buffers: " << opt.buffer_count << endl;
		client_logic(opt.ip_addr, opt.port, opt.buffer_count, opt.buffer_length);
	}
	else if (app_t = app_none_type)
	{
		cout << "ttcp option wrong" << endl;
		return 1;
	}
		
	return 0;
}