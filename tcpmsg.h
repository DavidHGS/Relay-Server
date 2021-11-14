#ifndef TCP_MSG_H
#define TCP_MSG_H
#include<unistd.h>
#define CLIENT_NUM 10000
#define BUFF_SIZE 12000
struct tcpHeader{//报文头
    int total_len;//报文长度
    __uint32_t sourse_sockfd;//发送fd
    __uint32_t des_sockfd;//接受fd
};//12bytes
struct tcpMsg{//数据包
    char msg[1012];
    struct tcpHeader *tcp_hd;
};
#endif