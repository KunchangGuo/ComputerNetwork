#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H
#include <WinSock2.h>
#include "PacketManager.h"
#include "Util.h"
#pragma comment(lib, "ws2_32.lib")


class Server {

private:
#define PORT 8888
#define IP_ADDR ("127.0.0.100")
#define SEND_BUF_SIZE 10240
#define RECV_BUF_SIZE 10240

    int state;
    enum State {
        CLOSED,
        LISTEN,
        SYN_RCVD,
        ESTABLISHED,
        LAST_ACK
    };

    WSAData wsa;
    SOCKET server_socket;
    SOCKADDR_IN server_addr;
    SOCKADDR_IN client_addr;
    int client_addr_len;
    UDP_Packet* send_packet;
    UDP_Packet* recv_packet;
    unsigned short MSS;   // maximum segment size
    char send_buf[SEND_BUF_SIZE];
    char recv_buf[RECV_BUF_SIZE];
    DWORD time_out; // timeout for recvfrom(), which mode is blocking-timeout
    int retry_count;    // retry count for retransmission
    unsigned int seq;
    unsigned int ack;
    const string download_dir = "..\\download\\";   // download directory containing files to be received
    const string log_dir = "..\\log\\";   // log directory containing log files
    string recv_file_name;  // file name to be received
    unsigned int recv_file_size;   // file length to be received
    unsigned int acc_recv_size = 0; // accumulated received size
    fstream dump_file;  // file to be received
    double duration;
    double bandwidth;
    int acc_corrupt;

    void set_timeout(int sec, int usec);    // set timeout for recvfrom()
    void set_retry_count(int count);    // set retry count for retransmission
    bool stop_and_wait_send_and_recv(u8 *data, u16 data_len, PacketManager::PacketType packet_type);    // stop and wait protocol for send and recv
    void parse_recv_packet();    // parse packet received
    void print_log();   // print log

    void init();    // initialize server
    void start();   // start server process
    void reset();   // reset server


public:
    Server() {init(); start();} // fields are initialized in included functions
    ~Server() { closesocket(server_socket); WSACleanup();}
};


#endif //SERVER_SERVER_H
