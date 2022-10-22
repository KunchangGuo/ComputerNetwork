#include <string>
#include <time.h>
#include "util.h"

string Util::currentTime()
{
	struct tm t;	// tm�ṹָ��
	time_t now;	// ����time_t���ͱ���
	time(&now);	// ��ȡϵͳ���ں�ʱ��
	localtime_s(&t, &now);	// ��ȡ�������ں�ʱ��

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
