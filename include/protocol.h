#ifndef PROTOCOL_H
#define PROTOCOL_H
#include<sys/types.h>
struct Bid{
    pid_t process_id;
    int value;
};
#endif