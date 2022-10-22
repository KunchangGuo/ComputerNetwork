#include "server.h"


#define DEBUG 1

Server::Server()
{
	if (initWinSock() == -1)
	{
		return;
	}
	if (initSocket() == -1)
	{
		return;
	}
	if (bindToPort() == -1)
	{
		return;
	}
	if (configureListen() == -1)
	{
		return;
	}
	startThread();
	close();
		
}

Server::~Server()
{
}

int Server::initWinSock()
{
	WSADATA wsa;
	//cout << "Initialising Winsock..." << endl;;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		cout<<this->util.currentTime().append(" Failed to start: %d\n", WSAGetLastError());
		return -1;
	}
	cout << this->util.currentTime().append(" Winsock initialised.\n");
	return 0;
}

int Server::initSocket()
{
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		cout << this->util.currentTime().append(" Could not create socket: ") << WSAGetLastError() << endl;
		return -1;
	}
	cout<<this->util.currentTime().append(" Listening socket created.")<<endl;
	return 0;
}

int Server::bindToPort()
{
#if DEBUG
	serverAddress.sin_port = htons(PORT);
#else
	int listeningPort;
	cout << "Please enter the port: ";
	cin >> listeningPort;
	serverAddress.sin_port = htons(listeningPort);
#endif
	
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;	// set as localhost
	
	if (bind(s, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) // Bind: cannot have 2 sockets bound to the same port
	{
		cout<<this->util.currentTime().append(" Bind failed with error code: ")<< WSAGetLastError()<<endl;
		return -1;
	}
	cout << this->util.currentTime().append(" Bind done.") << endl;
	return 0;
}

int Server::configureListen()
{
	if (listen(s, SOMAXCONN) == SOCKET_ERROR) //Places sListen socket in a state in which it is listening for an incoming connection. Note:SOMAXCONN = Socket Oustanding Max connections, if we fail to listen on listening socket...
	{
		cout << this->util.currentTime().append(" Failed to listen: ") << WSAGetLastError() << endl;
		return -1;
	}
	cout << this->util.currentTime().append(" Listening... Waiting for clients to connect...") << endl;
	return 0;
}

int Server::startThread()
{
	clientCount = 0;
	clientList = new ClientInfo();
	ClientInfo* currClient = clientList;
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)manipThread, (LPVOID)this, NULL, NULL);
	while (1)
	{
		// accept a new client
		currClient->s = new SOCKET;
		currClient->nextClient = new ClientInfo();
		int len = sizeof(serverAddress);
		if ((*(currClient->s) = accept(s, (struct sockaddr*)&serverAddress, &len)) == INVALID_SOCKET)
		{
			cout << this->util.currentTime().append(" Failed to accept: ") << WSAGetLastError() << endl;
			return -1;
		}

		// start a new thread for the client
		currClient->id = clientCount;
		struct params p;
		p.server = this;
		p.client = currClient;
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)clientHandlerThread, (LPVOID)&p, NULL, NULL);
		
		// adjust client list
		cout << this->util.currentTime().append(" Accepted client ") << currClient->id << endl;
		clientCount++;
		currClient = currClient->nextClient;
	}
	return 0;
}

void Server::clientHandlerThread(LPVOID lpParameter)
{
	struct params* p = (struct params*)lpParameter;
	Server* server = p->server;
	ClientInfo* client = p->client;
	int recvSize;
	char recvBuffer[BUFFER_SIZE] = {0};
	cout << server->getUtil().currentTime().append(" Client handling thread created.\n");
	
	// send client id back
	string commStr = string("").append(COMM_IDEN).append(" ").append(to_string(client->id));
	char* comm = new char[commStr.length() + 1];
	strcpy_s(comm, commStr.length() + 1, commStr.c_str());
	send(*client->s, comm, strlen(comm), 0);
	cout << server->getUtil().currentTime().append(" ClientID ").append(to_string(client->id)).append(" sent back.\n");
	
	// listening to client
	while (1)
	{
		if ((recvSize = recv(*client->s, recvBuffer, sizeof(recvBuffer), 0)) == SOCKET_ERROR)
		{
			cout << server->getUtil().currentTime().append(" Receive failed: ").append(to_string(WSAGetLastError())).append(" Client ").append(to_string(client->id).append(" disconnected.\n"));
			break;
		}
		if (client->status == OFFLINE)
		{
			cout << server->getUtil().currentTime().append(" Client ").append(to_string(client->id)).append(" disconnected.\n");
			break;
		}
		//cout << "Reply received: " << recvBuffer << endl;
		parseReceived(recvBuffer, server);
		memset(recvBuffer, 0, BUFFER_SIZE);
	}
	/*while (1)
	{
		if ((recvSize = recv(*client, recvBuffer, sizeof(recvBuffer), 0)) == SOCKET_ERROR)
		{
			cout << "Receive failed: " << WSAGetLastError() << endl;
			break;
		}
		cout << "Reply received: " << recvBuffer << endl;
		memset(recvBuffer, 0, BUFFER_SIZE);

		cout << "Please input messages to be sent to client: " << endl;
		gets_s(sendBuffer);
		if ((sendSize = send(*client, sendBuffer, strlen(sendBuffer), 0)) == SOCKET_ERROR)
		{
			printf("Send failed: %d\n", WSAGetLastError());
			break;
		}
		printf("Data sended.\n");
		memset(sendBuffer, 0, BUFFER_SIZE);
	}*/
}

void Server::manipThread(LPVOID lpParameter)
{
	Server* server = (Server*)lpParameter;
	cout << server->getUtil().currentTime().append(" Manipulation thread started.") << endl;
	char commBuffer[BUFFER_SIZE] = { 0 };
	while (1)
	{
		gets_s(commBuffer);
		parseCommand(commBuffer, server);
		memset(commBuffer, 0, BUFFER_SIZE);
	}
}

void Server::parseCommand(char* comm, Server* server)
{
	string commStr = comm;
	if (commStr.substr(0, 4) == COMM_QUIT)	// QUIT
	{
		cout << server->getUtil().currentTime().append(" Quited\n");
		
		closesocket(server->s);
		WSACleanup();
		exit(0);
	}
	else if (commStr.substr(0, 4) == COMM_HELP)
	{
		cout << server->getUtil().currentTime().append(" Help:\n\t");
		cout << helpTxt << endl;
	}
	else if (commStr.substr(0, 4) == COMM_LIST)	// LIST3
	{
		cout << server->getUtil().currentTime().append(" Clients listed: \n");
		
		ClientInfo* currClient = server->clientList;
		while (currClient && currClient->id != -1)
		{
			cout << "\t Client " << currClient->id << " : " << ((currClient->name == nullptr) ? "NULL" : (currClient->name)) << endl;
			currClient = currClient->nextClient;
		}
	}
	else if (commStr.substr(0, 4) == COMM_SEND)	// SEND [message] TO [recv id]
	{
		cout << server->getUtil().currentTime().append(" ").append(commStr) << endl;
		
		int recvID = atoi(server->getUtil().getSubStr(commStr, commStr.find("TO") + 3).c_str());
		string newComm = server->getUtil().getSubStr(commStr, 0, commStr.find("TO")-1).append("FROM -1");	//new: SEND [message] FROM -1
		sendToClient(newComm, findClientByID(recvID, server->clientList));
	}
	else if (commStr.substr(0,4) == COMM_BROA) // BROA [message]
	{
		cout << server->getUtil().currentTime().append(" ").append(commStr);
		
		ClientInfo* currClient = server->clientList;
		string newComm = commStr.append(" FROM -1");	// new: BROA [message] FROM -1
		while (currClient != NULL && currClient->id != -1)
		{
			sendToClient(commStr, currClient->s);
			currClient = currClient->nextClient;
		}
	}
	else if (commStr.substr(0, 4) == COMM_KICK)	// KICK [id]
	{
		int kickID = atoi(server->getUtil().getSubStr(commStr, commStr.find(' ') + 1).c_str());
		
		cout << server->getUtil().currentTime().append(" Kicked client ").append(to_string(kickID)) << endl;
		
		ClientInfo* currClient = server->clientList;
		while (currClient != NULL)
		{
			if (currClient->id == kickID)
			{
				currClient->status = OFFLINE;
				closesocket(*currClient->s);
				break;
			}
			currClient = currClient->nextClient;
		}
	}
	else
	{
		cout << "Please enter correct command.[HELP]" << endl;
	}
}

void Server::parseReceived(char* recv, Server* server)
{
	string recvStr = recv;
	
	if (recvStr.substr(0, 4) == RECV_SEND)	// SEND [message] TO [receive id] FROM [send id]
	{
		cout << server->getUtil().currentTime().append(" ").append(recvStr) << endl;
		
		int recvID = atoi(server->getUtil().getSubStr(recvStr, recvStr.find("TO") + 3, recvStr.find("FROM") - 2).c_str());
		int sendID = atoi(server->getUtil().getSubStr(recvStr, recvStr.find("FROM") + 5).c_str());
		string newComm = server->getUtil().getSubStr(recvStr, 0, recvStr.find("TO") - 1).append("FROM ").append(to_string(sendID)); // new:  SEND [message] FROM [send id]
		sendToClient(newComm, findClientByID(recvID, server->clientList));
	}
	else if (recvStr.substr(0, 4) == RECV_BROA)	// BROA [message] FROM [send id]
	{
		cout << server->getUtil().currentTime().append(" ").append(recvStr) << endl;
		
		int sendID = atoi(server->getUtil().getSubStr(recvStr, recvStr.find("FROM") + 5).c_str());
		ClientInfo* currClient = server->clientList;
		while (currClient != NULL)	// forward message to all clients
		{
			if (currClient->id != sendID && currClient->id >= 0)
			{
				sendToClient(recvStr, currClient->s);
			}
			currClient = currClient->nextClient;
		}
	}
	else if (recvStr.substr(0,4)==RECV_NAME)	// NAME [id]:[new name]
	{
		int id = atoi(server->getUtil().getSubStr(recvStr, recvStr.find(' ') + 1, recvStr.find(':') - 1).c_str());
		string name = server->getUtil().getSubStr(recvStr, recvStr.find(':') + 1);
		ClientInfo* currClient = server->clientList;
		while (currClient != NULL)
		{
			if (currClient->id == id)
			{
				currClient->name = new char[name.length() + 1];
				strcpy_s(currClient->name, name.length() + 1, name.c_str());
				cout << server->getUtil().currentTime().append(" ").append("Client ").append(to_string(id)).append(" changed name to ").append(name) << endl;
				break;
			}
			currClient = currClient->nextClient;
		}
	}
	else
	{
		cout << server->getUtil().currentTime().append(" Invalid command received.\n");
	}
}

SOCKET* Server::findClientByID(int id, ClientInfo* clientList)
{
	ClientInfo* currClient = clientList;
	while (currClient)
	{
		if (currClient->id == id)
		{
			return currClient->s;
		}
		currClient = currClient->nextClient;
	}
	return nullptr;
}

SOCKET* Server::findClientByName(char* name, ClientInfo* clientList)
{
	ClientInfo* currClient = clientList;
	while (currClient)
	{
		if (strcmp(name, currClient->name) == 0)
		{
			return currClient->s;
		}
		currClient = currClient->nextClient;
	}
	return nullptr;
}

void Server::sendToClient(string comm, SOCKET* client)
{
	char* sendMessage = new char[comm.length() + 1];
	strcpy_s(sendMessage, comm.length() + 1, comm.c_str());
	if (send(*client, sendMessage, strlen(sendMessage), 0) == SOCKET_ERROR)
	{
		cout << "Send failed: " << WSAGetLastError() << endl;
	}
}

int Server::close()
{
	closesocket(s);
	WSACleanup();
	return 0;
}
