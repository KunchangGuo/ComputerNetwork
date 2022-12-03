#include "PacketManager.h"
#include <iostream>
#include <cstring>
using namespace std;

/*
 * Steps to calculate checksum:
 * 1. Backup the checksum field
 * 2. Set the checksum field to 0
 * 3. Calculate the checksum by adding all the 4bytes in the packet and reverse the result
 * 4. Restore the checksum field
 */
u16 PacketManager::calculate_checksum(UDP_Packet *packet) {
    u32 checksum = 0;
    u16* ptr = (u16*) packet;
    u16 total_length = TOTAL_LEN(packet);
    u16 checksum_backup = packet->header.checksum;  // backup the checksum field
    packet->header.checksum = 0;    // set the checksum field to 0
    while(total_length > 1) {
        checksum += *ptr++;
        total_length -= 2;
    }
    if(total_length > 0) {   // if the length is odd, add the last byte
        checksum += *(u8*)ptr;
    }
    checksum = (checksum >> 16) + (checksum & 0xffff);  // add high 16 to low 16
    checksum += (checksum >> 16);
    packet->header.checksum = checksum_backup;  // restore the checksum field
    return (u16) (~checksum);
}

/*
 * Calculate the checksum without backup
 * Only if the result equals to 0( ~(checksum + ~checksum) ), the packet is not corrupted
 */
bool PacketManager::is_corrupted(UDP_Packet *packet) {
    u32 checksum = 0;
    u16* ptr = (u16*) packet;
    u16 total_length = TOTAL_LEN(packet);
    while(total_length > 1) {
        checksum += *ptr++;
        total_length -= 2;
    }
    if(total_length > 0) {   // if the length is odd, add the last byte
        checksum += *(u8*)ptr;
    }
    checksum = (checksum >> 16) + (checksum & 0xffff);  // add high 16 to low 16
    checksum += (checksum >> 16);
    return (u16)(checksum) != 0xffff;
}

bool PacketManager::is_ack(UDP_Packet *packet) {
    return packet->header.flags & 0x02;
}

bool PacketManager::is_syn(UDP_Packet *packet) {
    return packet->header.flags & 0x01;
}

bool PacketManager::is_fin(UDP_Packet *packet) {
    return packet->header.flags & 0x04;
}

bool PacketManager::is_psh(UDP_Packet *packet) {
    return packet->header.flags & 0x08;
}

void PacketManager::set_ack(UDP_Packet *packet) {
    packet->header.flags |= 0x02;   // set ACK flag
}

void PacketManager::set_syn(UDP_Packet *packet) {
    packet->header.flags |= 0x01;   // set SYN flag
}

void PacketManager::set_fin(UDP_Packet *packet) {
    packet->header.flags |= 0x04;   // set FIN flag
}

void PacketManager::set_psh(UDP_Packet *packet) {
    packet->header.flags |= 0x08;   // set PSH flag
}

/*
 * Set the data of the packet
 * If the data is too long, only the first DATA_MAX_SIZE bytes will be copied
 * The length of the packet will be set by the length of the data
 */
void PacketManager::set_data(UDP_Packet *packet, u8* data, u16 data_length) {
    if (data_length > DATA_MAX_SIZE) { // max segment size is controlled by upper application, but still need for checking
        data_length = DATA_MAX_SIZE;
        cout << "Data too long, truncated" << endl;
    }
    memcpy(packet->data, data, data_length);
    if(data_length % 2 == 0) {  // even
        set_length(packet, data_length);
    } else {    // odd
        memset(packet->data + data_length, 0, 1);
        set_length(packet, data_length);
    }
}

void PacketManager::set_seq_num(UDP_Packet *packet, u32 seq_num) {
    packet->header.seq_num = seq_num;
}

void PacketManager::set_ack_num(UDP_Packet *packet, u32 ack_num) {
    packet->header.ack_num = ack_num;
}

void PacketManager::set_length(UDP_Packet *packet, u16 length) {
    packet->header.length = length;
}

void PacketManager::set_checksum(UDP_Packet *packet, u16 checksum) {
    packet->header.checksum = checksum;
}

u8* PacketManager::get_data(UDP_Packet *packet) {
    return packet->data;
}

u32 PacketManager::get_seq_num(UDP_Packet *packet) {
    return packet->header.seq_num;
}

u32 PacketManager::get_ack_num(UDP_Packet *packet) {
    return packet->header.ack_num;
}

string PacketManager::to_str(UDP_Packet *packet) {
    string str = "PKT: ";
    str += "seq_num: " + to_string(packet->header.seq_num) + ", ";
    str += "ack_num: " + to_string(packet->header.ack_num) + ", ";
    str += "data_len: " + to_string(packet->header.length) + ", ";
    str += "checksum: " + to_string(packet->header.checksum) + ", ";
    str += "flags: ";
    if(is_syn(packet)) {
        str += "SYN,";
    }
    if(is_ack(packet)) {
        str += "ACK,";
    }
    if(is_fin(packet)) {
        str += "FIN,";
    }
    if(is_psh(packet)) {
        str += "PSH,";
    }
    return str;
}

/*
 * Make a packet with the given parameters
 * 1. Set flags
 * 2. Set seq_num and ack_num
 * 3. Set data
 * 4. Calculate checksum
 */
UDP_Packet* PacketManager::make_packet(PacketManager::PacketType type, u32 seq_num, u32 ack_num, u8 *data, u16 data_length) {
    UDP_Packet* packet = new UDP_Packet();
    switch (type) { // set flags
        case SYN:
            set_syn(packet);
            break;
        case SYN_ACK:
            set_syn(packet);
            set_ack(packet);
            break;
        case ACK:
            set_ack(packet);
            break;
        case FIN:
            set_fin(packet);
            break;
        case FIN_ACK:
            set_fin(packet);
            set_ack(packet);
            break;
        case PSH:
            set_psh(packet);
            break;
        case PSH_ACK:
            set_psh(packet);
            set_ack(packet);
            break;
        default:
            cout << "Unknown packet type" << endl;
            return nullptr;
    }
    set_seq_num(packet, seq_num);
    set_ack_num(packet, ack_num);
    packet->header.filter = 0;
    set_data(packet, data, data_length);
    set_checksum(packet, calculate_checksum(packet));
    return packet;
}

File_Info* PacketManager::make_file_info(u16 MSS, u32 file_size, const string& file_name){
    File_Info* file_info = new File_Info();
    file_info->MSS = MSS;
    file_info->file_size = file_size;
    strcpy((char*)file_info->file_name, file_name.c_str());
    file_info->file_name_len = file_name.length();
    return file_info;
}
