#include "Server.h"
#include <iostream>
#include <chrono>
#include <string>
using namespace std;
using namespace chrono;

/*
 * @param sec: seconds
 * @param usec: microseconds
 */
void Server::set_timeout(int sec, int usec) {
    unsigned long on = 1;
    if(ioctlsocket(server_socket, FIONBIO, &on) == SOCKET_ERROR) {
        EXCPT_LOG(string("ioctlsocket() failed with error code : ") + to_string(WSAGetLastError()));
        exit(EXIT_FAILURE);
    }
    time_out = sec * 1000 + usec / 1000;
    PLAIN_LOG("Timeout set to " + to_string(time_out) + " ms");
}

void Server::set_retry_count(int count) {
    retry_count = count;
    PLAIN_LOG(string("Retry count set to ") + to_string(retry_count));
}

void Server::print_log() {
    cout << "=====================" << endl;
    cout << "File " << recv_file_name << " downloaded" << endl;
    cout << "Total bytes received: " << acc_recv_size << endl;
    cout << "Duration: " << duration << " s" << endl;
    cout << "Bandwidth: " << bandwidth << " KB/s" << endl;
    cout << "Total packets corrupted: " << acc_corrupted_packet << endl;
    cout << "=====================" << endl;
}

/*
 * Steps to initialize a server:
 * 1. Initialize Winsock environment
 * 2. Create a UDP socket
 * 3. Bind the socket to specific IP address and port
 */
void Server::init() {
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {    // Initialize Winsock
        EXCPT_LOG(string("WSAStartup() failed with error code : ") + to_string(WSAGetLastError()));
        exit(EXIT_FAILURE);
    }
    PLAIN_LOG(string("Initialized"));

    if ((server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR) {   // Create UDP socket
        EXCPT_LOG(string("socket() failed with error code : ") + to_string(WSAGetLastError()));
        exit(EXIT_FAILURE);
    }
    PLAIN_LOG(string("Socket created"));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(IP_ADDR);    // set server ip address to localhost (another way to do this is to use inet_addr("127.0.0.1"))
    server_addr.sin_port = htons(PORT);         // set server port to 8888
    client_addr_len = sizeof(client_addr);
    if (bind(server_socket, (SOCKADDR *) &server_addr, sizeof(server_addr)) == SOCKET_ERROR) {   // Bind, which is essential to server
        EXCPT_LOG(string("bind() failed with error code : ") + to_string(WSAGetLastError()));
        exit(EXIT_FAILURE);
    }
    PLAIN_LOG(string("Server bound"));

    reset();
}

/*
 *  Main process of server:
 *  Establish connection with client;
 *  Interact with client
 */
void Server::main_process() {
    set_state(Server::STATE::LISTEN);   // set state to LISTEN
    STATE_LOG(string("Server state: LISTEN"));

    while(true) {
        switch (get_state()) {
            case LISTEN:
                while(true) {
                    if (recvfrom(server_socket, recv_buf, RECV_BUF_SIZE, 0, (SOCKADDR *) &client_addr, &client_addr_len) > 0) {
                        set_event(Server::EVENT::RDT_RCV);
                        if(handle_event()) {    // check if syn received
                            global_start = TIME_NOW;    // set global start time
                            set_state(Server::STATE::SYN_RCVD); // change state to SYN_RCVD
                            STATE_LOG(string("Server state: SYN_RCVD"));
                            break;
                        }
                    }
                }
                break;
            case SYN_RCVD:
                set_event(Server::EVENT::RDT_SEND);
                handle_event(); // send syn-ack
                while(true) {
                    if (recvfrom(server_socket, recv_buf, RECV_BUF_SIZE, 0, (SOCKADDR *) &client_addr, &client_addr_len) > 0) {
                        set_event(Server::EVENT::RDT_RCV);
                        if(handle_event()) {    // check if ack received, also set expected seq num
                            set_state(Server::STATE::ESTABLISHED); // change to ESTABLISHED
                            STATE_LOG(string("Server state: ESTABLISHED"));
                            set_event(Server::EVENT::RDT_SEND);
                            handle_event(); // send ack
                            break;
                        }
                    }
                }
                break;
            /*
             *  ESTABLISHED state:
             */
            case ESTABLISHED:
                while(true) {
                    if (recvfrom(server_socket, recv_buf, RECV_BUF_SIZE, 0, (SOCKADDR *) &client_addr, &client_addr_len) > 0) {
                        set_event(Server::EVENT::RDT_RCV);
                        if(acc_recv_size == recv_file_size) {   // check if file received completely
                            if(handle_event()) {    // check if fin received, change state to FIN_WAIT
                                break;
                            }
                        } else {
                            if(handle_event()) {    // check if packet received, and download data
                                set_event(Server::EVENT::RDT_SEND);
                                handle_event(); // send ack
                                DUMP_DATA(&dump_file, DATA(recv_packet), DATA_LEN(recv_packet));    // download data
                                acc_recv_size += DATA_LEN(recv_packet);
                                PLAIN_LOG(string("Downloaded ") + to_string(DATA_LEN(recv_packet)) + " bytes");
                            }
                        }
                    }
                }
                dump_file.close();
                duration = TIME_SPAN(global_start, Util::TimeStampType::SECOND);   // second
                bandwidth = (double)acc_recv_size / duration / 1024;    // KB/s
                set_state(Server::STATE::LAST_ACK); // change to LAST_ACK
                PLAIN_LOG(string("Download completed"));
                STATE_LOG(string("Server state: LAST_ACK"));
                break;
            case LAST_ACK:
                set_event(Server::EVENT::RDT_SEND);
                handle_event(); // send fin_ack
                TIME_POINT local_start = TIME_NOW;
                while(true) {
                    if (recvfrom(server_socket, recv_buf, RECV_BUF_SIZE, 0, (SOCKADDR *) &client_addr, &client_addr_len) > 0) {
                        set_event(Server::EVENT::RDT_RCV);
                        if(handle_event()) {    // check if fin received
                            set_state(Server::STATE::CLOSED); // change to CLOSED
                            STATE_LOG(string("Server state: CLOSED"));
                            print_log();
                            return;
                        }
                    }
                    if (TIME_OUT(local_start)) {
                        EXCPT_LOG(string("TIME OUT"));
                        set_state(Server::STATE::CLOSED); // change to CLOSED
                        print_log();
                        return;
                    }
                }
        }
    }
}

void Server::reset() {
    time_t t;
    srand((unsigned)time(&t));
    seq = rand();
    ack = 0;
    acc_corrupted_packet = 0;
    acc_recv_size = 0;
    set_timeout(2, 0);
    set_retry_count(3);
    PLAIN_LOG(string("Reset done"));
}

bool Server::Null_Func() {
    PLAIN_LOG(string("Null function"));
    return true;
}

bool Server::Listen_Rcv() {
    recv_packet = (UDP_Packet *) recv_buf;
    PLAIN_LOG(string("RCV_PKT ") + STR(recv_packet));
    if (CORRUPTED(recv_packet)) {
        EXCPT_LOG(string("Corrupted packet received: resend packet"));
        return false;
    }
    if(!SYN(recv_packet)) {
        EXCPT_LOG(string("SYN expected"));
        return false;
    }
    File_Info *file_info = (File_Info *) DATA(recv_packet);
    primary_expected_seq = file_info->primary_seq_num;  // get primary seq num as first expected seq num
    uint16_t client_mss = file_info->MSS;
    uint16_t server_mss = RECV_BUF_SIZE;
    MSS = min(client_mss, server_mss);
    recv_file_name = (char*) file_info->file_name;
    recv_file_size = file_info->file_size;
    MKDIR(download_dir);
    dump_file = fstream(download_dir + recv_file_name, ios::out | ios::binary);
    if(!dump_file.is_open()){
        reset();
        EXCPT_LOG(string("Failed to open file"));
        PLAIN_LOG(string("Reset"));
        return false;
    }
    PLAIN_LOG(string("Parsing SYN packet: ") + string("Expected_seq = ") + to_string(primary_expected_seq) + string(", MSS = ") + to_string(MSS) + string(", file_name = ") + recv_file_name + string(", file_size = ") + to_string(recv_file_size));
    ack = CIR_ADD2(SEQ_NUM(recv_packet), (uint32_t)DATA_LEN(recv_packet));    // update ack
    return true;
}
/*
 * Expected ack_psh packet
 */
bool Server::SynRcvd_Rcv() {
    recv_packet = (UDP_Packet *) recv_buf;
    PLAIN_LOG(string("RCV_PKT ") + STR(recv_packet));
    if (CORRUPTED(recv_packet)) {
        EXCPT_LOG(string("Corrupted packet received: resend packet"));
        return false;
    }
    if (!ACK(recv_packet) || !PSH(recv_packet)) {
        EXCPT_LOG(string("Ack_psh expected"));
        return false;
    }
    if(ACK_NUM(recv_packet) != seq) {
        EXCPT_LOG(string("Ack mismatched"));
        return false;
    }
    ack = primary_expected_seq; // set expected seq num
    if(ack != SEQ_NUM(recv_packet)) {   // check if packet expected
        EXCPT_LOG(string("Not expected packet: Expected_seq = ") + to_string(ack) + string(", Received_seq = ") + to_string(SEQ_NUM(recv_packet)));
        return false;
    }
    CIR_ADD1(&ack, (uint32_t)DATA_LEN(recv_packet));    // update expected seq num
    DUMP_DATA(&dump_file, DATA(recv_packet), DATA_LEN(recv_packet));    // download data
    acc_recv_size += DATA_LEN(recv_packet);
    PLAIN_LOG(string("Parsing PSH packet"));
    PLAIN_LOG(string("Downloaded ") + to_string(DATA_LEN(recv_packet)) + " bytes");
    return true;
}

bool Server::SynRcvd_Send() {
    send_data((uint8_t*)&MSS, sizeof(MSS), SYN_ACK_TYPE);
    CIR_ADD1(&seq, uint32_t(DATA_LEN((UDP_Packet*)send_buf)));   // increase seq
    return true;
}

bool Server::SynRcvd_Timeout() {
    return Null_Func();
}

bool Server::Establish_Send() {
    char null_data = 0;
    send_data((uint8_t*)&null_data, 0, ACK_TYPE);
    return true;
}

bool Server::LastAck_Rcv() {
    return Null_Func();
}

bool Server::LastAck_Send() {
    char null_data = 0;
    send_data((uint8_t*)&null_data, 1, FIN_ACK_TYPE);
    CIR_ADD1(&seq, 1);
    return true;
}

bool Server::LastAck_Timeout() {
    return Null_Func();
}

Server::STATE Server::get_state() {
    return this->state;
}

void Server::set_state(Server::STATE state) {
    this->state = state;
}

void Server::set_event(Server::EVENT event) {
    this->event = event;
}

bool Server::handle_event() {
    return (this->*event_handler_table[state][event].fp)();
}

void Server::resend_data() {
    PLAIN_LOG(string("Timeout. Resending packet"));
    sendto(server_socket, (char*) send_packet, TOTAL_LEN(send_packet), 0, (SOCKADDR*)&client_addr, client_addr_len);
    PLAIN_LOG(string("SND_PKT ") + STR(send_packet));
}

bool Server::Establish_Rcv() {
    recv_packet = (UDP_Packet *) recv_buf;
    PLAIN_LOG(string("RCV_PKT ") + STR(recv_packet));
    if (CORRUPTED(recv_packet)) {
        EXCPT_LOG(string("Corrupted packet received: resend packet"));
        return false;
    }
    if (acc_recv_size == recv_file_size) {  // file downloaded completely, expect fin
        if (!FIN(recv_packet)) {
            EXCPT_LOG(string("FIN expected"));
            return false;
        }
        ack = CIR_ADD2(SEQ_NUM(recv_packet), (uint32_t)DATA_LEN(recv_packet));    // update ack
    } else {    // haven't downloaded completely, expect psh
        if (!PSH(recv_packet)) {
            EXCPT_LOG(string("Psh expected"));
            return false;
        }
        if(ack != SEQ_NUM(recv_packet)) {   // check if packet expected
            EXCPT_LOG(string("Not expected packet: Expected_seq = ") + to_string(ack) + string(", Received_seq = ") + to_string(SEQ_NUM(recv_packet)));
            return false;
        }
        CIR_ADD1(&ack, (uint32_t)DATA_LEN(recv_packet));    // update expected seq num
    }
    return true;
}

void Server::send_data(uint8_t *data, uint16_t data_len, PacketManager::PacketType packet_type) {
    send_packet = MAKE_PKT(packet_type, seq, ack, data, data_len);  // make packet
    PLAIN_LOG(string("SND_PKT ") + STR(send_packet));    // log
    memset(send_buf, 0, MSS);   // clear send_buf
    memcpy(send_buf, send_packet, TOTAL_LEN(send_packet));  // copy packet to send_buf
    sendto(server_socket, send_buf, SEND_BUF_SIZE, 0, (SOCKADDR *) &client_addr, client_addr_len);  // send packet
}
