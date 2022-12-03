#include "Client.h"
#include <iostream>
#include <chrono>
using namespace std;
using namespace chrono;

/*
 * Open directory -> select a file to send -> get file size
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
    cout << "File chosen: " << send_file_name << endl;
    cout << "File size: " << send_file_size << endl;
    return true;
}

/*
 * @param sec: seconds
 * @param usec: microseconds
 */
void Client::set_timeout(int sec, int usec) {
    time_out = sec * 1000 + usec / 1000;
    if(setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out)) == SOCKET_ERROR){
        EXCPT_LOG(string("setsockopt() failed with error code : ") + to_string(WSAGetLastError()));
        exit(EXIT_FAILURE);
    }
    PLAIN_LOG("Timeout set to " + to_string(time_out) + " ms");
}

void Client::set_retry_count(int count) {
    retry_count = count;
    PLAIN_LOG("Retry count set to " + to_string(retry_count));
}

/*
 * Main function of transaction: send data and deal with recv packet
 * @param data: data to be sent
 * @param data_len: length of data
 * @param flags: type of packet
 * @return: true for success and false for failure
 *
 * Actions:
 * 1. Clear send buffer, make send packet and send it
 * 2. Wait for ack packet
 * 3. Check if ack packet is corrupted, ack_mismatched or timeout
 * 4. If timeout, resend packet (at most retry_count times)
 */
bool Client::stop_and_wait_send_and_recv(unsigned char *data, unsigned short data_len, PacketManager::PacketType packet_type) {
#define DEFAULT_TIMEOUT
    int retry_cnt = 0;
    TIME_POINT start = TIME_NOW;    // give a value to init, not real start of timer

    send_packet = MAKE_PKT(packet_type, seq, ack, data, data_len);
#ifdef DEBUG_CORRUPT
    u16 checksum_backup = send_packet->header.checksum;
#endif
    seq += data_len;    // auto increment seq
    memset(send_buf, 0, MSS);
    memcpy(send_buf, send_packet, TOTAL_LEN(send_packet));
    PLAIN_LOG(string("SND_PKT ") + STR(send_packet));

    while(retry_cnt < retry_count) {
#ifdef DEBUG_CORRUPT
        if (corrupt_count == CORRUPT_RATE - 1) {
            send_packet = (UDP_Packet*)send_buf;
            send_packet->header.checksum = rand() % 256;
            acc_corrupt++;
            PLAIN_LOG(string("Made corrupted packet"));
        }
        if(corrupt_count == 0) {
            send_packet->header.checksum = checksum_backup;
        }
        corrupt_count = ++corrupt_count % CORRUPT_RATE;
#endif
        acc_sent++;
        if (sendto(client_socket, send_buf, SEND_BUF_SIZE, 0, (SOCKADDR *) &server_addr, server_addr_len) == SOCKET_ERROR) {
            EXCPT_LOG(string("sendto() failed with error code : ") + to_string(WSAGetLastError()));
            exit(EXIT_FAILURE);
        }

#ifndef DEFAULT_TIMEOUT
        if(retry_cnt == 0){
            start = TIME_NOW;    // start timer
        }
#endif

        memset(recv_buf, 0, MSS);
        while(true)
        {
            if (recvfrom(client_socket, recv_buf, MSS, 0, (SOCKADDR *) &server_addr, &server_addr_len) == SOCKET_ERROR) {
#ifndef DEFAULT_TIMEOUT
                if (TIME_OUT(start)) {   // timeout
                    EXCPT_LOG(string("recvfrom() timeout: resend packet"));
                    retry_cnt++;
                    acc_resent++;
                    break;
                }
#else
                if (WSAGetLastError() == WSAETIMEDOUT) {  // reserved to be tested
                    EXCPT_LOG(string("recvfrom() timeout: resend packet"));
                    retry_cnt++;
                    acc_resent++;
                    break;
                }
#endif
                else {
                    EXCPT_LOG(string("recvfrom() failed with error code : ") + to_string(WSAGetLastError()));
                    exit(EXIT_FAILURE);
                }
            }

            recv_packet = (UDP_Packet *) recv_buf;
            if (CORRUPTED(recv_packet)) {
                if(!TIME_OUT(start)) continue;
                EXCPT_LOG(string("Corrupted packet received: resend packet"));
                continue;
            }
            if (!ACK(recv_packet)) {
                if (!TIME_OUT(start)) continue;
                EXCPT_LOG(string("Ack expected"));
                continue;
            }
            if(ACK_NUM(recv_packet) != seq) {
                if (!TIME_OUT(start)) continue;
                EXCPT_LOG(string("Ack mismatched"));
                continue;
            }
            ack = SEQ_NUM(recv_packet) + DATA_LEN(recv_packet);    // auto increment ack
//            ack += DATA_LEN(recv_packet);
            PLAIN_LOG(string("RCV_ACK ") + STR(recv_packet));
            return true;    // success
        }
    }
    return false;   // retry_count times used up
}

/*
 * Parse packet according to its type
 */
void Client::parse_recv_packet() {
    /*
     * Parse SYN packet:
     * get MSS
     */
    if(SYN(recv_packet)){
        MSS = *(u16*)recv_packet->data;
        PLAIN_LOG(string("Parsing SYN packet"));
        PLAIN_LOG(string("MSS set to ") + to_string(MSS));
    }

    /*
     * Parse FIN packet:
     * close resources
     */
    if(FIN(recv_packet)){
        PLAIN_LOG(string("Parsing FIN packet"));
    }
}

void Client::print_log() {
    cout << "=====================" << endl;
    cout << "File " << send_file_name << " sent" << endl;
    cout << "Total bytes sent: " << acc_sent_size << endl;
    cout << "Duration: " << duration << " s" << endl;
    cout << "Bandwidth: " << bandwidth << " KB/s" << endl;
#ifdef DEBUG_CORRUPT
    cout << "Total packets corrupted made: " << acc_corrupt << endl;
#endif
    cout << "Total Packets sent: " << acc_sent << endl;
    cout << "Total Packets resent: " << acc_resent << endl;
    cout << "=====================" << endl;
}

/*
 * Init Winsock -> Create socket -> Setup remote params -> Set timeout & retry count
 */
void Client::init() {
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {    // initialize winsock
        EXCPT_LOG(string("WSAStartup failed") + to_string(WSAGetLastError()));
        exit(EXIT_FAILURE);
    }

    if ((client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR) {       // create udp socket
        EXCPT_LOG(string("socket() failed with error code : ") + to_string(WSAGetLastError()));
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;       // create remote server type, address and port
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP_ADDR);
    server_addr_len = sizeof(server_addr);
    PLAIN_LOG(string("Server params set"));

//    client_addr.sin_family = AF_INET;       // create local client type, address and port
//    client_addr.sin_port = htons(CLIENT_PORT);
//    client_addr.sin_addr.s_addr = inet_addr(CLIENT_IP_ADDR);
//    client_addr_len = sizeof(client_addr);
//    if (bind(client_socket, (SOCKADDR*)&client_addr, client_addr_len) == SOCKET_ERROR) {   // bind local client address and port
//        cout << "bind() failed" << endl;
//        exit(EXIT_FAILURE);
//    }
//    cout << "Bind done" << endl;

    reset();    // reset state, seq, ack, retry_cnt, timeout
}

/*
 * Main process of client:
 * Choose a file to send
 * Establish connection with server;
 * Interact with server
 */
void Client::start() {
    if(!choose_send_file(res_dir)){
        EXCPT_LOG(string("No file chosen"));
        exit(EXIT_FAILURE);
    }

    TIME_POINT global_start = TIME_NOW;    // start timer for global transition duration

    seq = ack = 0;
    unsigned read_data_len = 0;
    char null_data = 1;
    while(true){
        switch(state) {
            /*
             * Closed state:
             * Action: Send SYN packet to server and wait for ack -> change state to SYN_SENT
             */
            case(CLOSED):{
                STATE_LOG(string("Client state: CLOSED"));

                MSS = SEND_BUF_SIZE;    // mss is not set here, just a temporal instance of send_buf_size
                File_Info* file_info = PacketManager::make_file_info(MSS, send_file_size, send_file_name);
                if(!stop_and_wait_send_and_recv((u8*)file_info, FILE_INFO_LEN(file_info), SYN_TYPE)){
                    EXCPT_LOG(string("Failed to send SYN packet"));
                    exit(EXIT_FAILURE);
                }

                if(!SYN(recv_packet)){  // SYN packet is not received
                    EXCPT_LOG(string("SYN expected"));
                    reset();
                    PLAIN_LOG(string("Reset"));
                    break;
                }
                parse_recv_packet();

                state = SYN_SENT;
                STATE_LOG(string("Client state: SYN_SENT"));

                break;
            }

            /*
             * SYN_SENT state:
             * Action: send ACK packet to server and wait for ack -> change state to ESTABLISHED
             */
            case(SYN_SENT):{
                memset(data_buf, 0, MSS-HEADER_SIZE);
                read_data_len = READ_DATA(&send_file, data_buf, MSS-HEADER_SIZE);
                if(!stop_and_wait_send_and_recv((unsigned char*)data_buf, read_data_len, PSH_ACK_TYPE)){
                    EXCPT_LOG(string("Failed to send ACK_PSH packet"));
                    exit(EXIT_FAILURE);
                }
                acc_sent_size += DATA_LEN(send_packet);
                PLAIN_LOG(string("Sent ") + to_string(DATA_LEN(send_packet)) + string(" bytes"));

                state = ESTABLISHED;
                STATE_LOG(string("Client state: ESTABLISHED"));

                break;
            }

            /*
             * Established state:
             * return to continue transaction with server
             */
            case ESTABLISHED:{
                while(acc_sent_size < send_file_size){
                    memset(data_buf, 0, MSS-HEADER_SIZE);
                    read_data_len = READ_DATA(&send_file, data_buf, MSS-HEADER_SIZE);
                    if(!stop_and_wait_send_and_recv((unsigned char*)data_buf, read_data_len, PSH_ACK_TYPE)){
                        EXCPT_LOG(string("Failed to send ACK_PSH packet"));
                        exit(EXIT_FAILURE);
                    }
                    acc_sent_size += DATA_LEN(send_packet);
                    PLAIN_LOG(string("Sent ") + to_string(DATA_LEN(send_packet)) + string(" bytes"));
                }
                send_file.close();
                PLAIN_LOG(string("File ") + send_file_name + string(" sent"));

                state = FIN_WAIT_1;
                STATE_LOG(string("Client state: FIN_WAIT_1"));
                break;
            }

            /*
             * FIN_WAIT_1 state:
             * Action: send FIN packet to server and wait for ack -> change state to FIN_WAIT_2
             */
            case FIN_WAIT_1:{
                if(!stop_and_wait_send_and_recv((u8*)&null_data, sizeof(null_data), FIN_ACK_TYPE)){
                    EXCPT_LOG(string("Failed to send FIN_ACK packet"));
                    exit(EXIT_FAILURE);
                }
                parse_recv_packet();

                state = FIN_WAIT_2;
                STATE_LOG(string("Client state: FIN_WAIT_2"));
                break;
            }

            /*
             * FIN_WAIT_2 state:
             * Action: send ACK packet to server -> close
             */
            case FIN_WAIT_2:{
                send_packet = MAKE_PKT(ACK_TYPE, seq, ack, (u8*)&null_data, sizeof(null_data));
                sendto(client_socket, (char*)send_packet, TOTAL_LEN(send_packet), 0, (SOCKADDR*)&server_addr, server_addr_len);
                Sleep(1000);    // sendto is non-blocking, sleep for 1s to ensure packet is sent
                PLAIN_LOG(string("SND_PKT") + STR(send_packet));

                state = CLOSED;
                STATE_LOG(string("Client state: CLOSED"));
                duration = TIME_SPAN(global_start, Util::TimeStampType::SECOND);   // second
                bandwidth = (double)send_file_size / duration / 1024;  // KB/s
                print_log();
                return;
            }
        }
    }
}

void Client::reset() {
    state = CLOSED;
    seq = ack = 0;
    acc_sent = 0;
    acc_resent = 0;
    set_timeout(0, 200 * 1000);
    set_retry_count(5);
    PLAIN_LOG(string("Reset"));
}