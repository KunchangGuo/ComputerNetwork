#ifndef CLIENT_PACKETMANAGER_H
#define CLIENT_PACKETMANAGER_H

#include <string>
using namespace std;

#define u32 unsigned int
#define u16 unsigned short
#define u8 unsigned char
#define HEADER_SIZE 14
#define DATA_MAX_SIZE (65535-HEADER_SIZE)    // 65535 is the maximum size of a UDP packet, 16 is the size of the header, a byte for 0 is reserved for the end of the data
#define FILE_NAME_MAX_SIZE 256

#pragma pack(1) // align to 1 byte
struct UDP_Header {
    u32 seq_num;                // 4B, sequence number
    u32 ack_num;                // 4B, acknowledgement number
    u16 length;                 // 2B, length of data segment
    u16 checksum;               // 2B, checksum of the packet
    u8 flags;                   // 1B, flags of the packet, one hot encoded, 0x01 for SYN, 0x02 for ACK, 0x04 for FIN, 0x08 for RST
    u8 filter;                  // 1B, filler to make the header align to 16 bits
};
struct UDP_Packet{
    UDP_Header header;
    u8 data[DATA_MAX_SIZE];   // data of the packet
};
struct File_Info {  // this struct is used to send file info to the server
    u32 primary_seq_num;        // primary sequence number of GBN
    u16 MSS;                    // maximum segment size
    u32 file_size;
    u16 file_name_len;
    u8 file_name[FILE_NAME_MAX_SIZE];
};
#pragma pack()  // align to default


class PacketManager {
public:
#define TOTAL_LEN(packet) PacketManager::get_total_length(packet)
#define DATA_LEN(packet) PacketManager::get_data_length(packet)
#define FILE_INFO_LEN(file_info) (sizeof(u32) + sizeof(u16) + sizeof(u32) + sizeof(u16) + file_info->file_name_len)
#define CORRUPTED(packet) PacketManager::is_corrupted(packet)
#define SYN(packet) PacketManager::is_syn(packet)
#define ACK(packet) PacketManager::is_ack(packet)
#define FIN(packet) PacketManager::is_fin(packet)
#define PSH(packet) PacketManager::is_psh(packet)
#define SYN_TYPE PacketManager::PacketType::SYN
#define ACK_TYPE PacketManager::PacketType::ACK
#define FIN_TYPE PacketManager::PacketType::FIN
#define PSH_TYPE PacketManager::PacketType::PSH
#define SYN_ACK_TYPE PacketManager::PacketType::SYN_ACK
#define FIN_ACK_TYPE PacketManager::PacketType::FIN_ACK
#define PSH_ACK_TYPE PacketManager::PacketType::PSH_ACK
#define ACK_NUM(packet) PacketManager::get_ack_num(packet)
#define SEQ_NUM(packet) PacketManager::get_seq_num(packet)
#define DATA(packet) PacketManager::get_data(packet)
#define STR(packet) PacketManager::to_str(packet)
#define MAKE_PKT(type, seq_num, ack_num, data, data_len) PacketManager::make_packet(type, seq_num, ack_num, data, data_len)
#define MAKE_FILE_INFO(primary_seq_num, MSS, file_size, file_name) PacketManager::make_file_info(primary_seq_num, MSS, file_size, file_name)

    enum PacketType {
        SYN,
        ACK,
        FIN,
        PSH,
        SYN_ACK,
        FIN_ACK,
        PSH_ACK,
    };

    static u16 calculate_checksum(UDP_Packet* packet);    // calculate checksum of the packet
    static bool is_corrupted(UDP_Packet* packet); // check if the packet is corrupted
    static bool is_ack(UDP_Packet* packet);    // check if the packet is an ACK packet
    static bool is_syn(UDP_Packet* packet);    // check if the packet is a SYN packet
    static bool is_fin(UDP_Packet* packet);    // check if the packet is a FIN packet
    static bool is_psh(UDP_Packet* packet);    // check if the packet is a PSH packet
    static void set_ack(UDP_Packet* packet);   // set the ACK flag of the packet
    static void set_syn(UDP_Packet* packet);   // set the SYN flag of the packet
    static void set_fin(UDP_Packet* packet);   // set the FIN flag of the packet
    static void set_psh(UDP_Packet* packet);   // set the PSH flag of the packet
    static void set_data(UDP_Packet* packet, u8* data, u16 data_length);  // set the data flag of the packet
    static void set_seq_num(UDP_Packet* packet, u32 seq_num);  // set the seq_num of the packet
    static void set_ack_num(UDP_Packet* packet, u32 ack_num);  // set the ack_num of the packet
    static void set_length(UDP_Packet* packet, u16 length);    // set the length of the packet
    static void set_checksum(UDP_Packet* packet, u16 checksum);    // set the checksum of the packet
    static u8* get_data(UDP_Packet* packet);   // get the data of the packet
    static u16 get_data_length(UDP_Packet* packet); // get the length of the data of the packet
    static u16 get_total_length(UDP_Packet* packet);    // get the total length of the packet
    static u32 get_seq_num(UDP_Packet* packet);    // get the seq_num of the packet
    static u32 get_ack_num(UDP_Packet* packet);    // get the ack_num of the packet

    static string to_str(UDP_Packet* packet);
    static UDP_Packet* make_packet(PacketType type, u32 seq_num, u32 ack_num, u8* data, u16 data_length);  // make a packet
    static File_Info* make_file_info(u32 primary_seq_num, u16 MSS, u32 file_size, const string& file_name);  // make a file info packet
};


#endif //CLIENT_PACKETMANAGER_H
