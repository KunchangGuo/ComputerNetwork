#ifndef CLIENT_CLIENT_H
#define CLIENT_CLIENT_H

#include <WinSock2.h>
#include "PacketManager.h"
#include "Util.h"
#include <vector>
#pragma comment(lib, "ws2_32.lib")


class Client {
private:
#define ROUTER
#define DEBUG_CORRUPT

#define ROUTER_IP_ADDR ("127.0.0.200")  // params of router
#define ROUTER_PORT 9999
#define SERVER_IP_ADDR ("127.0.0.100")  // params of server
#define SERVER_PORT 8888
#define WND_FRAME_SIZE 10
#define SEND_BUF_SIZE 10240
#define RECV_BUF_SIZE 10240
#define DATA_BUF_SIZE SEND_BUF_SIZE
#ifdef DEBUG_CORRUPT
#define CORRUPT_RATE 200
#endif

private:
    enum STATE {
        CLOSED, SYN_SENT, ESTABLISHED, FIN_WAIT
    };
    enum EVENT {
        RDT_SEND, RDT_RCV, TIMEOUT
    };
    typedef bool (Client::*FP)();
    typedef struct {
        STATE state;
        EVENT event;
        FP fp;
    } EventHandlerEntry;
    STATE state;
    EVENT event;
    EventHandlerEntry event_handler_table[6][3] = {
            {{CLOSED,      RDT_SEND, &Client::Closed_Send},    {CLOSED,      RDT_RCV, &Client::Null_Func},     {CLOSED,      TIMEOUT, &Client::Null_Func}},
            {{SYN_SENT,    RDT_SEND, &Client::SynSent_Send},   {SYN_SENT,    RDT_RCV, &Client::SynSent_Rcv},   {SYN_SENT,    TIMEOUT, &Client::SynSent_Timeout}},
            {{ESTABLISHED, RDT_SEND, &Client::Establish_Send}, {ESTABLISHED, RDT_RCV, &Client::Establish_Rcv}, {ESTABLISHED, TIMEOUT, &Client::Establish_Timeout}},
            {{FIN_WAIT,    RDT_SEND, &Client::FinWait_Send},   {FIN_WAIT,    RDT_RCV, &Client::FinWait_Rcv},   {FIN_WAIT,    TIMEOUT, &Client::FinWait_Timeout}},
    };
    STATE get_state();
    void set_state(STATE state);
    void set_event(EVENT event);
    bool handle_event();
    bool Null_Func();
    bool Closed_Send();
    bool SynSent_Send();
    bool SynSent_Rcv();
    bool SynSent_Timeout();
    bool Establish_Send();
    bool Establish_Rcv();
    bool Establish_Timeout();
    bool FinWait_Send();
    bool FinWait_Rcv();
    bool FinWait_Timeout();

    WSAData wsa;
    SOCKET client_socket;
    SOCKADDR_IN server_addr;
    int server_addr_len;
    UDP_Packet *send_packet;
    UDP_Packet *recv_packet;
    unsigned short MSS;    // maximum segment size
    char send_buf[SEND_BUF_SIZE];
    char recv_buf[RECV_BUF_SIZE];
    char data_buf[DATA_BUF_SIZE];
    vector<UDP_Packet*> send_packet_buf;    // frames <= 20
    DWORD time_out;
    int retry_count;        // local_retry count for resent
    int local_retry;        // local local_retry counter
    uint32_t seq;
    uint32_t ack;
    uint32_t primary_base_seq;
    const string res_dir = "..\\resource\\";   // resource directory containing files to be sent
    const string log_dir = "..\\log\\";        // log directory containing log files
    fstream send_file;  // send file handler
    string send_file_name;  // send file name
    uint32_t send_file_size;    // send file size
    uint32_t acc_sent_size; // accumulated sent size (bytes)
    TIME_POINT global_start;    // global timer start point
    TIME_POINT local_start;   // local timer start point
    double duration;    // duration for sending file, s
    double bandwidth;   // bandwidth for sending file, KB/s
    int acc_sent_packet;   // accumulated sent packets' number
    int acc_resent_packet; // accumulated resent packets' number
    bool ready_to_close; // intermediate state for closing connection
#ifdef DEBUG_CORRUPT
    int corrupt_count = 0;  // counter for corrupting packet
    int acc_corrupt = 0;    // accumulated corrupt packet number
#endif

    bool choose_send_file(const string &dir);    // choose file to send
    void set_timeout(int sec, int usec);    // set timeout for recvfrom(), which mode is blocking-timeout
    void set_retry_count(int count);    // set local_retry count for retransmission
    void send_data(uint8_t *data, uint16_t data_len, PacketManager::PacketType packet_type);   // send data
    void print_log();

    void init();    // initialize client
    void main_process();    // main process of client
    void reset();   // reset client


public:
    Client() {
        init();
        main_process();
    }
    ~Client() {
        closesocket(client_socket);
        WSACleanup();
    }
};


#endif //CLIENT_CLIENT_H

