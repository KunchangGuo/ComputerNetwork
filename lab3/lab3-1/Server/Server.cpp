#include "Server.h"
#include <iostream>
#include <chrono>
using namespace std;
using namespace chrono;

/*
 * @param sec: seconds
 * @param usec: microseconds
 */
void Server::set_timeout(int sec, int usec) {
    time_out = sec * 1000 + usec / 1000;
    if(setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out)) == SOCKET_ERROR){
        cout << "setsockopt() failed with error code : " << WSAGetLastError() << endl;
        exit(EXIT_FAILURE);
    }
    cout << "Timeout set to " << time_out << " ms" << endl;
}

void Server::set_retry_count(int count) {
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
bool Server::stop_and_wait_send_and_recv(unsigned char *data, unsigned short data_len, PacketManager::PacketType packet_type) {
    int retry_cnt = 0;
    TIME_POINT start = TIME_NOW;    // give a value to init, not real start of timer

    send_packet = MAKE_PKT(packet_type, seq, ack, data, data_len);
    seq += data_len;    // auto increment seq
    memset(send_buf, 0, MSS);
    memcpy(send_buf, send_packet, TOTAL_LEN(send_packet));
    cout << "send_packet: " << STR(send_packet) << endl;

    while(retry_cnt < retry_count) {
        if (sendto(server_socket, send_buf, SEND_BUF_SIZE, 0, (SOCKADDR *) &client_addr, client_addr_len) == SOCKET_ERROR) {
            cout << "sendto() failed with error code : " << WSAGetLastError() << endl;
            exit(EXIT_FAILURE);
        }

        if(retry_cnt == 0){
            start = TIME_NOW;    // start timer
        }

        memset(recv_buf, 0, MSS);
        if (recvfrom(server_socket, recv_buf, MSS, 0, (SOCKADDR *) &client_addr, &client_addr_len) == SOCKET_ERROR) {
            if (TIME_OUT(start)) {   // timeout
                cout << "recvfrom() timeout: resend packet" << endl;
                retry_cnt++;
                continue;
            }
//            if (WSAGetLastError() == WSAETIMEDOUT) {
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
        if (!ACK(recv_packet) || ACK_NUM(recv_packet) != seq) {
            cout << "Ack expected or ack mismatched" << endl;
            continue;
        }

        ack = SEQ_NUM(recv_packet) + DATA_LEN(recv_packet);    // auto increment ack
        cout << "recv_packet: " << STR(recv_packet) << endl;
        return true;    // success
    }
    return false;   // retry_count times used up
}

void Server::parse_recv_packet() {
    /*
     * Parse SYN packet:
     * 1. Get MSS from SYN packet and compare with recv buf size. Set MSS
     * 2. Get file_name and file_size
     */
    if(SYN(recv_packet)) {
        cout << "Parsing SYN packet" << endl;
        File_Info *file_info = (File_Info *) DATA(recv_packet);
        MSS = file_info->MSS;
        MSS = MSS < RECV_BUF_SIZE ? MSS : RECV_BUF_SIZE;
        cout << "MSS: " << MSS << endl;
        recv_file_name = (char*) file_info->file_name;
        cout << "File name: " << recv_file_name << endl;
        recv_file_size = file_info->file_size;
        cout << "File size: " << recv_file_size << endl;
    }

    /*
     * Parse PSH packet:
     * Get file data and length -> dump
     */
    if(PSH(recv_packet)) {
        cout << "Parsing PSH packet" << endl;
        DUMP_DATA(&dump_file, DATA(recv_packet), DATA_LEN(recv_packet));
        cout << "Downloaded " << DATA_LEN(recv_packet) << " bytes" << endl;
        acc_recv_size += DATA_LEN(recv_packet);
    }

    /*
     * Parse FIN packet:
     * Close file
     */
    if(FIN(recv_packet)) {
        cout << "Parsing FIN packet" << endl;
        state = LAST_ACK;
    }
}

/*
 * Steps to initialize a server:
 * 1. Initialize Winsock environment
 * 2. Create a UDP socket
 * 3. Bind the socket to specific IP address and port
 */
void Server::init() {
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {    // Initialize Winsock
        cout << "Failed. Error Code : " << WSAGetLastError() << endl;
        exit(EXIT_FAILURE);
    }
    cout << "Initialized." << endl;

    if ((server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR) {   // Create UDP socket
        cout << "Could not create socket : " << WSAGetLastError() << endl;
        exit(EXIT_FAILURE);
    }
    cout << "Socket created." << endl;

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");    // set server ip address to localhost (another way to do this is to use inet_addr("127.0.0.1"))
    server_addr.sin_port = htons(PORT);         // set server port to 8888
    client_addr_len = sizeof(client_addr);
    if (bind(server_socket, (SOCKADDR *) &server_addr, sizeof(server_addr)) == SOCKET_ERROR) {   // Bind, which is essential to server
        cout << "Bind failed with error code : " << WSAGetLastError() << endl;
        exit(EXIT_FAILURE);
    }
    cout << "Bind done." << endl;

    state = LISTEN;
}

/*
 *  Main process of server:
 *  Establish connection with client;
 *  Interact with client
 */
void Server::start() {
    seq = ack = 0;
    char null_data = 1;
    while(true) {
        switch (state) {
            /*
             *  Listen state:
             *  Action: Wait for SYN packet from client -> Change state to SYN_RCVD
             *  Note: The ultimate MSS(max_segment_size) is the minimum of server's recv buffer size and client's max send buffer size
             */
            case LISTEN: {
                cout << "LISTEN" << endl;

                memset(recv_buf, 0, RECV_BUF_SIZE);
                if (recvfrom(server_socket, recv_buf, RECV_BUF_SIZE, 0, (SOCKADDR *) &client_addr, &client_addr_len) == SOCKET_ERROR) {
                    cout << "recvfrom() failed with error code : " << WSAGetLastError() << endl;
                    exit(EXIT_FAILURE);
                }
                recv_packet = (UDP_Packet *) recv_buf;
                if (CORRUPTED(recv_packet)) {
                    cout << "Corrupted packet received" << endl;
                    break;
                }
                if(!SYN(recv_packet)) {
                    cout << "SYN packet expected" << endl;
                    break;
                }
                ack = SEQ_NUM(recv_packet) + DATA_LEN(recv_packet);    // auto increment ack

                parse_recv_packet();
                cout << "recv_packet: " << STR(recv_packet) << endl;
                MKDIR(download_dir);
                dump_file = fstream(download_dir + recv_file_name, ios::out | ios::binary);
                if(!dump_file.is_open()){
                    cout << "Failed to open file" << endl;
                    reset();
                    cout << "Reset" << endl;
                    break;
                }
                cout << "File " << recv_file_name << " downloading" << endl;

                state = SYN_RCVD;
                cout << "SYN_RCVD" << endl;
                break;
            }

            /*
             *  SYN_RCVD state:
             *  Action: Send MSS to client -> Wait for ACK packet from client -> Change state to ESTABLISHED
             */
            case SYN_RCVD: {
                set_timeout(1, 0);  // set timeout to 1 sec
                set_retry_count(3);

                if(!stop_and_wait_send_and_recv((unsigned char *) &MSS, sizeof(MSS), PacketManager::SYN_ACK)) {
                    cout << "Failed to send SYN_ACK packet" << endl;
                    exit(EXIT_FAILURE);
                }
                parse_recv_packet();

                state = ESTABLISHED;
                cout << "ESTABLISHED" << endl;
                break;
            }

            /*
             *  ESTABLISHED state:
             *  Action: Return and start transaction
             */
            case ESTABLISHED: {
                while(acc_recv_size <= recv_file_size && state != LAST_ACK) {
                    if(!stop_and_wait_send_and_recv((u8*)&null_data, 1, PacketManager::ACK)) {
                        cout << "Failed to send ACK packet" << endl;
                        exit(EXIT_FAILURE);
                    }
                    parse_recv_packet();
                }
                cout << "acc_recv_size: " << acc_recv_size << endl;
                cout << "File " << recv_file_name << " downloaded" << endl;
                dump_file.close();

                state = LAST_ACK;
                cout << "LAST_ACK" << endl;
                break;
            }

            /*
             *  LAST_ACK state:
             *  Action: Send FIN packet to client -> Wait for ACK packet from client -> Change state to CLOSED
             */
            case LAST_ACK: {
                if(!stop_and_wait_send_and_recv((u8*)&null_data, 1, FIN_ACK_TYPE)) {
                    cout << "Failed to send FIN_ACK packet" << endl;
                    exit(EXIT_FAILURE);
                }

                state = CLOSED;
                cout << "CLOSED" << endl;
                return;
            }
        }
    }
}

void Server::reset() {
    dump_file.close();
    state = LISTEN;
    ack = seq = 0;
}
