#include "Client.h"
#include <iostream>
#include <chrono>
using namespace std;
using namespace chrono;
/*
 * Open directory -> select a file to send -> get file size
 * FileSystem (C++ 17) to be supported
 */
bool Client::choose_send_file(const std::string &dir) {
    vector<string> files = REL_FILES(dir);
    if( files.empty()){
        EXCPT_LOG("Failed to get files");
        return false;
    }
    int i = 0;
    for(const auto& file: files){
        cout << ++i << " " << file << endl;
    }
    cout << "Choose a file to send: ";
    cin >> i;
    if(i < 1 || i > files.size()){
        EXCPT_LOG("Invalid file index");
        return false;
    }
    send_file_name = files[i-1];
    send_file_size = FILE_SIZE(res_dir + send_file_name);
    send_file = fstream(res_dir + send_file_name, ios::in | ios::binary);
    acc_sent_size = 0;
    PLAIN_LOG(string("File chosen: ") + send_file_name + string(" File size: ") + to_string(send_file_size) + string(" bytes"));
    return true;
}

/*
 * Set time_out and make recvfrom() non-blocking
 * @param sec: seconds
 * @param usec: microseconds
 */
void Client::set_timeout(int sec, int usec) {
    unsigned long on = 1;
    if(ioctlsocket(client_socket, FIONBIO, &on) == SOCKET_ERROR) {
        EXCPT_LOG(string("ioctlsocket() failed with error code : ") + to_string(WSAGetLastError()));
        exit(EXIT_FAILURE);
    }
    time_out = sec * 1000 + usec / 1000;
    PLAIN_LOG("Timeout set to " + to_string(time_out) + " ms");
}
/*
 * Set local_retry count for resent
 */
void Client::set_retry_count(int count) {
    retry_count = count;
    PLAIN_LOG("Retry count set to " + to_string(retry_count));
}
/*
 * Print final log
 */
void Client::print_log() {
    cout << "=====================" << endl;
    cout << "File " << send_file_name << " sent" << endl;
    cout << "Total bytes sent: " << acc_sent_size << endl;
    cout << "Duration: " << duration << " s" << endl;
    cout << "Bandwidth: " << bandwidth << " KB/s" << endl;
#ifdef DEBUG_CORRUPT
    cout << "Total packets corrupted made: " << acc_corrupt << endl;
#endif
    cout << "Total packets sent: " << acc_sent_packet << endl;
    cout << "Total local_retry count: " << acc_resent_packet << endl;
    cout << "=====================" << endl;
}
/*
 * Initialize client. Finally, the state is set to CLOSED
 */
void Client::init() {
    /*
     * Initialize WinSock
     */
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        EXCPT_LOG(string("WSAStartup failed") + to_string(WSAGetLastError()));
        exit(EXIT_FAILURE);
    }
    /*
     * Create udp socket
     */
    if ((client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR) {
        EXCPT_LOG(string("socket() failed with error code : ") + to_string(WSAGetLastError()));
        exit(EXIT_FAILURE);
    }
    /*
     * Setup remote server params
     */
    server_addr.sin_family = AF_INET;
#ifdef ROUTER
    server_addr.sin_port = htons(ROUTER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(ROUTER_IP_ADDR);
#else
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP_ADDR);
#endif
    server_addr_len = sizeof(server_addr);
    PLAIN_LOG(string("Server params set: ") + string("Addr: ") + to_string(server_addr.sin_addr.s_addr) + string(" Port: ") + to_string(server_addr.sin_port));
    /*
     * Choose a file to send
     */
    if(!choose_send_file(res_dir)){
        EXCPT_LOG(string("No file chosen"));
        exit(EXIT_FAILURE);
    }
    /*
     * Set timeout, local_retry count……
     */
    reset();
}
/*
 * Reset timeout, local_retry count etc.
 */
void Client::reset() {
    time_t t;
    srand((unsigned)time(&t));
    seq = rand();
    ack = 0;
    primary_base_seq = rand();
    acc_sent_packet = 0;
    acc_resent_packet = 0;
    ready_to_close = false;
#ifdef DEBUG_CORRUPT
    acc_corrupt = 0;
#endif
    set_timeout(0, 500 * 1000);
    set_retry_count(3);
    PLAIN_LOG(string("Reset done"));
}
/*
 * State: Any state can call this
 * Event: Do nothing
 */
bool Client::Null_Func() {
    PLAIN_LOG(string("Null function"));
    return true;
}
/*
 * State: Closed
 * Event: First step to establish connection--send syn to server
 */
bool Client::Closed_Send() {
    MSS = SEND_BUF_SIZE;    // Client's MSS
    File_Info* file_info = PacketManager::make_file_info(primary_base_seq, MSS, send_file_size, send_file_name);
    send_data((u8*)file_info, FILE_INFO_LEN(file_info), SYN_TYPE);  // Send syn carrying file info to server
    CIR_ADD1(&seq, uint32_t(DATA_LEN((UDP_Packet*)send_buf)));
    return true;
}
/*
 * State: SynSent
 * Event: Packet received. Check if ack_syn received and return.
 */
bool Client::SynSent_Rcv() {
    recv_packet = (UDP_Packet *) recv_buf;
    PLAIN_LOG(string("RCV_PKT ") + STR(recv_packet));
    if (CORRUPTED(recv_packet)) {   // check if corrupted
        EXCPT_LOG(string("Corrupted packet received"));
        return false;
    }
    if (!ACK(recv_packet) || !SYN(recv_packet)) {   // check if ack_syn
        EXCPT_LOG(string("Not ack_syn packet"));
        return false;
    }
    if (ACK_NUM(recv_packet) != seq) {  // check if ack_num is correct
        EXCPT_LOG(string("Ack_num not correct"));
        return false;
    }
    MSS = *(u16*)recv_packet->data;
    PLAIN_LOG(string("MSS: ") + to_string(MSS));
    ack = CIR_ADD2(SEQ_NUM(recv_packet),DATA_LEN(recv_packet));
    return true;
}
/*
 * State: SYN_SENT
 * Event: Timeout. Resend syn packet which is stored in send_packet (pointing to send_buf)
 */
bool Client::SynSent_Timeout() {
    EXCPT_LOG(string("Timeout. Resend syn packet: ") + STR(send_packet));
    sendto(client_socket, (char*) send_packet, TOTAL_LEN(send_packet), 0, (SOCKADDR*)&server_addr, server_addr_len);
    acc_sent_packet++;
    acc_resent_packet++;
    return true;
}
/*
 * State: Established
 * Event: Send data to server
 */
bool Client::Establish_Send() {
    memset(data_buf, 0, MSS-HEADER_SIZE);   // Clear data_buf
    int read_data_len = READ_DATA(&send_file, data_buf, MSS-HEADER_SIZE);   // Read data from file
    send_data((u8*)data_buf, read_data_len, PSH_TYPE);  // Send data to server
#ifndef DEBUG_CORRUPT
    send_packet_buf.push_back(send_packet); // push packet into send buffer
#endif
    CIR_ADD1(&seq, (uint32_t)read_data_len);
    return true;
}
/*
 * State: Established
 * Event: Packet received. Check if ack and not-corrupted.
 */
bool Client::Establish_Rcv() {
    recv_packet = (UDP_Packet *) recv_buf;
    PLAIN_LOG(string("RCV_PKT ") + STR(recv_packet));
    if (CORRUPTED(recv_packet)) {
        EXCPT_LOG(string("Corrupted packet received: resend packet"));
        return false;
    }
    uint32_t increment = 0;
    for(auto it = send_packet_buf.begin(); it != send_packet_buf.end(); it++) {
        increment += DATA_LEN(*it);
        if(ACK_NUM(recv_packet) == CIR_ADD2(SEQ_NUM(*it),DATA_LEN(*it))) {
            PLAIN_LOG(string("Erased until: ") + to_string(SEQ_NUM(*it)));
            send_packet_buf.erase(send_packet_buf.begin(), it+1); // erase sent packets
            PLAIN_LOG(string("WndFrames: ") + to_string(send_packet_buf.size()) + string(", (base)") + to_string(SEQ_NUM(send_packet_buf.front())) + string("......(next)") + to_string(SEQ_NUM(send_packet_buf.back())));
            acc_sent_size += increment;
            return true;
        }
    }
    return false;
}
/*
 * State: Established
 * Event: Timeout. Resend data packet which is stored in send_packet (pointing to send_buf)
 */
bool Client::Establish_Timeout() {
    PLAIN_LOG(string("Timeout. Resending packet"));
    for (auto &packet : send_packet_buf) {
        memset(send_buf, 0, MSS);
        memcpy(send_buf, packet, TOTAL_LEN(packet));
        send_packet = (UDP_Packet *) send_buf;
        PLAIN_LOG(string("SND_PKT ") + STR(send_packet));
        sendto(client_socket, send_buf, SEND_BUF_SIZE, 0, (SOCKADDR *) &server_addr, server_addr_len);
        acc_sent_packet++;
        acc_resent_packet++;
    }
}
/*
 * State: FIN_WAIT
 * Event: Send fin to server
 */
bool Client::FinWait_Send() {
    char null_data = 0;
    if(!ready_to_close) {
        send_data((u8*)&null_data, 1, FIN_TYPE);
        CIR_ADD1(&seq, 1);
    } else {
        send_data((u8*)&null_data, 1, ACK_TYPE);
    }
    return true;
}
/*
 * State: FIN_WAIT
 * Event: Packet received. If fin_ack and not-corrupted, return true
 */
bool Client::FinWait_Rcv() {
    recv_packet = (UDP_Packet *) recv_buf;
    PLAIN_LOG(string("RCV_PKT ") + STR(recv_packet));
    if (CORRUPTED(recv_packet)) {
        EXCPT_LOG(string("Corrupted packet received: resend packet"));
        return false;
    }
    if (!FIN(recv_packet) || !ACK(recv_packet)) {
        EXCPT_LOG(string("Not fin_ack packet"));
        return false;
    }
    CIR_ADD1(&ack, 1);
    return true;
}
/*
 * State: FIN_WAIT
 * Event: Timeout. Resend fin packet which is stored in send_packet (pointing to send_buf)
 */
bool Client::FinWait_Timeout() {
    EXCPT_LOG(string("Timeout. Resend fin packet: ") + STR(send_packet));
    sendto(client_socket, (char*) send_packet, TOTAL_LEN(send_packet), 0, (SOCKADDR*)&server_addr, server_addr_len);
    PLAIN_LOG(string("SND_PKT ") + STR(send_packet));
    acc_sent_packet++;
    acc_resent_packet++;
    return true;
}
/*
 * Main process of client
 */
void Client::main_process() {
    global_start = TIME_NOW;    // start global timer
    set_state(Client::STATE::CLOSED);   // set state to closed
    STATE_LOG(string("Client state: CLOSED"));
    while (true) {
        switch (get_state()) {
            case CLOSED:
                set_event(Client::EVENT::RDT_SEND); // first step to establish connection
                handle_event();
                local_start = TIME_NOW; // start local timer for possible retry
                set_state(Client::STATE::SYN_SENT); // change state to syn_sent
                STATE_LOG(string("Client state: SYN_SENT"));
                break;
            case SYN_SENT:
                local_retry = 0;
                while (true) {
                    if (TIME_OUT(local_start)) {   // If timeout and chances are not used up, resend syn packet
                        if (local_retry == retry_count) {    // else, chances used up. close and return
                            EXCPT_LOG(string("Retry chances used up"));
                            set_state(Client::STATE::CLOSED);
                            STATE_LOG(string("Client state: CLOSED"));
                            return;
                        }
                        set_event(Client::EVENT::TIMEOUT);
                        handle_event(); // resend syn
                        local_start = TIME_NOW; // restart local timer
                        local_retry++;  // increase local retry times
                    }
                    if (recvfrom(client_socket, recv_buf, RECV_BUF_SIZE, 0, (SOCKADDR *) &server_addr,
                                 &server_addr_len) > 0) {
                        set_event(Client::EVENT::RDT_RCV);
                        if (handle_event()) {   // Parse packet received. If syn_ack, change state to established.
                            set_event(Client::EVENT::RDT_SEND);
                            handle_event(); // send ack_psh and establish connection
                            set_state(Client::STATE::ESTABLISHED);
                            STATE_LOG(string("Client state: ESTABLISHED"));
                            break;
                        }
                    }
                }
                break;
            case ESTABLISHED:
                local_retry = 0;
                while (true) {  // file hasn't been sent completely
                    if (recvfrom(client_socket, recv_buf, RECV_BUF_SIZE, 0, (SOCKADDR *) &server_addr,
                                 &server_addr_len) > 0) {   // if receive a packet, check if ack and not-corrupted
                        set_event(Client::EVENT::RDT_RCV);
                        if (handle_event()) {   // Parse packet received. erase acked packets. Acc sent increased here.
                            local_retry = 0;        // reset local retry times
                            local_start = TIME_NOW; // restart local timer when receiving new ack
                            if(acc_sent_size == send_file_size) { // file sent completely and got last ack from server1
                                PLAIN_LOG(string("File ") + send_file_name + string(" sent completely"));  // file sent completely
                                send_file.close();  // close file
                                duration = TIME_SPAN(global_start, Util::TimeStampType::SECOND);
                                bandwidth = (double)send_file_size / duration / 1024;
                                set_state(Client::STATE::FIN_WAIT);
                                STATE_LOG(string("Client state: FIN_WAIT"));
                                break;
                            }
                        }
                    }
                    // send when wnd is not full and file hasn't been sent completely
                    if (send_packet_buf.size() < WND_FRAME_SIZE && acc_sent_size != send_file_size) {
                        if(send_packet_buf.empty()) { // case that base == nextseq
                            local_start = TIME_NOW; // start local timer
                        }
                        set_event(Client::EVENT::RDT_SEND);
                        handle_event(); // send data packet
                    }
                    // timeout, resend all packets in send buffer
                    if(TIME_OUT(local_start)) {   // If timeout and chances are not used up, resend data packet
                        if (local_retry == retry_count) {    // else, chances used up. close and return
                            EXCPT_LOG(string("Retry chances used up"));
                            set_state(Client::STATE::CLOSED);
                            STATE_LOG(string("Client state: CLOSED"));
                            return;
                        }
                        set_event(Client::EVENT::TIMEOUT);
                        handle_event(); // resend data
                        local_start = TIME_NOW; // restart local timer
                        local_retry++;  // increase local retry times
                    }
                }
                break;
            case FIN_WAIT:
                local_retry = 0;
                set_event(Client::EVENT::RDT_SEND);
                handle_event(); // send fin
                while (true) {
                    if (TIME_OUT(local_start)) { // If timeout and chances are not used up, resend fin packet
                        if (local_retry == retry_count) {    // else, chances used up. close and return
                            EXCPT_LOG(string("Retry chances used up"));
                            set_state(Client::STATE::CLOSED);
                            STATE_LOG(string("Client state: CLOSED"));
                            return;
                        }
                        set_event(Client::EVENT::TIMEOUT);
                        handle_event(); // resend fin
                        local_start = TIME_NOW; // restart local timer
                        local_retry++;  // increase local retry times
                    }
                    if (recvfrom(client_socket, recv_buf, RECV_BUF_SIZE, 0, (SOCKADDR *) &server_addr,
                                 &server_addr_len) > 0) {
                        set_event(Client::EVENT::RDT_RCV);
                        if (handle_event()) {   // Parse packet received. If fin_ack, change state to closed.
                            break;
                        }
                    }
                }
                ready_to_close = true;
                set_event(Client::EVENT::RDT_SEND);
                handle_event(); // send last ack
                set_state(Client::STATE::CLOSED);
                STATE_LOG(string("Client state: CLOSED"));
                print_log();    // print log
                return;
        }
    }
}
/*
 * Send data
 */
void Client::send_data(uint8_t *data, uint16_t data_len, PacketManager::PacketType packet_type) {
    send_packet = MAKE_PKT(packet_type, seq, ack, data, data_len);  // make packet
#ifdef DEBUG_CORRUPT
    if(get_state() == Client::STATE::ESTABLISHED) {
        send_packet_buf.push_back(send_packet);
    }
#endif
    PLAIN_LOG(string("SND_PKT ") + STR(send_packet));    // log
    memset(send_buf, 0, SEND_BUF_SIZE);   // clear send_buf
    memcpy(send_buf, send_packet, TOTAL_LEN(send_packet));  // copy packet to send_buf
#ifdef DEBUG_CORRUPT
    if (acc_sent_packet % CORRUPT_RATE == 0 && get_state() == Client::STATE::ESTABLISHED) {
        time_t t;
        srand((unsigned)time(&t));
        int index = rand() % TOTAL_LEN(send_packet);
        send_buf[index+HEADER_SIZE] = ~send_buf[index+HEADER_SIZE];
        acc_corrupt++;
        PLAIN_LOG(string("Made corrupt packet"));
    }
#endif
    sendto(client_socket, send_buf, SEND_BUF_SIZE, 0, (SOCKADDR *) &server_addr, server_addr_len);  // send packet
    acc_sent_packet++; // increase sent packet count
}

bool Client::handle_event() {
    return (this->*event_handler_table[state][event].fp)();
}

Client::STATE Client::get_state() {
    return this->state;
}

void Client::set_state(Client::STATE state) {
    this->state = state;
}

void Client::set_event(Client::EVENT event) {
    this->event = event;
}
/*
 * Read data from file and send ack_psh packet to server
 */
bool Client::SynSent_Send() {
    seq = primary_base_seq; // reset seq to primary_base_seq
    memset(data_buf, 0, MSS-HEADER_SIZE);   // Clear data_buf
    int read_data_len = READ_DATA(&send_file, data_buf, MSS-HEADER_SIZE);   // Read data from file
    send_data((u8*)data_buf, read_data_len, PSH_ACK_TYPE);  // Send data to server
    send_packet_buf.push_back(send_packet); // push packet into send buffer
    CIR_ADD1(&seq, read_data_len);    // update seq
    return true;
}
