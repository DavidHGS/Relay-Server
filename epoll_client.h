#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<malloc.h>
#include<arpa/inet.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/epoll.h>
#define MAXSOCKFDCOUNT 65535 //最大端口数量
#define TCPLEN 1012;//报文长度

typedef enum{
    FREE=0,//空闲
    CONNECT_OK=1,//连接成功
    SNED_OK=2,//发送成功
    RECV_OK=3//接受成功
}EPOLL_STATES;



struct userStates
{
    EPOLL_STATES states;
    int sockfd;
    char buf[1024];
    int buflen;
    ssize_t epoll_event;
};

class EpollClient
{
private:
    int m_userCount;//用户数量；
    struct userStates *m_allUserStates;//用户状态数组
    int m_epollfd;//需要创建epollfd
    int m_sockfd_userid[MAXSOCKFDCOUNT];//将用户ID和socketid关联起来  
    int m_port;//端口号
    char m_addr[40];//IP地址
public:
    EpollClient(int usercount,const char *addr,int port);
    int connToServ(int userId,const char *servaddr,unsigned short servport);//连接到服务器成功返回socketfd,失败返回的socketfd为-1 
    int sendDataToServ(int userId);
    int recvDataFromServ(int userId,char *recvbuf,int buflen);
    bool closeUser(int userId);
    bool delEpoll(int sockfd);
    int runFunc();
    ~EpollClient();
};
