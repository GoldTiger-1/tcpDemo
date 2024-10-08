#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<errno.h>
#include <fcntl.h>
#include <sys/epoll.h> 
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>//close()
#include<netinet/in.h>//struct sockaddr_in
#include <netinet/tcp.h>
#include<arpa/inet.h>//inet_ntoa
#include<pthread.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include"tscns.h"
#include <sys/eventfd.h>
#include <netdb.h>
// #include"efvitcp1.cpp"

TSCNS T;
int64_t ts11,ts22;
 #define DEST_PORT 6666
//  #define DEST_IP_ADDRESS "172.16.38.105"
 #define DEST_IP_ADDRESS "192.168.92.12"
//  #define DEST_IP_ADDRESS "127.0.0.1"

#define MAX_EVENT           (128) 
#define MAX_BUFSIZE         (4096)   

typedef pthread_t THREAD_HANDLE;
typedef unsigned int DWORD;
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

class CThread
{
public:
	/**构造函数
	 */
	CThread()
	{
		m_hThread = (THREAD_HANDLE)0;
		m_IDThread = 0;
	}

	/**析构函数
	 */
	virtual ~CThread() {}

	/**创建一个线程
	 * @return true:创建成功 false:创建失败
	 */
	virtual bool Create()
	{
		if (m_hThread != (THREAD_HANDLE)0)
		{
			return true;
		}
		bool ret = true;
#ifdef WIN32
		m_hThread = ::CreateThread(NULL, 0, _ThreadEntry, this, 0, &m_IDThread);
		if (m_hThread == NULL)
		{
			ret = false;
		}
#else
		ret = (::pthread_create(&m_hThread, NULL, &_ThreadEntry, this) == 0);
#endif
		return ret;
	}

	void ExitThread()
	{
#ifdef WIN32
		::ExitThread(0);
#endif
	}

private:
#ifdef WIN32
	static DWORD WINAPI _ThreadEntry(LPVOID pParam)
#else
	static void *_ThreadEntry(void *pParam)
#endif
	{
		CThread *pThread = (CThread *)pParam;
		if (pThread->InitInstance())
		{
			pThread->Run();
		}

		pThread->ExitInstance();

		// 20140613 xuzh 如果设置为0，join部分就无法join了，导致资源无法释放
		// pThread->m_hThread = (THREAD_HANDLE)0;

		return NULL;
	}

	/**虚函数，子类可做一些实例化工作
	 * @return true:创建成功 false:创建失败
	 */
	virtual bool InitInstance()
	{
		return true;
	}

	/**虚函数，子类清楚实例
	 */
	virtual void ExitInstance() {}

	/**线程开始运行，纯虚函数，子类必须继承实现
	 */
	virtual void Run() = 0;

private:
	THREAD_HANDLE m_hThread; /**< 线程句柄 */
	DWORD m_IDThread;
};

class COrderThread : public CThread
{
public:
	COrderThread(int fd):cfd(fd){ memset(szSendBuf,'1',200); }
	~COrderThread() {}

	virtual void Run()
	{
        uint32_t count=0;
		while (true)
		{
            //发送数据
            // 发送数据
            struct msghdr msg;
            struct iovec iov;
            iov.iov_base = szSendBuf;
            iov.iov_len = sizeof(szSendBuf);
            msg.msg_iov = &iov;
            msg.msg_iovlen = 1;
            msg.msg_control = NULL;
            msg.msg_controllen = 0;
            msg.msg_flags = 0;

            ts11 = T.rdns();
            int nSendNum = sendmsg(cfd, &msg, 0);
            ts22 = T.rdns();
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
                printf("sendmsg fd=%d success, No.%d\n",cfd, count++);
            }
            // printf("send pid=%lu\n",pthread_self());
            printf("send latency:%ld \n",ts22-ts11 );
            usleep(100);
            // MSLEEP(1);
		}
		
	}

private:
	int cfd;
    char szSendBuf[200];
};

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

    int i,j=0;
    int nSockfd;
    int nEventNum;
    int nSendNum;
    int nRecvNum;

    char szSendBuf[200];
    memset(szSendBuf,'1',200);
    char szRecvBuf[MAX_BUFSIZE];

    sleep(1);

    COrderThread obj(sock_fd);
    obj.Create();
    // COrderThread obj2(sock_fd);
    // obj2.Create();
    // COrderThread obj3(sock_fd);
    // obj3.Create();
    for (;;)
    {
        j++;
        memset(events,0,sizeof(events));
        // printf("--------epoll_wait--------\n");
        // ts11 = T.rdns();
        nEventNum = epoll_wait(nEpollfd, events, MAX_EVENT, 0);
        // usleep(10);
        // ts22 = T.rdns();
        // printf("epoll_wait latency:%ld ns nEveNum=%d\n",ts22-ts11 ,nEventNum);
        // printf("----------------\n");
        // printf(" epoll events num:%d\n", nEventNum);
        // printf("poll pid=%lu\n",pthread_self());
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
                // ts22 = T.rdns();
                // printf("recvmsg latency:%ld ns\n",ts22-ts11);
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

        }
    }

    // close(sock_fd);

    return 0;
}