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
    TIME_POINT local_start = TIME_NOW;
    Sleep(1000);
    cout << TIME_SPAN(local_start, Util::TimeStampType::SECOND) << endl;
#else
    Client client;
#endif
    return 0;
}
