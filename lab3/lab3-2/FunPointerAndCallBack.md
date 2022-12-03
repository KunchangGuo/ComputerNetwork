A great example of function-call-back:

```c++
#include <iostream>
using namespace std;

typedef struct {
//#define CLOSED 0
//#define SYN_SENT 1
//#define ESTABLISH 2
    enum Status { CLOSED, SYN_SENT, ESTABLISH };
    uint8_t status;
    uint8_t (*fp)(void);
}WorkStatusType;

uint8_t send_syn(void) {
    cout << "Syn sent" <<endl;
}

uint8_t ack_syn(void) {
    cout << "Syn acknowledged" <<endl;
}

uint8_t send_data(void) {
    cout << "Data sent" <<endl;
}

WorkStatusType work_status_tab[] = {
        {WorkStatusType::Status::CLOSED, send_syn},
        {WorkStatusType::Status::SYN_SENT, ack_syn},
        {WorkStatusType::Status::ESTABLISH, send_data},
};

uint8_t work_call(uint8_t status) {
    if(status > 2 || status < 0) {
        cout << "Status is not included" <<endl;
        return 0;
    }
    return work_status_tab[status].fp();
//    uint8_t i;
//    for(i = 0; i < 3; i++) {
//        if(status == work_status_tab[i].status) {
//            return work_status_tab[i].fp();
//        }
//    }
//    return 0;
}

int main()
{
    work_call(WorkStatusType::Status::CLOSED);
    work_call(WorkStatusType::Status::SYN_SENT);
    work_call(WorkStatusType::Status::ESTABLISH);
   return 0;
}
```

> Reference:
>
> 1. [C语言回调函数详解](https://blog.csdn.net/qq_41854911/article/details/121058935)
>
> 2. [C++ 类成员函数的函数指针](https://blog.csdn.net/afei__/article/details/81985937)