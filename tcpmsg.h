#ifndef TCP_MSG_H
#define TCP_MSG_H
#include<unistd.h>
#define CLIENT_NUM 1000
struct tcpHeader{
    int total_len;
    ssize_t sourse_sockfd;
    ssize_t des_sockfd;
};//12bytes
struct tcpMsg{//数据包
    char msg[1012];
    struct tcpHeader *tcp_hd;
};
#endif