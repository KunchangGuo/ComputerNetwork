#include "Util.h"
#include <iostream>
#include <sstream>
#include <ctime>
//#include "windows.h"

string Util::curr_time(int time_stamp_type) {
    using namespace chrono;
    system_clock::time_point now = system_clock::now();

    time_t now_time_t = system_clock::to_time_t(now);
    tm* now_tm = localtime(&now_time_t);

    char buffer[128] = {0};
    strftime(buffer, 128, "%H:%M:%S", now_tm);

    ostringstream ss;
    ss.fill('0');

    milliseconds ms;
    microseconds cs;
    nanoseconds ns;

    switch (time_stamp_type)
    {
        case SECOND:
            ss << buffer;
            break;
        case MILLISECOND:
            ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
            ss << buffer << ":" << ms.count();
            break;
        case MICROSECOND:
            ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
            cs = duration_cast<microseconds>(now.time_since_epoch()) % 1000000;
            ss << buffer << ":" << ms.count() << ":" << cs.count() % 1000;
            break;
        case NANOSECOND:
            ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
            cs = duration_cast<microseconds>(now.time_since_epoch()) % 1000000;
            ns = duration_cast<nanoseconds>(now.time_since_epoch()) % 1000000000;
            ss << buffer << ":" << ms.count() << ":" << cs.count() % 1000 << ":" << ns.count() % 1000;
            break;
        default:
            ss << buffer;
            break;
    }
    return ss.str();
}

double Util::time_span(chrono::high_resolution_clock::time_point start, int time_stamp_type)
{
    using namespace chrono;
    high_resolution_clock::time_point end = high_resolution_clock::now();
    duration<double> time_span_milliseconds = duration_cast<duration<double>>(end - start);
    if(time_stamp_type == MILLISECOND){
        return time_span_milliseconds.count() * 1000;
    }
    else if(time_stamp_type == MICROSECOND){
        return time_span_milliseconds.count() * 1000000;
    }
    else if(time_stamp_type == NANOSECOND){
        return time_span_milliseconds.count() * 1000000000;
    }
    else{
        return time_span_milliseconds.count();
    }
}

vector<string> Util::get_files(const string& dir_path){
    namespace fs = std::filesystem;
    if(!fs::exists(dir_path)){
        return {};
    }
    if(!fs::is_directory(dir_path)){
        return {};
    }
    vector<string> files;
    for(const auto & file : fs::directory_iterator(dir_path)){
        files.push_back(file.path().filename().string());
    }
    return files;
}

vector<string> Util::get_absolute_files(const string& dir_path) {
    namespace fs = std::filesystem;
    if(!fs::exists(dir_path)){
        return {};
    }
    if(!fs::is_directory(dir_path)){
        return {};
    }
    vector<string> files;
    for(const auto & file : fs::directory_iterator(dir_path)){
        files.push_back(fs::absolute(file.path()).string());
    }
    return files;
}

unsigned int Util::get_file_size(const string& file_path){
    namespace fs = std::filesystem;
    return fs::file_size(file_path);
}

void Util::mkdir(const std::string &dir_path) {
    namespace fs = std::filesystem;
    if(!fs::exists(dir_path)){
        fs::create_directory(dir_path);
    }
}

unsigned short Util::read_data(std::fstream *file, char *buff, unsigned short max_segment_size) {
    if(!file->is_open()){
        return 0;
    }
    file->read((char *)buff, max_segment_size);
    return (unsigned short)file->gcount();
}

bool Util::dump_data(std::fstream *file, char *data, unsigned short data_len) {
    if(!file->is_open()){
        return false;
    }
    file->write((char *)data, data_len);
    return true;
}

void Util::log(Util::INFO_TYPE type, std::string msg) {
    switch(type) {
        case Util::INFO_TYPE::STATE: {
            string new_msg = curr_time() + " [STATE] " + msg;
//            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN);
            cout << new_msg << endl;
            break;
        }
        case Util::INFO_TYPE::PLAIN: {
            string new_msg = curr_time() + " [PLAIN] " + msg;
//            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE);
            cout << new_msg << endl;
            break;
        }
        case Util::INFO_TYPE::EXCPT: {
            string new_msg = curr_time() + " [EXCPT] " + msg;
//            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED);
            cout << new_msg << endl;
            break;
        }
        default: {
            cout << "No matching info_type" <<endl;
            return;
        }
    }
}

void Util::circulative_add(uint32_t *num, uint32_t increment) {
    if(*num + increment > UINT32_MAX){
        *num = *num + increment - UINT32_MAX;
    }
    else{
        *num += increment;
    }
}

uint32_t Util::circulative_add(uint32_t num1, uint32_t num2) {
    if(num1 + num2 > UINT32_MAX){
        return num1 + num2 - UINT32_MAX;
    }
    else{
        return num1 + num2;
    }
}
