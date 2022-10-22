#include <string>
#include <time.h>
#include "util.h"

string Util::currentTime()
{
	struct tm t;	// tm结构指针
	time_t now;	// 声明time_t类型变量
	time(&now);	// 获取系统日期和时间
	localtime_s(&t, &now);	// 获取当地日期和时间

	string s = "";
	if (t.tm_hour < 10) s += "0";
	s += to_string(t.tm_hour);
	s += ":";
	if (t.tm_min < 10) s += "0";
	s += to_string(t.tm_min);
	s += ":";
	if (t.tm_sec < 10) s += "0";
	s += to_string(t.tm_sec);
	return s;
}

string Util::getSubStr(string s, int start, int end)
{
	if (end == -1) {
		end = s.length() - 1;
	}
	int len = end - start + 1;
	return s.substr(start, len);
}
