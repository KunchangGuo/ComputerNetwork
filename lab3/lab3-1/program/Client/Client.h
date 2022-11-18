#ifndef CLIENT_CLIENT_H
#define CLIENT_CLIENT_H
#include <WinSock2.h>
#include "PacketManager.h"
#include "Util.h"
#pragma comment(lib, "ws2_32.lib")

class Client {
private:
#define SERVER_PORT 8888
#define CLIENT_PORT 8887
#define SERVER_IP_ADDR ("127.0.0.1")
#define CLIENT_IP_ADDR ("127.0.0.1")
#define SEND_BUF_SIZE 1024
#define RECV_BUF_SIZE 1024
#define DATA_BUF_SIZE SEND_BUF_SIZE

    int state;
    enum State{
        CLOSED,
        SYN_SENT,
        ESTABLISHED,
        FIN_WAIT_1,
        FIN_WAIT_2,
    };

    WSAData wsa;
    SOCKET client_socket;
    SOCKADDR_IN server_addr;
    SOCKADDR_IN client_addr;
    int server_addr_len;
    int client_addr_len;
    UDP_Packet* send_packet;
    UDP_Packet* recv_packet;
    unsigned short MSS;    // maximum segment size
    char send_buf[SEND_BUF_SIZE];
    char recv_buf[RECV_BUF_SIZE];
    char data_buf[DATA_BUF_SIZE];
    DWORD time_out;
    int retry_count;    // retry count for retransmission
    unsigned int seq;
    unsigned int ack;
    const string res_dir = "..\\resource\\";   // resource directory containing files to be sent
    const string log_dir = "..\\log\\";   // log directory containing log files
    string send_file_name;  // file name to be sent
    unsigned int send_file_size;    // file size to be sent
    unsigned int acc_sent_size; // accumulated sent size
    fstream send_file;  // file to be sent

    bool choose_send_file(const string& dir);    // choose file to send
    void set_timeout(int sec, int usec);    // set timeout for recvfrom(), which mode is blocking-timeout
    void set_retry_count(int count);    // set retry count for retransmission
    bool stop_and_wait_send_and_recv(u8* data, u16 data_len, PacketManager::PacketType packet_type);   // stop and wait protocol for send and recv
    void parse_recv_packet();    // parse packet received

    void init();    // initialize client
    void start();   // start client process
    void reset();   // reset client


public:
    Client() {init(); start();} // fields are initialized in included functions
    ~Client() { closesocket(client_socket); WSACleanup();}
};


#endif //CLIENT_CLIENT_H
