#include "Server.h"
//#define DEBUG_PM
#ifdef DEBUG_PM
#include "PacketManager.h"
#include <iostream>
#endif

int main() {
//    Server server;
#ifdef DEBUG_PM
    u16 max_segment_size = 1024;
    u32 file_len = 1024;
    const string file_name = "test.txt";
    File_Info* file_info = PacketManager::make_file_info(max_segment_size, file_len, file_name);
    u16 data_len = FILE_INFO_LEN(file_info);
    cout << "data_len: " << data_len << endl;
    UDP_Packet* packet = PacketManager::make_packet(PacketManager::PacketType::PSH, 0, 0, (u8*)file_info, data_len);
    cout << "Packet: " << STR(packet) << endl;
    cout << "Packet length: " << TOTAL_LEN(packet) << endl;
    cout << "Packet data length: " << DATA_LEN(packet) << endl;
    cout << "MSS" << file_info->MSS << endl;
    cout << "File size: " << file_info->file_size << endl;
    cout << "File name: " << file_info->file_name << endl;
    cout << "File name length: " << file_info->file_name_len << endl;
#else
    Server server;
#endif
    return 0;
}
