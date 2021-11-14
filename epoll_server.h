#ifndef EPOLL_SERVER_H
#define EPOLL_SERVER_H
#include"tcpmsg.h"
#include<unistd.h>
#include<iostream>
#include<stdlib.h>
#include<string.h>
#include<malloc.h>
#include<fcntl.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<pthread.h>
// #include<map>
// #define MAX_SOCK_COUNT 1000
struct mybuff{
    char buff[1024];
};
class EpollServer{
    public:
        EpollServer();
        ~EpollServer();
        bool initServer(const char* addr,int port);
        static void listenThread(void* pvoid);
        void runServer();
    private:
        int m_epfd;
        int m_listenfd;
        int m_connactnum;//服务器在线连接数
        pthread_t m_listenThreadId;//监听线程句柄
        int connfd_sockfd[CLIENT_NUM];
        int sockfd_connfd[CLIENT_NUM];
        struct mybuff *allbuffs;
};


#endif