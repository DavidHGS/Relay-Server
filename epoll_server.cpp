#include"epoll_server.h"
#include"tcpmsg.h"

using namespace std;

EpollServer::EpollServer(){
    m_connactnum=0;//初始化连接数为0
}

EpollServer::~EpollServer(){
    close(m_listenfd);
}

bool EpollServer::initServer(const char *addr,int port){
    sockaddr_in servaddr;

    m_listenfd=socket(AF_INET,SOCK_STREAM,0);

    if(m_listenfd<0){
        cout<<"create listenfd error"<<endl;
        return false;
    }

    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(port);
    inet_pton(AF_INET,addr,&servaddr.sin_addr);

    //设置端口复用
    int reused=1;
    setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&reused,sizeof(reused));

    if(bind(m_listenfd,(sockaddr*)&servaddr,sizeof(servaddr))<0){
        cout<<"bind error"<<endl;
        return false;
    }
    if(listen(m_listenfd,10240)<0){//最大监听数10240
        cout<<"listen error"<<endl;
        return false;
    }
    else{
        cout<<"server listen on"<<endl;
    }

    m_epfd=epoll_create(CLIENT_NUM);

    //设置epfd非阻塞
    int flag=fcntl(m_epfd,F_GETFL,0);
    if(fcntl(m_epfd,F_SETFL,flag|O_NONBLOCK)<0){
        cout<<"设置非阻塞失败"<<endl;
        return false;
    }
    //创建监听线程
    if(pthread_create(&m_listenThreadId,0,(void*(*)(void*))listenThread,this)!=0){
        cout<<"Server listenThread create fail"<<endl;
        return false;
    }
}
void EpollServer::listenThread(void *pvoid){
    EpollServer *server=(EpollServer*)pvoid;
    sockaddr_in remoteaddr;
    int addrlen =sizeof(remoteaddr);
    while(1){
        int connfd=accept(server->m_listenfd,(struct sockaddr *)&remoteaddr,(socklen_t*)&addrlen);
        if(connfd<0){
            cout<<"server accept error"<<endl;
            continue;
        }
        else{//connect成功
            //设置listenfd非阻塞
            int flag=fcntl(connfd,F_GETFL,0);
            fcntl(connfd,F_SETFL,flag|O_NONBLOCK);
            struct epoll_event tmpevent;
            tmpevent.data.fd=connfd;
            tmpevent.events=EPOLLIN|EPOLLOUT|EPOLLERR|EPOLLET;
            epoll_ctl(server->m_epfd,EPOLL_CTL_ADD,connfd,&tmpevent);
            server->m_connactnum++;
            cout<<"user " <<server->m_connactnum<<"connect success"<<endl;
        }
    }
}
// handleRecvMsg();
// handleSendMsg();
void EpollServer::runServer(){
    while(1){
        struct epoll_event events[CLIENT_NUM];
        int nready=epoll_wait(m_epfd,events,CLIENT_NUM,0);
        for(int i=0;i<nready;i++){
            int connfd=events[i].data.fd;
            char buff[CLIENT_NUM][1024];//MAX_SOCK_COUNT缓冲区
            // char recvbuff[1024];
            if(events[i].events&EPOLLIN){//读事件，接受数据
                int recv_size;
                int recv_total=0;
                while((recv_size=recv(connfd,buff[connfd],sizeof(buff[connfd]),0))>0){
                    cout<<"Server recv message content "<<buff[connfd]<<endl;
                    recv_total+=recv_size;
                    if(recv_total>=12){
                        tcpMsg tmpmsg;
                        tcpHeader tmpheader;
                        memcpy(buff[connfd],&tmpheader,sizeof(tcpHeader));
                        connfd_sockfd[connfd]=tmpheader.sourse_sockfd;//更新服务器sockfd和客户端connfd
                    }
                    if(recv_total>=1024){//用户缓冲区满
                        cout<<"user buff full"<<endl;
                        struct epoll_event tmpevent;
                        tmpevent.data.fd=connfd;
                        tmpevent.events=EPOLLOUT|EPOLLERR|EPOLLET;
                        epoll_ctl(m_epfd,EPOLL_CTL_MOD,events[i].data.fd,&tmpevent);
                        break;
                    }
                }
                if(recv_size<0){
                    cout<<"recv error:recv size:"<<recv_size<<endl;
                    struct epoll_event event_del;
                    event_del.data.fd=connfd;
                    event_del.events=0;
                    epoll_ctl(m_epfd,EPOLL_CTL_DEL,events[i].data.fd,&event_del);
                }else if(recv_size==0){//正常读完
                    cout<<"recv ok"<<endl;
                    struct epoll_event tmpevent;
                    tmpevent.data.fd=connfd;
                    tmpevent.events=EPOLLOUT|EPOLLERR|EPOLLET;
                    epoll_ctl(m_epfd,EPOLL_CTL_MOD,events[i].data.fd,&tmpevent);
                }
            }else if(events[i].events&EPOLLOUT){//写事件
                // char sendbuff[1024];
                int sendsize;
                int total_size=0;
                int userfd;
                if((connfd_sockfd[connfd]+1)%2==0){//奇数
                    userfd=connfd_sockfd[connfd]-1;//配对用户的sockfd比他小
                }else{
                    userfd=connfd_sockfd[connfd]+1;//偶数 则加1
                }
                while(sendsize=send(userfd,buff[connfd],strlen(buff[connfd]),0)>0){
                    total_size+=sendsize;
                    if(total_size>=1024){
                        cout<<"! msg: "<<buff[connfd]<<endl;
                        struct epoll_event tmpevent;
                        tmpevent.data.fd=events[i].data.fd;
                        tmpevent.events=EPOLLIN|EPOLLERR|EPOLLET;
                        epoll_ctl(m_epfd,EPOLL_CTL_MOD,events[i].data.fd,&tmpevent);
                        break;
                    }
                }   
                if(sendsize<0){
                    struct epoll_event event_del;
                    event_del.data.fd=events[i].data.fd;
                    event_del.events=0;
                    epoll_ctl(m_epfd,EPOLL_CTL_DEL,events[i].data.fd,&event_del);
                }else if(sendsize==0){//接收数据处理
                    cout<<"Server replay ok! msg: "<<buff[connfd]<<endl;
                    struct epoll_event tmpevent;
                    tmpevent.data.fd=events[i].data.fd;
                    tmpevent.events=EPOLLIN|EPOLLERR|EPOLLET;
                    epoll_ctl(m_epfd,EPOLL_CTL_MOD,events[i].data.fd,&tmpevent);
                }
            }else{//scokfd出错
                cout<<"Epoll error"<<endl;
                epoll_ctl(m_epfd,EPOLL_CTL_DEL,events[i].data.fd,&events[i]);
            }
        }
    }
}

int main(){
    EpollServer *epServer=new EpollServer();
    epServer->initServer("127.0.0.1",8000);
    epServer->runServer();
    return 0;
}