#ifndef CLIENT_UTIL_H
#define CLIENT_UTIL_H

#include <string>
#include <chrono>
#include <vector>
#include <filesystem>
#include <fstream>
using namespace std;


class Util {
public:
#define TIME_SEC curr_time(Util::SECOND)
#define TIME_MSEC curr_time(Util::MILLISECOND)
#define TIME_USEC curr_time(Util::MICROSECOND)
#define TIME_NSEC curr_time(Util::NANOSECOND)
#define TIME_POINT chrono::high_resolution_clock::time_point
#define TIME_NOW chrono::high_resolution_clock::now()
#define TIME_SPAN(time_point, time_stamp_type) Util::time_span(time_point, time_stamp_type)
#define TIME_OUT(start) ((unsigned int)TIME_SPAN(start, Util::TimeStampType::MILLISECOND) >= time_out)

enum TimeStampType {
    SECOND = 0,
    MILLISECOND = 1,
    MICROSECOND = 2,
    NANOSECOND = 3
};

    static string curr_time(int time_stamp_type = Util::MILLISECOND);   // get time stamp
    static double time_span(chrono::high_resolution_clock::time_point start, int time_stamp_type = Util::MILLISECOND); // get time span

public:
#define REL_FILES(dir_path) Util::get_files(dir_path)
#define ABS_FILES(dir_path) Util::get_absolute_files(dir_path)
#define FILE_SIZE(file_path) Util::get_file_size(file_path)
#define MKDIR(dir_path) Util::mkdir(dir_path)
#define READ_DATA(file, buff, max_segment_size) Util::read_data(file, buff, max_segment_size)
#define DUMP_DATA(file, data, data_len) Util::dump_data(file, (char*)data, data_len)

    static vector<string> get_files(const string& dir_path);
    static vector<string> get_absolute_files(const string& dir_path);
    static unsigned int get_file_size(const string& file_path);    // file size is limited to 4GB
    static void mkdir(const string& dir_path);
    static unsigned short read_data(fstream* file, char* buff, unsigned short max_segment_size);  // return the number of bytes read
    static bool dump_data(fstream* file, char* data, unsigned short data_len);

public:
#define STATE_LOG(msg) Util::log(Util::INFO_TYPE::STATE, msg)
#define PLAIN_LOG(msg) Util::log(Util::INFO_TYPE::PLAIN, msg)
#define EXCPT_LOG(msg) Util::log(Util::INFO_TYPE::EXCPT, msg)
    enum INFO_TYPE {
        STATE,
        PLAIN,
        EXCPT
    };

    static void log(INFO_TYPE type, string msg);
};


#endif //CLIENT_UTIL_H
