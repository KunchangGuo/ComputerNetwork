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
        cout << "Failed to get files" << endl;
        return false;
    }
    int i = 0;
    for(const auto& file: files){
        cout << ++i << " " << file << endl;
    }
    cout << "Choose a file to send: ";
    cin >> i;
    if(i < 1 || i > files.size()){
        cout << "Invalid file index" << endl;
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
        cout << "setsockopt() failed with error code : " << WSAGetLastError() << endl;
        exit(EXIT_FAILURE);
    }
    cout << "Timeout set to " << time_out << " ms" << endl;
}

void Client::set_retry_count(int count) {
    retry_count = count;
    cout << "Retry count set to " << retry_count << endl;
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
    int retry_cnt = 0;
    int recv_len = 0;
    TIME_POINT start = TIME_NOW;    // give a value to init, not real start of timer

    send_packet = MAKE_PKT(packet_type, seq, ack, data, data_len);
    seq += data_len;    // auto increment seq
    memset(send_buf, 0, MSS);
    memcpy(send_buf, send_packet, TOTAL_LEN(send_packet));
    cout << "send_packet: " << STR(send_packet) << endl;

    while(retry_cnt < retry_count) {
        if (sendto(client_socket, send_buf, SEND_BUF_SIZE, 0, (SOCKADDR *) &server_addr, server_addr_len) == SOCKET_ERROR) {
            cout << "sendto() failed with error code : " << WSAGetLastError() << endl;
            exit(EXIT_FAILURE);
        }

        if(retry_cnt == 0){
            start = TIME_NOW;    // start timer
        }

        memset(recv_buf, 0, MSS);
        if (recvfrom(client_socket, recv_buf, MSS, 0, (SOCKADDR *) &server_addr, &server_addr_len) == SOCKET_ERROR) {
            if (TIME_OUT(start)) {   // timeout
                cout << "recvfrom() timeout: resend packet" << endl;
                retry_cnt++;
                continue;
            }
//            if (WSAGetLastError() == WSAETIMEDOUT) {  // reserved to be tested
//                cout << "recvfrom() timed out" << endl;
//                retry_cnt++;
//                continue;
//            }
            else {
                cout << "recvfrom() failed with error code : " << WSAGetLastError() << endl;
                exit(EXIT_FAILURE);
            }
        }

        recv_packet = (UDP_Packet *) recv_buf;
        if (CORRUPTED(recv_packet)) {
            cout << "Corrupted packet received" << endl;
            continue;
        }
        if (!ACK(recv_packet)) {
            cout << "Ack expected" << endl;
            continue;
        }
        if(ACK_NUM(recv_packet) != seq){
            cout << "Ack mismatched" << endl;
            continue;
        }

        ack = SEQ_NUM(recv_packet) + DATA_LEN(recv_packet);    // auto increment ack
        cout << "recv_packet: " << STR(recv_packet) << endl;
        return true;    // success
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
        cout << "Parsing SYN packet" << endl;
        MSS = *(u16*)recv_packet->data;
        cout << "MSS: " << MSS << endl;
    }

    /*
     * Parse FIN packet:
     * close resources
     */
    if(FIN(recv_packet)){
        cout << "Parsing FIN packet" << endl;
        cout << "FIN packet received" << endl;
    }
}

/*
 * Init Winsock -> Create socket -> Setup remote params -> Set timeout & retry count
 */
void Client::init() {
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {    // initialize winsock
        cout << "WSAStartup failed" << endl;
        exit(EXIT_FAILURE);
    }

    if ((client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR) {       // create udp socket
        cout << "socket() failed" << endl;
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;       // create remote server type, address and port
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP_ADDR);
    server_addr_len = sizeof(server_addr);
    cout << "Server params set" << endl;

    client_addr.sin_family = AF_INET;       // create local client type, address and port
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = inet_addr(CLIENT_IP_ADDR);
    client_addr_len = sizeof(client_addr);
    if (bind(client_socket, (SOCKADDR*)&client_addr, client_addr_len) == SOCKET_ERROR) {   // bind local client address and port
        cout << "bind() failed" << endl;
        exit(EXIT_FAILURE);
    }
    cout << "Bind done" << endl;

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
        cout << "No file chosen" << endl;
        exit(EXIT_FAILURE);
    }

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
                cout << "CLOSED" << endl;

                MSS = SEND_BUF_SIZE;    // mss is not set here, just a temporal instance of send_buf_size
                File_Info* file_info = PacketManager::make_file_info(MSS, send_file_size, send_file_name);
                if(!stop_and_wait_send_and_recv((u8*)file_info, FILE_INFO_LEN(file_info), SYN_TYPE)){
                    cout << "Failed to send SYN packet" << endl;
                    exit(EXIT_FAILURE);
                }

                if(!SYN(recv_packet)){  // SYN packet is not received
                    cout << "SYN expected" << endl;
                    reset();
                    cout << "Reset" <<endl;
                    break;
                }
                parse_recv_packet();

                state = SYN_SENT;
                cout << "SYN_SENT" << endl;

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
                    cout << "Failed to send ACK_PSH packet" << endl;
                    exit(EXIT_FAILURE);
                }
                acc_sent_size += DATA_LEN(send_packet);
                cout << "Sent " << DATA_LEN(send_packet) << " bytes" << endl;

                state = ESTABLISHED;
                cout << "ESTABLISHED" << endl;

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
                        cout << "Failed to send ACK_PSH packet" << endl;
                        exit(EXIT_FAILURE);
                    }
                    acc_sent_size += DATA_LEN(send_packet);
                    cout << "Sent " << read_data_len << " bytes" << endl;
                }
                send_file.close();
                cout << "File " << send_file_name << " sent" << endl;
                cout << "acc_sent_size: " << acc_sent_size << endl;

                state = FIN_WAIT_1;
                cout << "FIN_WAIT_1" << endl;
                break;
            }

            /*
             * FIN_WAIT_1 state:
             * Action: send FIN packet to server and wait for ack -> change state to FIN_WAIT_2
             */
            case FIN_WAIT_1:{
                if(!stop_and_wait_send_and_recv((u8*)&null_data, sizeof(null_data), FIN_ACK_TYPE)){
                    cout << "Failed to send FIN_ACK packet" << endl;
                    exit(EXIT_FAILURE);
                }
                parse_recv_packet();

                state = FIN_WAIT_2;
                cout << "FIN_WAIT_2" << endl;
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
                cout << "send_packet: " << STR(send_packet) << endl;

                cout << "CLOSED" << endl;
                return;
            }
        }
    }
}

void Client::reset() {
    state = CLOSED;
    seq = ack = 0;
    set_timeout(1, 0);
    set_retry_count(3);
    cout << "Reset done" << endl;
}