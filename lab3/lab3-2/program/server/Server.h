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

    enum STATE {
        CLOSED,
        LISTEN,
        SYN_RCVD,
        ESTABLISHED,
        LAST_ACK
    };
    enum EVENT {
        RDT_RCV,
        RDT_SEND,
        TIMEOUT
    };
    typedef bool (Server::*FP)();
    typedef struct {
        STATE state;
        EVENT event;
        FP fp;
    } EventHandlerEntry;
    STATE state;
    EVENT event;
    EventHandlerEntry event_handler_table[5][3] = {
            {{CLOSED,      RDT_RCV, &Server::Null_Func}, {CLOSED,      RDT_SEND, &Server::Null_Func}, {CLOSED,      TIMEOUT, &Server::Null_Func}},
            {{LISTEN,      RDT_RCV, &Server::Listen_Rcv}, {LISTEN,      RDT_SEND, &Server::Null_Func}, {LISTEN,      TIMEOUT, &Server::Null_Func}},
            {{SYN_RCVD,    RDT_RCV, &Server::SynRcvd_Rcv}, {SYN_RCVD,    RDT_SEND, &Server::SynRcvd_Send}, {SYN_RCVD,   TIMEOUT, &Server::SynRcvd_Timeout}},
            {{ESTABLISHED, RDT_RCV, &Server::Establish_Rcv}, {ESTABLISHED, RDT_SEND, &Server::Establish_Send}, {ESTABLISHED, TIMEOUT, &Server::Null_Func}},
            {{LAST_ACK,    RDT_RCV, &Server::LastAck_Send}, {LAST_ACK,    RDT_SEND, &Server::LastAck_Send}, {LAST_ACK,    TIMEOUT, &Server::LastAck_Timeout}},
    };

    bool Null_Func();

    bool Listen_Rcv();

    bool SynRcvd_Rcv();

    bool SynRcvd_Send();

    bool SynRcvd_Timeout();

    bool Establish_Rcv();

    bool Establish_Send();

    bool LastAck_Rcv();

    bool LastAck_Send();

    bool LastAck_Timeout();

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
    uint32_t primary_expected_seq;
    const string download_dir = "..\\download\\";   // download directory containing files to be received
    const string log_dir = "..\\log\\";   // log directory containing log files
    string recv_file_name;  // file name to be received
    unsigned int recv_file_size;   // file length to be received
    unsigned int acc_recv_size; // accumulated received size
    fstream dump_file;  // file to be received
    TIME_POINT global_start;    // global local_start time
    double duration;
    double bandwidth;
    int acc_corrupted_packet;

    void set_timeout(int sec, int usec);    // set timeout for recvfrom()
    void set_retry_count(int count);    // set retry count for retransmission
    void parse_recv_packet();    // parse packet received
    void send_data(uint8_t *data, uint16_t data_len, PacketManager::PacketType packet_type);   // send data
    void print_log();   // print log

    void init();    // initialize server
    void main_process();   // main_process server process
    void reset();   // reset server


public:
    Server() { // fields are initialized in included functions
        init();
        main_process();
    }
    ~Server() {
        closesocket(server_socket);
        WSACleanup();
    }

    STATE get_state();

    void set_state(STATE state);

    void set_event(EVENT event);

    bool handle_event();

    void resend_data();
};


#endif //SERVER_SERVER_H
