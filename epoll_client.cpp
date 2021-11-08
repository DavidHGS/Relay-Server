#include"epoll_client.h"
#include"tcpmsg.h"

EpollClient::EpollClient(int usercount,const char *addr,int port)
{
    strcpy(m_addr,addr);
    m_port=port;
    m_userCount=usercount;
    m_epollfd=epoll_create(MAXSOCKFDCOUNT);
    m_allUserStates=(struct userStates*)malloc(usercount*sizeof(struct userStates));
    for(int i=0;i<usercount;i++){
        m_allUserStates[i].states=FREE;
        // sprintf(m_allUserStates[i].buf,"%d",i);
        // m_allUserStates[i].buflen=strlen(m_allUserStates[i].buf)+1;
        m_allUserStates[i].sockfd=-1;
    }
    memset(m_sockfd_userid,0xff,sizeof(m_sockfd_userid));
}

EpollClient::~EpollClient()
{
    free(m_allUserStates);
}
// class EpollClient
// {
// private:
//     int m_userCount;//用户数量；
//     struct userStates *m_allUserStates;//用户状态数组
//     int m_epollfd;//需要创建epollfd
//     int m_sockfd_userid[MAXSOCKFDCOUNT];//将用户ID和socketid关联起来  
//     int m_port;//端口号
//     char m_addr[40];//IP地址
// public:
//     EpollClient(int usercount,const char* addr,int port);
//     int connToServ(int userId,const char *servaddr,ssize_t servport);//连接到服务器成功返回socketfd,失败返回的socketfd为-1 
//     int sendDataToServ(int userId);
//     int recvDataFromServ(int userId,char * buf,int buflen);
//     bool closeClient(int userId);
//     bool DelEpoll(int sockfd);
//     ~EpollClient();
// };

int EpollClient::connToServ(int userId,const char* serverip,unsigned short servport){
    if((m_allUserStates[userId].sockfd=socket(AF_INET,SOCK_STREAM,0))<0){//socket()
        printf("socket error%s\n",strerror(errno));
        m_allUserStates[userId].sockfd=-1;
        return -1;
    }

    struct sockaddr_in addr;
    bzero(&addr,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(servport);
    inet_pton(AF_INET,serverip,&addr.sin_addr);

    //支持端口复用  
    int reuseadd_on = 1;
    setsockopt(m_allUserStates[userId].sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseadd_on, sizeof(reuseadd_on));

    //设置非阻塞
    int flag=fcntl(m_allUserStates[userId].sockfd,F_GETFL,0);//获取建立的sockfd的当前状态
    if(fcntl(m_allUserStates[userId].sockfd,F_SETFL,flag|O_NONBLOCK)==-1){//设置非阻塞
        printf("%s:set nonblock error\n",__func__);
        close(m_allUserStates[userId].sockfd);
        return -1;
    }
    int n;
    if((n=connect(m_allUserStates[userId].sockfd,(struct sockaddr *)&addr,sizeof(addr)))<0){//connect()
        if(errno!=EINPROGRESS){
            return -1;
        }
    }
    m_allUserStates[userId].states=CONNECT_OK;
    return m_allUserStates[userId].sockfd;
}

int EpollClient::sendDataToServ(int userId){
    int sendsize=-1;
    if(m_allUserStates[userId].states==CONNECT_OK||m_allUserStates[userId].states==RECV_OK){
        //初始化报文头
        struct tcpHeader tcphd;
        tcphd.total_len=TCPLEN;
        tcphd.des_sockfd=m_allUserStates[userId].sockfd;//接收方sockfd
        if((tcphd.des_sockfd+1)%2==0){//奇数
            tcphd.sourse_sockfd=tcphd.des_sockfd-1;
        }else{
            tcphd.sourse_sockfd=tcphd.des_sockfd+1;
        }
        struct tcpMsg tcpmsg;
        tcpmsg.tcp_hd=&tcphd;
        for(int i=0;i<1011;i++){
            tcpmsg.msg[i]='a';
        }
        tcpmsg.msg[1012]='\n';
        
        int resverse_len=TCPLEN;//报文需要发送长度

        int buff_reverse_size=0;//socketbuf中剩余数据
        while(resverse_len>0){//数据包未发完
            int len=0;
            memset(m_allUserStates[userId].buf,0,1024);//缓冲空间初始化为0
            memcpy(m_allUserStates[userId].buf,&tcphd,sizeof(tcphd));//拷贝报文头
            len+=sizeof(tcphd);
            memcpy(m_allUserStates[userId].buf+len,&tcpmsg.msg,sizeof(tcpmsg.msg));//数据拷贝到用户缓冲
            buff_reverse_size=1012;//缓冲区剩余长度
            resverse_len-=1012;
            while(buff_reverse_size>0){//socket缓冲区中数据未发完
                sendsize=send(m_allUserStates[userId].sockfd,m_allUserStates[userId].buf,m_allUserStates[userId].buflen,0);
                 if(sendsize<0){
                    printf("send error %s\n",strerror(errno));
                }else{
                    buff_reverse_size-=sendsize;
                    printf("connfd %d send msg to connfd %d content:%s\n",tcphd.sourse_sockfd,tcphd.des_sockfd,m_allUserStates[userId].buf);
                }
            }
        }
        m_allUserStates[userId].states=SNED_OK;
    }
    return sendsize;
}

int EpollClient::recvDataFromServ(int userId,char *recvbuf,int buflen){
    int recvsize=-1;
    if(m_allUserStates[userId].states==SNED_OK){
        recvsize=recv(m_allUserStates[userId].sockfd,recvbuf,buflen,0);
        if(recvsize<0){
            printf("recive error %s\n",strerror(errno));
        }else if(recvsize==0){
            printf("Client %d :connection closed by Server ,Sockfd:%d\n",userId,m_allUserStates[userId].sockfd);
        }else{
            if(recvsize>12){//接受到真正数据
                tcpMsg tmpmsg;
                tcpHeader tmpheader;
                tmpmsg.tcp_hd=&tmpheader;
                memcpy(recvbuf,&tmpheader,sizeof(tcpHeader));
                memcpy(recvbuf+sizeof(tcpHeader),tmpmsg.msg,sizeof(recvbuf)-sizeof(tcpHeader));//读出剩余数据
                printf("connfd %d recive from connfd %d ,msg :%s\n",tmpmsg.tcp_hd->des_sockfd,tmpmsg.tcp_hd->sourse_sockfd,recvbuf);
                m_allUserStates[userId].states=RECV_OK;
            }
        }
    }
    return recvsize;
}

bool EpollClient::closeUser(int userId){
    close(m_allUserStates[userId].sockfd);
    m_allUserStates[userId].sockfd=-1;
    m_allUserStates[userId].states=FREE;
    return 1;
}

bool EpollClient::delEpoll(int sockfd){
    bool bret =false;
    struct epoll_event event_del;
    if(sockfd >0){
        event_del.data.fd=sockfd;
        event_del.events=0;
        if(epoll_ctl(m_epollfd,EPOLL_CTL_DEL,sockfd,&event_del)==0){
            bret=true;
        }else{
            printf("%s ,delEpoll error\n",__func__);
        }
        m_sockfd_userid[sockfd]=-1;
    }else{
        bret=true;
    }
    return bret;
}

int EpollClient::runFunc(){
    int sockfd=-1;
    for(int i=0;i<m_userCount;i++){
        struct epoll_event event;
        sockfd=connToServ(i,m_addr,m_port);//connected
        if(sockfd<0){
            printf("Run error,connect fail\n");
        }
        m_sockfd_userid[sockfd]=i;//socket和userid绑定
        event.data.fd=sockfd;
        event.events=EPOLLIN|EPOLLOUT|EPOLLET|EPOLLERR;//读写错误 ET
        m_allUserStates[i].epoll_event=event.events;
        epoll_ctl(m_epollfd,EPOLL_CTL_ADD,sockfd,&event);
    }
    while(1){
        struct epoll_event events[MAXSOCKFDCOUNT];
        char buf[1024];
        bzero(buf,sizeof(buf));

        int nready=epoll_wait(m_epollfd,events,MAXSOCKFDCOUNT,1000);//触发事件的fd个数
        for(int i=0;i<nready;i++){//处理所发生事件
            struct epoll_event tmpevent;
            int clientsockfd=events[i].data.fd;//触发事件的sockfd
            int userid=m_sockfd_userid[clientsockfd];//根据sockfd得到用户id

            if(events[i].events&EPOLLOUT){//写事件
                int nres=sendDataToServ(userid);//发送数据
                if(nres>0){//正常发送完成
                    tmpevent.data.fd=clientsockfd;
                    tmpevent.events=EPOLLIN|EPOLLET|EPOLLERR;
                    epoll_ctl(m_epollfd,EPOLL_CTL_MOD,tmpevent.data.fd,&tmpevent);//写完成，切换读
                }
                else{
                    printf("%s fail,send %d,userid:%d,fd:%d\n",__func__,nres,userid,clientsockfd);
                    delEpoll(clientsockfd);
                    closeUser(userid);
                }
            }
            else if(events[i].events&EPOLLIN){//读事件，接受数据
                int nlen=recvDataFromServ(userid,buf,1024);
                if(nlen<0){//接受数据错误
                    printf("%s fail,error %s \n",__func__,strerror(errno));
                    delEpoll(clientsockfd);
                    closeUser(userid);
                }else if(nlen==0){//对端服务器断开
                    printf("server disconnect ,userid:%d,sockfd:%d\n",userid,clientsockfd);
                    delEpoll(clientsockfd);
                    closeUser(userid);
                }else{//正常接收数据完成
                    m_sockfd_userid[clientsockfd]=userid;
                    tmpevent.data.fd=clientsockfd;
                    tmpevent.events=EPOLLOUT|EPOLLET;
                    epoll_ctl(m_epollfd,EPOLL_CTL_MOD,tmpevent.data.fd,&tmpevent);
                }
            }
            else{//事件错误处理
                printf("%s,other epoll error\n",__func__);
                delEpoll(events[i].data.fd);
                closeUser(userid);
            }
        }
    }
}

int main(int argc,char **argv){
    EpollClient *epollcli=new EpollClient(100,"127.0.0.1",8000);
    if(epollcli==NULL){
        printf("create epoll Client fail\n");
    }
    epollcli->runFunc();
    if(epollcli!=NULL){
        delete epollcli;
        epollcli=NULL;
    }
    return 0;
}

