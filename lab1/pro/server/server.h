#include <iostream>
#include <WinSock2.h>
#include <string>
#include "util.h"
#include "config.h"
#pragma comment(lib, "ws2_32.lib")
using namespace std;

class ClientInfo
{
public:
	int id;
	char* name;
	SOCKET* s;
	ClientInfo* nextClient;
	bool status;
	
	ClientInfo()
	{
		id = -1;
		name = NULL;
		s = NULL;
		nextClient = NULL;
		status = ONLINE;
	}
};

class Server
{
public:
	Server();
	~Server();

private:
	SOCKET s;
	struct sockaddr_in serverAddress;
	ClientInfo* clientList;
	int clientCount;

	int initWinSock();
	int initSocket();
	int bindToPort();
	int configureListen();
	int startThread();
	int close();

	static void clientHandlerThread(LPVOID lpParameter);
	static void manipThread(LPVOID lpParameter);
	static void parseCommand(char* comm, Server* server);
	static void parseReceived(char* recv, Server* server);

private:
	Util util;
	
public:
	ClientInfo* getClientList() { return clientList; }
	Util getUtil() { return util; }

private:
	static SOCKET* findClientByID(int id, ClientInfo* clientList);
	static SOCKET* findClientByName(char* name, ClientInfo* clientList);
	static void sendToClient(string comm, SOCKET* client);
};

struct params {
	Server* server;
	ClientInfo* client;
};