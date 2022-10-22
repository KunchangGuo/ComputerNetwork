#include <WinSock2.h>
#include "config.h"
#include "util.h"
#pragma	comment(lib, "ws2_32.lib")
using namespace std;

class Client
{
public:
	Client();
	~Client();

private:
	SOCKET s;
	struct sockaddr_in serverAddress;
	
	int initWinSock();
	int initSocket();
	int connectToServer();
	int startThread();
	int close();

	static void recvThread(LPVOID lpParameter);
	static void sendToServer(string comm, SOCKET* server);
	static int parseReceived(char* recv, Client* client);
	static int parseCommand(char* comm, Client* client);

private:
	int id;
	string name;
	int exitFlag;
	Util util;

public:
	SOCKET* getServerSocket() { return &s; }
	int getID() { return id; }
	void setExitFlag(int flag) { exitFlag = flag; }
	Util getUtil() { return util; }
};
