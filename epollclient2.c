 #include<stdio.h>
 #include<string.h>
 #include<stdlib.h>
 #include <cstdlib>
 #include<errno.h>
 #include <fcntl.h>
 #include <sys/epoll.h> 
 #include<sys/types.h>
 #include<sys/socket.h>
 #include<unistd.h>//close()
 #include<netinet/in.h>//struct sockaddr_in
 #include <netinet/tcp.h>   //TCP_QUICKACK
 #include<arpa/inet.h>//inet_ntoa
 #include<pthread.h>
 #include <sys/ioctl.h>
 #include <net/if.h>
 #include"tscns.h"
 #include <sys/eventfd.h>
#include <netdb.h>
TSCNS T;
int64_t ts11,ts22;
 #define DEST_PORT 6666
//  #define DEST_IP_ADDRESS "172.16.38.105"
 #define DEST_IP_ADDRESS "192.168.92.12"
//  #define DEST_IP_ADDRESS "127.0.0.1"

#define MAX_EVENT           (128) 
#define MAX_BUFSIZE         (4096)   

/// @brief 
/// @param p_nSock 
/// @return 
int setNonBlocking(int p_nSock)
{
    int nOpts;
    nOpts = fcntl(p_nSock, F_GETFL);
    if (nOpts<0)
    {
        printf("[%s %d] Fcntl Sock GETFL fail!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    nOpts = nOpts | O_NONBLOCK;
    if (fcntl(p_nSock, F_SETFL, nOpts)<0)
    {
        printf("[%s %d] Fcntl Sock SETFL fail!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

//纳秒时间

void gettime(int &lt)
{
    struct timespec tpTime;
    clock_gettime(CLOCK_REALTIME, &tpTime);
    long long llTime = tpTime.tv_sec * 1000000000.0+tpTime.tv_nsec;
    lt = llTime & 0x7FFFFFFF;
}
int main(int argc,char *argv[])
{
    T.init(2000000000,3000000000);

    int sock_fd;
    sock_fd = socket(AF_INET,SOCK_STREAM,0);
    if(sock_fd < 0)
    {
        printf("[%s %d] Socket Create fail return:%d!\n", __FUNCTION__, __LINE__, sock_fd);
        return 0;
    } 

    struct sockaddr_in addr_serv;//服务器端地址
    memset(&addr_serv,0,sizeof(addr_serv));

    addr_serv.sin_family = AF_INET;
    addr_serv.sin_port =  htons(DEST_PORT);
    addr_serv.sin_addr.s_addr = inet_addr(DEST_IP_ADDRESS);

    setNonBlocking(sock_fd);
    // printf("errno=%d : %s  %d\n",errno,strerror(errno),__LINE__);

    // int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
    int gsndto=0;
    socklen_t tlen=sizeof(gsndto);
    int ret1 = getsockopt(sock_fd,SOL_SOCKET,SO_SNDTIMEO,&gsndto,&tlen);
    printf("getsockopt ret=%d gsndto= %d s\n",ret1,gsndto);

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    socklen_t vlen = sizeof(timeout);
    int ret2=setsockopt(sock_fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,vlen);
    printf("setsockopt ret=%d\n",ret2);

    // printf("errno=%d : %s  %d\n",errno,strerror(errno),__LINE__);

// 2、设置 sockFd 超时时间*/
	
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    int on = 1; 
    int ret3=setsockopt(sock_fd,IPPROTO_TCP,TCP_NODELAY,(char*)&on,sizeof(on));
    printf("set nodelay ret=%d\n",ret3);

    int on2=1;
    int retnodelay = setsockopt(sock_fd,IPPROTO_TCP,TCP_QUICKACK,(char*)&on2, sizeof(on2));
    if(retnodelay==0){
        printf("set QUICKACK success\n");
    }else{
        printf("set QUICKACK fail\n");
    }

    if( connect(sock_fd,(struct sockaddr *)&addr_serv,sizeof(struct sockaddr)) < 0)
    {
        // printf("[%s %d] Client connect errno:%d!\n",__FUNCTION__,__LINE__,errno);
        if (errno != EINPROGRESS) 
        {
            printf("[%s %d] Connnect Remote Server fail.\n",__FUNCTION__, __LINE__);
            return(0);
        } 
    }

    struct sockaddr_in localaddr;
    memset(&localaddr,0,sizeof(localaddr));
    socklen_t addrlen=sizeof(localaddr);
    int retgs = getsockname(sock_fd,(struct sockaddr *)&localaddr,&addrlen);
    if(retgs==0){
        printf("getsock localaddr= %s:%d\n",inet_ntoa(((struct sockaddr_in*)&localaddr)->sin_addr),ntohs(((struct sockaddr_in*)&localaddr)->sin_port));
    }
    else{
        printf("getsockname fail\n");
    }

    struct sockaddr_in serveraddr;
    memset(&serveraddr,0,sizeof(serveraddr));
    socklen_t serlen=sizeof(serveraddr);
    char ipAddr[serlen+1];
    int retgp= getpeername(sock_fd,(struct sockaddr *)&serveraddr,&serlen);
    if(retgp==0){
        printf("connected peer address = %s:%d\n", inet_ntop(AF_INET, &serveraddr.sin_addr, ipAddr, sizeof(ipAddr)), ntohs(serveraddr.sin_port));
    }
    else{
        printf("getpeername fail\n");
    }


    int nEpollfd;
    struct epoll_event ev;
    struct epoll_event events[MAX_EVENT];

    //生成用epoll专用文件描述符   
    nEpollfd=epoll_create(MAX_EVENT);
    if (nEpollfd <= 0)
    {
        printf("[%s %d] Epoll create fail return:%d!\n",__FUNCTION__,__LINE__,nEpollfd);
        return 0;
    }

    ev.data.fd=sock_fd;        
    ev.events=EPOLLIN|EPOLLOUT;     
    if (epoll_ctl(nEpollfd,EPOLL_CTL_ADD,sock_fd,&ev) < 0)
    {
        printf("[%s %d] Epoll ctl error!\n",__FUNCTION__,__LINE__);
        return 0;
    }


// 2--
    int sock_fd2 = socket(AF_INET,SOCK_STREAM,0);
    setNonBlocking(sock_fd2);
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    int retst = setsockopt(sock_fd2, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    if(retst == 0){
        printf("SO_SNDTIMEO set success\n");
    }else{
        printf("SO_SNDTIMEO set error\n");
    }

    // if( connect(sock_fd2,(struct sockaddr *)&addr_serv,sizeof(struct sockaddr)) < 0)
    // {
    //     if (errno != EINPROGRESS) 
    //     {
    //         printf("[%s %d] Connnect Remote Server fail.\n",__FUNCTION__, __LINE__);
    //         // return(0);
    //     } 
    // }
    
    // int retgs2 = getsockname(sock_fd2,(struct sockaddr *)&localaddr,&addrlen);
    // if(retgs2==0){
    //     printf("getsock2 localaddr= %s:%d\n",inet_ntoa(((struct sockaddr_in*)&localaddr)->sin_addr),ntohs(((struct sockaddr_in*)&localaddr)->sin_port));
    // }
    // else{
    //     printf("getsockname fail\n");
    // }

    // memset(&serveraddr,0,sizeof(serveraddr)); 
    // retgp= getpeername(sock_fd2,(struct sockaddr *)&serveraddr,&serlen);
    // if(retgp==0){
    //     printf("connected2 peer address = %s:%d\n", inet_ntop(AF_INET, &serveraddr.sin_addr, ipAddr, sizeof(ipAddr)), ntohs(serveraddr.sin_port));
    // }
    // else{
    //     printf("getpeername fail\n");
    // }

    // struct tcp_info info;
    // int len = sizeof(info);
    // getsockopt(sock_fd2, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&len);
    // printf("info.tcpi_state = %d\n",info.tcpi_state);
    // if(info.tcpi_state == TCP_ESTABLISHED)
    // {
    //     printf("connect ok \r\n");
    //     //return 0;
    // }
    // else
    // {
    //     printf("connect error\r\n");
    //     //return -1;
    // }

    // int t = 1;
    // int rett = setsockopt( sock_fd2, IPPROTO_TCP, TCP_QUICKACK, (void *)&t, sizeof(t));
    // printf(" set TCP_QUICKACK ret= %d\n",rett);
    // rett = setsockopt( sock_fd2, IPPROTO_TCP, TCP_NODELAY, (void *)&t, sizeof(t));
    // printf(" set TCP_NODELAY ret= %d\n",rett);


    // memset(&ev,0,sizeof(ev));
    // ev.data.fd=sock_fd2;        
    // ev.events=EPOLLIN|EPOLLOUT;     
    // if (epoll_ctl(nEpollfd,EPOLL_CTL_ADD,sock_fd2,&ev) < 0)
    // {
    //     printf("[%s %d] Epoll ctl error!\n",__FUNCTION__,__LINE__);
    //     return 0;
    // }


    int i,j=0;
    int nSockfd;
    int nEventNum;
    int nSendNum;
    int nRecvNum;
    // char szSendBuf[]="NonBlocking Epoll Test!";
    char szSendBuf[200];
    memset(szSendBuf,'1',200);
    char szRecvBuf[MAX_BUFSIZE];
    // char mbuf[200];
    
    // int  iLocalTime =0;
    // int  iLocalTime2 =0;

    sleep(1);
    int count = 100000;
    int number=0;
    for (;;)
    {
        j++;
        memset(events,0,sizeof(events));
        // printf("--------epoll_wait--------\n");
        // ts11 = T.rdns();
        nEventNum = epoll_wait(nEpollfd, events, MAX_EVENT, 0);
        // ts22 = T.rdns();
        // printf("epoll_wait latency:%ld ns nEveNum=%d\n",ts22-ts11 ,nEventNum);
        // printf("----------------\n");
        // printf(" epoll events num:%d\n", nEventNum);

        for (i = 0; i<nEventNum; i++)
        {
            if ((events[i].events & EPOLLERR) ||  
                    (events[i].events & EPOLLHUP))
            {  
                printf("[%s %d] fd=%d Epoll event error!\n",__FUNCTION__,__LINE__,events[i].data.fd);
                close (events[i].data.fd);
                return 0;
            }

            if (events[i].events & EPOLLIN)
            {
                // printf("[%d] EPOLLIN Event.\n", __LINE__);
                if ((nSockfd = events[i].data.fd) < 0)
                {
                    continue;
                }

                memset(szRecvBuf, 0x00, sizeof(szRecvBuf));
                // nRecvNum = recv(nSockfd, szRecvBuf, sizeof(szRecvBuf), 0);

                // 接收数据
                struct msghdr msgrecv;
                // struct iovec iovr;
                struct iovec iovr[1];
                iovr[0].iov_base = szRecvBuf;
                iovr[0].iov_len = sizeof(szRecvBuf);
                // msg.msg_name = &client_addr;
                // msg.msg_namelen = addrlen;
                msgrecv.msg_iov = iovr;
                msgrecv.msg_iovlen = 1;
                msgrecv.msg_control = NULL;
                msgrecv.msg_controllen = 0;
                msgrecv.msg_flags = 0;
                // ts11 = T.rdns();
                nRecvNum = recvmsg(nSockfd,&msgrecv,0);
                int64_t tssend = atoll(szRecvBuf);
                ts22 = T.rdns();
                // printf("tssend = %ld ts22=%ld\n",tssend,ts22);
                
                printf("recvmsg latency:%ld ns\n",ts22-tssend);
                if (nRecvNum < 0)
                {
                    // printf("[%s %d] Client Recv Data Error!\n",__FUNCTION__, __LINE__);
                    // printf("errno=%d %s\n",errno,strerror(errno));
                    if(errno == EAGAIN || errno== EWOULDBLOCK || errno==EINTR)
                        continue;
                    else
                        return 0;
                }
                else{
                    printf("recvmsg len= %d\n",nRecvNum);
                }

                // printf("\n******************************\n");
                // // printf("**RecvData:%s\n", szRecvBuf);
                // printf("**fd:%d RecvData:%s\n",nSockfd, msgrecv.msg_iov[0].iov_base);
                // printf("******************************\n");

            }

            if (events[i].events & EPOLLOUT) 
            {
                // if(number%100){
                //     number++;
                //     continue;
                // }
                // printf("[%d] EPOLLOUT Event.\n",__LINE__);
                if ((nSockfd = events[i].data.fd) < 0)
                {
                    continue;
                }
                memset(szSendBuf,0,sizeof(szSendBuf));
                // 发送数据
                struct msghdr msg;
                struct iovec iov;
                iov.iov_base = szSendBuf;
                iov.iov_len = sizeof(szSendBuf);
                // iov.iov_base = sSendBuf;
                // iov.iov_len = 256;
                // msg.msg_name = &client_addr;
                // msg.msg_namelen = addrlen;
                msg.msg_iov = &iov;
                msg.msg_iovlen = 1;
                msg.msg_control = NULL;
                msg.msg_controllen = 0;
                msg.msg_flags = 0;

                // nSendNum = send(nSockfd, szSendBuf, sizeof(szSendBuf), 0);
                ts11 = T.rdns();
                // gettime(iLocalTime);
                // usleep(10);
                sprintf(szSendBuf,"%ld",ts11);
                // printf("ts11=%ld szSendBuf=%s\n",ts11,szSendBuf);
                nSendNum = sendmsg(nSockfd, &msg, 0);
                // count--;
                // printf("count=%d\n",count);
                // if(count<0)
                //     return 0;
                // gettime(iLocalTime2);
                // ts22 = T.rdns();
                // printf("sendmsg latency:%ld ns\n",ts22-ts11);
                // printf("sendmsg latency:%ld ns\n",iLocalTime2-iLocalTime);

                if (nSendNum < 0)
                {
                    printf("[%s %d] Send Error", __FUNCTION__,__LINE__);
                    // printf("errno=%d %s\n",errno,strerror(errno));
                    // return 0;
                }else if(nSendNum==0){
                    printf("sendmsg len=0\n");
                    // return 0;
                }
                else{
                    printf("sendmsg fd=%d success\n",nSockfd);
                }
                // sleep(1);
                usleep(1);
            }

            
            // ts11 = T.rdns();
            // memcpy(mbuf,szRecvBuf,sizeof(szRecvBuf));
            // ts22 = T.rdns();
            // printf("memcpy199B latency:%ld ns\n",ts22-ts11);
        }
        
        
        // ts22 = T.rdns();
        // printf("epolllatency ret:%d  :%ld ns\n",nEventNum,ts22-ts11);

    }

    // close(sock_fd);
    // shutdown(sock_fd);
    return 0;
}