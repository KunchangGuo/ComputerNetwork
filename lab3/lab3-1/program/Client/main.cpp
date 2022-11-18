#include <iostream>
#include <string>
#include "Client.h"
#include "Util.h"
//#define DEBUG_UTIL
#ifdef DEBUG_UTIL
#include "Util.h"
#include <windows.h>
    using namespace chrono;
#endif

int main() {
#ifdef DEBUG_UTIL
    int i = 0;
    vector<string> relative_files = REL_FILES("..\\res");
    for (const auto& file : relative_files) {
        cout << file << endl;
    }
    vector<string> absolute_files = ABS_FILES("..\\res");
    for(const string& file : absolute_files) {
        cout << i++ << ": " << file << endl;
    }
    cin >> i;
    cout << Util::get_file_size(absolute_files[i]) << endl;
    char buff[1028];
    buff[1024] = 0;
    fstream file(absolute_files[i], ios::in | ios::binary);
    fstream dump_file(relative_files[i], ios::out | ios::binary);
    unsigned int offset = 0;
    unsigned int read_len = 0;
    while((read_len = Util::read_data(&file, (unsigned char*)buff, 1024))) {
        offset += read_len;
        Util::dump_data(&dump_file, (unsigned char*)buff, read_len);
    }
    cout << "offset: " << offset << endl;
#else
    Client client;
#endif
    return 0;
}
