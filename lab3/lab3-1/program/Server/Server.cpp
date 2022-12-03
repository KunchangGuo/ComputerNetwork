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
    time_out = sec * 1000 + usec / 1000;
    if(setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out)) == SOCKET_ERROR){
        EXCPT_LOG(string("setsockopt() failed with error: ") + to_string(WSAGetLastError()));
        exit(EXIT_FAILURE);
    }
    PLAIN_LOG(string("Timeout set to ") + to_string(time_out) + " ms");
}

void Server::set_retry_count(int count) {
    retry_count = count;
    PLAIN_LOG(string("Retry count set to ") + to_string(retry_count));
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
//#define DEFAULT_TIMEOUT
    int retry_cnt = 0;
    TIME_POINT start = TIME_NOW;    // give a value to init, not real start of timer

    send_packet = MAKE_PKT(packet_type, seq, ack, data, data_len);
    seq += data_len;    // auto increment seq
    memset(send_buf, 0, MSS);
    memcpy(send_buf, send_packet, TOTAL_LEN(send_packet));
    PLAIN_LOG(string("SND_PKT: ") + STR(send_packet));

    while(retry_cnt < retry_count) {
        if (sendto(server_socket, send_buf, SEND_BUF_SIZE, 0, (SOCKADDR *) &client_addr, client_addr_len) == SOCKET_ERROR) {
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
            if (recvfrom(server_socket, recv_buf, MSS, 0, (SOCKADDR *) &client_addr, &client_addr_len) == SOCKET_ERROR) {
#ifndef DEFAULT_TIMEOUT
                if (TIME_OUT(start)) {   // timeout
                    EXCPT_LOG(string("recvfrom() timeout: resend packet"));
                    retry_cnt++;
                    break;
                }
#else
                if (WSAGetLastError() == WSAETIMEDOUT) {
                    EXCPT_LOG(string("recvfrom() timeout: resend packet"));
                    retry_cnt++;
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
                acc_corrupt++;
                EXCPT_LOG(string("Corrupted packet received"));
                continue;
            }
            if (!ACK(recv_packet)) {
                if (!TIME_OUT(start)) continue;
                EXCPT_LOG(string("Ack expected"));
                continue;
            }
            if (ACK_NUM(recv_packet) != seq) {
                if (!TIME_OUT(start)) continue;
                EXCPT_LOG(string("Ack mismatched"));
                continue;
            }
            ack = SEQ_NUM(recv_packet) + DATA_LEN(recv_packet);    // auto increment ack
//        ack += DATA_LEN(recv_packet);    // auto increment ack
            PLAIN_LOG(string("RCV_PKT: ") + STR(recv_packet));
            return true;    // success
        }
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
        File_Info *file_info = (File_Info *) DATA(recv_packet);
        MSS = file_info->MSS;
        MSS = MSS < RECV_BUF_SIZE ? MSS : RECV_BUF_SIZE;
        recv_file_name = (char*) file_info->file_name;
        recv_file_size = file_info->file_size;
        PLAIN_LOG(string("Parsing SYN packet"));
        PLAIN_LOG(string("MSS: ") + to_string(MSS));
        PLAIN_LOG(string("File name: ") + recv_file_name);
        PLAIN_LOG(string("File size: ") + to_string(recv_file_size));
    }

    /*
     * Parse PSH packet:
     * Get file data and length -> dump
     */
    if(PSH(recv_packet)) {
        DUMP_DATA(&dump_file, DATA(recv_packet), DATA_LEN(recv_packet));
        acc_recv_size += DATA_LEN(recv_packet);
        PLAIN_LOG(string("Parsing PSH packet"));
        PLAIN_LOG(string("Downloaded ") + to_string(DATA_LEN(recv_packet)) + " bytes");
    }

    /*
     * Parse FIN packet:
     * Close file
     */
    if(FIN(recv_packet)) {
        state = LAST_ACK;
        PLAIN_LOG(string("Parsing FIN packet"));
    }
}

void Server::print_log() {
    cout << "=====================" << endl;
    cout << "File " << recv_file_name << " downloaded" << endl;
    cout << "Total bytes received: " << acc_recv_size << endl;
    cout << "Duration: " << duration << " s" << endl;
    cout << "Bandwidth: " << bandwidth << " KB/s" << endl;
    cout << "Total packets corrupted: " << acc_corrupt << endl;
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

    state = LISTEN;
    STATE_LOG(string("Server state: LISTEN"));
}

/*
 *  Main process of server:
 *  Establish connection with client;
 *  Interact with client
 */
void Server::start() {
    seq = ack = 0;
    acc_corrupt = 0;
    char null_data = 1;

    TIME_POINT start;

    while(true) {
        switch (state) {
            /*
             *  Listen state:
             *  Action: Wait for SYN packet from client -> Change state to SYN_RCVD
             *  Note: The ultimate MSS(max_segment_size) is the minimum of server's recv buffer size and client's max send buffer size
             */
            case LISTEN: {
                memset(recv_buf, 0, RECV_BUF_SIZE);
                if (recvfrom(server_socket, recv_buf, RECV_BUF_SIZE, 0, (SOCKADDR *) &client_addr, &client_addr_len) == SOCKET_ERROR) {
                    EXCPT_LOG(string("recvfrom() failed with error code : ") + to_string(WSAGetLastError()));
                    exit(EXIT_FAILURE);
                }
                recv_packet = (UDP_Packet *) recv_buf;
                if (CORRUPTED(recv_packet)) {
                    acc_corrupt++;
                    EXCPT_LOG(string("Packet corrupted"));
                    break;
                }
                if(!SYN(recv_packet)) {
                    EXCPT_LOG(string("Expecting SYN packet"));
                    break;
                }
                ack = SEQ_NUM(recv_packet) + DATA_LEN(recv_packet);    // auto increment ack

                parse_recv_packet();
                PLAIN_LOG(string("Received packet: ") + STR(recv_packet));

                MKDIR(download_dir);
                dump_file = fstream(download_dir + recv_file_name, ios::out | ios::binary);
                if(!dump_file.is_open()){
                    reset();
                    EXCPT_LOG(string("Failed to open file"));
                    PLAIN_LOG(string("Reset"));
                    break;
                }
                PLAIN_LOG(string("Downloading ") + recv_file_name + " to " + download_dir);

                state = SYN_RCVD;
                STATE_LOG(string("Server state: SYN_RCVD"));
                break;
            }

            /*
             *  SYN_RCVD state:
             *  Action: Send MSS to client -> Wait for ACK packet from client -> Change state to ESTABLISHED
             */
            case SYN_RCVD: {
                set_timeout(2, 0);  // set timeout to 2 sec
                set_retry_count(5);
                start = TIME_NOW;
                PLAIN_LOG(string("Global timer started"));

                if(!stop_and_wait_send_and_recv((unsigned char *) &MSS, sizeof(MSS), PacketManager::SYN_ACK)) {
                    EXCPT_LOG(string("Failed to send SYN_ACK packet"));
                    exit(EXIT_FAILURE);
                }
                parse_recv_packet();

                state = ESTABLISHED;
                STATE_LOG(string("Server state: ESTABLISHED"));
                break;
            }

            /*
             *  ESTABLISHED state:
             *  Action: Return and start transaction
             */
            case ESTABLISHED: {
                while(acc_recv_size <= recv_file_size && state != LAST_ACK) {
                    if(!stop_and_wait_send_and_recv((u8*)&null_data, 1, PacketManager::ACK)) {
                        EXCPT_LOG(string("Failed to send ACK packet"));
                        exit(EXIT_FAILURE);
                    }
                    parse_recv_packet();
                }

                dump_file.close();
                duration = TIME_SPAN(start, Util::TimeStampType::SECOND);   // second
                bandwidth = (double)acc_recv_size / duration / 1024;    // KB/s
                PLAIN_LOG(string("Download completed"));

                state = LAST_ACK;
                STATE_LOG(string("Server state: LAST_ACK"));
                break;
            }

            /*
             *  LAST_ACK state:
             *  Action: Send FIN packet to client -> Wait for ACK packet from client -> Change state to CLOSED
             */
            case LAST_ACK: {
                if(!stop_and_wait_send_and_recv((u8*)&null_data, 1, FIN_ACK_TYPE)) {
                    EXCPT_LOG(string("Failed to send FIN_ACK packet"));
                    exit(EXIT_FAILURE);
                }

                state = CLOSED;
                STATE_LOG(string("Server state: CLOSED"));
                print_log();    // print global info
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
