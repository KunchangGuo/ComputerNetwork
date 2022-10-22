#include <iostream>
#include <string>
#include "client.h"
#define DEBUG 1

Client::Client()
{
	id = -1; name = ""; exitFlag = 0;
	if (initWinSock() == -1)
	{
		return;
	}
	if (initSocket() == -1)
	{
		return;
	}
	if (connectToServer() == -1)
	{
		return;
	}
	startThread();
	close();
}

Client::~Client()
{
}

int Client::initWinSock()
{
	WSADATA wsa;
	//cout << this->getUtil().currentTime().append(" Initialising Winsock...") << endl;;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		cout << this->getUtil().currentTime().append(" Failed to start: ") << WSAGetLastError() << endl;
		return -1;
	}
	cout << this->getUtil().currentTime().append(" Winsock initialised.\n");
	return 0;
}

int Client::initSocket()
{
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		cout << this->getUtil().currentTime().append(" Could not create socket: ") << WSAGetLastError() << endl;
		return -1;
	}
	cout << this->getUtil().currentTime().append(" Socket created.\n");
	return 0;
}

int Client::connectToServer()
{
#if DEBUG
	this->name = "";
	serverAddress.sin_addr.s_addr = inet_addr(SERVERADDRESS);	// localhost
	serverAddress.sin_port = htons(PORT);
#else
	string serverAddressString;
	int serverPort;
	cout << "Please enter your name: ";
	getline(cin, this->name);
	cout << "Please enter the server address: ";
	getline(cin, serverAddressString);
	cout << "Please enter the server port: ";
	cin >> serverPort;
	serverAddress.sin_addr.s_addr = inet_addr(serverAddressString.c_str());	// server address
	serverAddress.sin_port = htons(serverPort);
#endif // DEBUG

	serverAddress.sin_family = AF_INET;

	if (connect(s, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
	{
		cout << this->getUtil().currentTime().append(" Connect error: ") << WSAGetLastError() << endl;
		return -1;
	}

	cout << this->getUtil().currentTime().append(" Connected. Welcome to the chat room!\n");
	return 0;
}

int Client::startThread()
{
	char commBuffer[BUFFER_SIZE] = {0};
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)recvThread, (LPVOID)this, NULL, NULL);	// start receive thread
	//while (true)
	//{
	//	if (this->id != -1)	// wait until id is configured
	//	{
	//		sendToServer(string("").append(COMM_NAME).append(" ").append(this->name), &s);	// send my name to server
	//		break;
	//	}
	//}
	while (!exitFlag)
	{
		gets_s(commBuffer);
		parseCommand(commBuffer, this);
		/*if (send(s, commBuffer, strlen(commBuffer), 0) == SOCKET_ERROR)
		{
			cout << "Send failed: " << WSAGetLastError() << endl;
			break;
		}
		cout << "Data sended." << endl;*/
		memset(commBuffer, 0, BUFFER_SIZE);
	}
	return 0;
}

int Client::parseReceived(char* recv, Client* client)
{
	string recvStr = recv;
	if (recvStr.substr(0, 4) == RECV_IDEN)	// IDEN [id]
	{
		
		client->id = atoi(client->getUtil().getSubStr(recvStr, recvStr.find(' ') + 1).c_str());	// get allocated id from server
		sendToServer(string("").append(COMM_NAME).append(" ").append(to_string(client->id)).append(":").append(client->name), client->getServerSocket());	// send my name to server

		cout << client->getUtil().currentTime().append(" Accepted. Your ID is ").append(to_string(client->id)) << endl;
	}
	else if (recvStr.substr(0, 4) == RECV_SEND)	// SEND [message] FROM [send id]
	{
		int sendID = atoi(client->getUtil().getSubStr(recvStr, recvStr.find("FROM") + 5).c_str());
		string message = client->getUtil().getSubStr(recvStr, recvStr.find(' ') + 1, recvStr.find("FROM") - 2);

		cout << client->getUtil().currentTime().append(" <--- ").append(to_string(sendID)).append(" : ").append(message) << endl;
	}
	else if (recvStr.substr(0, 4) == RECV_BROA)	// BROA [message] FROM [send id]
	{
		int sendID = atoi(client->getUtil().getSubStr(recvStr, recvStr.find("FROM") + 5).c_str());
		string message = client->getUtil().getSubStr(recvStr, recvStr.find(' ') + 1, recvStr.find("FROM") - 2);

		cout<<client->getUtil().currentTime().append(" ALL <--- ").append(to_string(sendID)).append(" : ").append(message) << endl;
	}
	return 0;
}

int Client::parseCommand(char* comm, Client* client)
{
	string commStr = comm;
	if (commStr.substr(0, 4) == COMM_QUIT)	// QUIT
	{
		cout << client->getUtil().currentTime().append(" Quited.\n");
		client->setExitFlag(1);
	}
	else if (commStr.substr(0, 4) == COMM_SEND)	// IN : SEND [message] TO [receive id]		OUT : SEND [message] TO [receive id] FROM [send id]
	{
		int recvID = atoi(client->getUtil().getSubStr(commStr, commStr.find("TO") + 3).c_str());
		string message = client->getUtil().getSubStr(commStr, commStr.find(' ') + 1, commStr.find("TO") - 2);

		cout << client->getUtil().currentTime().append(" ---> ").append(to_string(recvID)).append(" : ").append(message) << endl;

		string newComm = commStr.append(" FROM ").append(to_string(client->getID()));
		char* sendMessage = new char[newComm.length() + 1];
		strcpy_s(sendMessage, newComm.length() + 1, newComm.c_str());
		if (send(client->s, sendMessage, strlen(sendMessage), 0) == SOCKET_ERROR)
		{
			cout << client->getUtil().currentTime().append(" Send failed: ") << WSAGetLastError() << endl;
		}
	}
	else if (commStr.substr(0, 4) == COMM_BROA)	// IN : BROA [message]		OUT:  BROA [message] FROM [send id]
	{
		string message = client->getUtil().getSubStr(commStr, commStr.find(' ') + 1);

		cout << client->getUtil().currentTime().append(" ---> ALL : ").append(message) << endl;

		string newComm = commStr.append(" FROM ").append(to_string(client->getID()));
		sendToServer(newComm, &client->s);
	}
	return 0;
}

void Client::sendToServer(string comm, SOCKET* server)
{
	char* sendMessage = new char[comm.length() + 1];
	strcpy_s(sendMessage, comm.length() + 1, comm.c_str());
	if (send(*server, sendMessage, strlen(sendMessage), 0) == SOCKET_ERROR)
	{
		cout << "Send failed: " << WSAGetLastError() << endl;
	}
}

void Client::recvThread(LPVOID lpParameter)
{
	Client* client = (Client*)lpParameter;
	char recvBuffer[BUFFER_SIZE] = {0};
	int recvSize;

	while (!client->exitFlag)
	{
		recvSize = recv(client->s, recvBuffer, BUFFER_SIZE, 0);
		if (recvSize < 0)
		{
			cout << client->getUtil().currentTime().append(" Recv failed: ") << WSAGetLastError() << endl;
			return;
		}
		parseReceived(recvBuffer, client);
		//cout << "Received: " << recvBuffer << endl;
		memset(recvBuffer, 0, BUFFER_SIZE);
	}
}

int Client::close()
{
	closesocket(s);
	WSACleanup();
	return 0;
}