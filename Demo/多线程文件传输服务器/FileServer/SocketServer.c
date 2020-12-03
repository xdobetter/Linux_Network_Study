#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <syslog.h>
#include <sys/epoll.h>
#include <pthread.h>

#include "SocketServer.h"
#include "debug.h"

int epfd = -1;
SocketRecord_t *socketRecordHead = NULL;

int CreateSocketRecord(void)
{
	int tr = 1;
	SocketRecord_t *SrcRecord;
	SocketRecord_t *NewSocket =(SocketRecord_t *) calloc(1 , sizeof(SocketRecord_t));
	NewSocket->ClientAddrLen = sizeof(NewSocket->ClientAddr);
	NewSocket->SocketFd = accept(socketRecordHead->SocketFd,(struct sockaddr *) &(NewSocket->ClientAddr), &(NewSocket->ClientAddrLen));
	if (NewSocket->SocketFd < 0) {
		DEBUG_INFO("ERROR on accept");
		free(NewSocket);
		return -1;
	}
    DEBUG_INFO("client %s connect",inet_ntoa(NewSocket->ClientAddr.sin_addr));
    //为客户端创建文件目录 
    char ClientDataBaseDir[64] = {0};
    sprintf(ClientDataBaseDir,"%s/%s",FileDataBaseDir,inet_ntoa(NewSocket->ClientAddr.sin_addr));
    mkdir(ClientDataBaseDir,S_IRWXU|S_IRWXG |S_IROTH|S_IXOTH);
    pthread_mutex_init(&(NewSocket->ReceDataLock),NULL);
    
    AddEpollEvent(NewSocket->SocketFd);
    SetLinger(NewSocket->SocketFd,1);
	// "Address Already in Use" error on the bind
	setsockopt(NewSocket->SocketFd, SOL_SOCKET, SO_REUSEADDR, &tr, sizeof(int));
	fcntl(NewSocket->SocketFd, F_SETFL, O_NONBLOCK);//多线程就采用,阻塞接收，默认是阻塞
	NewSocket->next = NULL;
	SrcRecord = socketRecordHead;
	while (SrcRecord->next)
		SrcRecord = SrcRecord->next;

	SrcRecord->next = NewSocket;
	return (NewSocket->SocketFd);
}

void DeleteSocketRecord(int RmSocketFd)
{
	SocketRecord_t *SrcRecord, *PrevRecord = NULL;
	SrcRecord = socketRecordHead;
	while ((SrcRecord->SocketFd != RmSocketFd) && (SrcRecord->next)) {
		PrevRecord = SrcRecord;
		SrcRecord = SrcRecord->next;
	}

	if (SrcRecord->SocketFd != RmSocketFd) {
		return;
	}

	if (SrcRecord) {
		if (PrevRecord == NULL) {
			return;
		}
		PrevRecord->next = SrcRecord->next;
        pthread_mutex_unlock(&(SrcRecord->ReceDataLock));
        DelEpollEvent(SrcRecord->SocketFd);
        DEBUG_INFO("client %s close",inet_ntoa(SrcRecord->ClientAddr.sin_addr));
		close(SrcRecord->SocketFd);
		free(SrcRecord);
	}
}

void DelEpollEvent(int SockFd)
{
    struct epoll_event ev;
    ev.data.fd = SockFd;
    ev.events=EPOLLIN|EPOLLET|EPOLLHUP;
    epoll_ctl(epfd, EPOLL_CTL_DEL, SockFd, &ev);
}

/*events可以是以下几个宏的集合：
EPOLLIN ：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）；
EPOLLOUT：表示对应的文件描述符可以写；
EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）；
EPOLLERR：表示对应的文件描述符发生错误；
EPOLLHUP：表示对应的文件描述符被挂断；
EPOLLET： 将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)来说的。
EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，
如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里 */
void AddEpollEvent(int SockFd)
{
    struct epoll_event ev;
    ev.data.fd = SockFd;
    ev.events=EPOLLIN|EPOLLET|EPOLLHUP;
    epoll_ctl(epfd, EPOLL_CTL_ADD, SockFd, &ev);
}

/*
设置 l_onoff 为非0，l_linger为非0，当套接口关闭时内核将拖延一段时间（由l_linger决定）如果套接口缓冲区中仍残留数据，进程将处于睡眠状态，
直到有数据发送完且被对方确认，之后进行正常的终止序列。
此种情况下,应用程序查close的返回是非常重要的，如果在数据发送完并被确认前时间到
close将返回EWOULDBLOCK错误且套接口发缓冲区中的任何数据都丢失
close的成功返回仅告诉我们发的数据（和FIN）已由对方TCP确认
它并不能告诉我们对方应用进程是否已读了数据,如果套接口设为非阻塞的，它将不等待close完成
*/
void SetLinger(const int SockFd, unsigned l_onoff)
{
	int z; /* Status code*/	
	struct linger so_linger;

	so_linger.l_onoff = l_onoff;
	so_linger.l_linger = 30;
	
	z = setsockopt(SockFd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof( so_linger));
	if (z) {
		perror("setsockopt(2)");
	}
}

int SocketSeverInit(int port)
{
	struct sockaddr_in serv_addr;
	int stat, tr = 1;

	if (socketRecordHead == NULL) {	
		SocketRecord_t *lsSocket = malloc(sizeof(SocketRecord_t));
		lsSocket->SocketFd = socket(AF_INET, SOCK_STREAM, 0);
		if (lsSocket->SocketFd < 0) {
			DEBUG_INFO("ERROR opening socket");
			return -1;
		}
		setsockopt(lsSocket->SocketFd, SOL_SOCKET, SO_REUSEADDR, &tr, sizeof(int));
		// Set the fd to none blocking
		fcntl(lsSocket->SocketFd, F_SETFL, O_NONBLOCK);
		bzero((char *) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(port);
		stat = bind(lsSocket->SocketFd, (struct sockaddr*) &serv_addr,sizeof(serv_addr));
		if (stat < 0) {
			DEBUG_INFO("ERROR on binding: %s\n", strerror(errno));
			return -1;
		}
		listen(lsSocket->SocketFd, 5);
		lsSocket->next = NULL;
		socketRecordHead = lsSocket;
        epfd = epoll_create(EPOLL_SIZE);
        if (epfd < 0)  {
           perror("epoll_create");
           exit(-1);
        }
        AddEpollEvent(lsSocket->SocketFd);
	}

	return 0;
}

void GetClientFds(int *fds, int maxFds)
{
	int recordCnt = 0;
	SocketRecord_t *SrcRecord;
	SrcRecord = socketRecordHead;
	while ((SrcRecord) && (recordCnt < maxFds)) {
		fds[recordCnt++] = SrcRecord->SocketFd;
		SrcRecord = SrcRecord->next;
	}
	return;
}

int GetNumClients(void)
{
	int recordCnt = 0;
	SocketRecord_t *SrcRecord;
	SrcRecord = socketRecordHead;
	if (SrcRecord == NULL) {
		return -1;
	}

	while (SrcRecord) {
		SrcRecord = SrcRecord->next;
		recordCnt++;
	}
	return (recordCnt);
}

SocketRecord_t * GetAuthBySockfd(int ClinetFd)
{
	SocketRecord_t * SrcRecord;
	SrcRecord = socketRecordHead;
	while (SrcRecord!=NULL) {
		if(SrcRecord->SocketFd == ClinetFd)
			return SrcRecord;
		else
			SrcRecord=SrcRecord->next;
	}
	return NULL;
}

/*接收文件body线程*/
void *RecvFileDataThread(void *arg)
{
     pthread_detach(pthread_self());
     #define RECVBUF_SIZE 65530
     SocketRecord_t *SocketRecord = (SocketRecord_t*)arg;
     pthread_mutex_lock(&(SocketRecord->ReceDataLock));
     SocketRecord->ReceFlag = 1;
     int size = SocketRecord->pFileHeadInfo->FileSize;
     char *mfd = SocketRecord->pFileHeadInfo->mmapfd;
     DEBUG_INFO("SocketRecord->SocketFd:%d",SocketRecord->SocketFd);
     int len = -1;
     while(size > 0){
         int len = recv(SocketRecord->SocketFd, mfd, RECVBUF_SIZE,0);
         if(len > 0){
            mfd+=len;
            size-=len;  
            //DEBUG_INFO("size=%d,len=%d",size,len);
            continue;
         }
         else if (len == 0){
            //1.客户端ctrl + c 终止进程
            //2.客户端close(fd)
            DEBUG_INFO("连接被对端关闭");
            SocketRecord->ReceFlag = 0;
            DeleteSocketRecord(SocketRecord->SocketFd);
            return;
        }
        else {
            if (errno == EAGAIN){
                DEBUG_INFO("EAGAIN");
                break;
            }

            if (errno == EINTR){
                DEBUG_INFO("EINTR");
                break;
            }

            else{
                DEBUG_INFO("size=%d,len=%d",size,len);
                SocketRecord->ReceFlag = 0;
                DeleteSocketRecord(SocketRecord->SocketFd);
                return;
            }
        }
    }
    DEBUG_INFO("size=%d,len=%d",size,len);
    SocketRecord->ReceFlag = 0;
    munmap((void*)SocketRecord->pFileHeadInfo->mmapfd, SocketRecord->pFileHeadInfo->FileSize);
    pthread_mutex_unlock(&(SocketRecord->ReceDataLock));
}

/*根据文件信息创建文件*/
void CreateClientFile(SocketRecord_t *SocketRecord)
{
    int fd = open(SocketRecord->FileDir,O_RDWR|O_CREAT|O_TRUNC,0664);
    if (fd < 0){
        perror("open");
        return;
    }
    ftruncate(fd,SocketRecord->pFileHeadInfo->FileSize);
    char *mmapfd = (char *)mmap(NULL, SocketRecord->pFileHeadInfo->FileSize, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
    if (mmapfd == NULL){
        perror("mmap");
    }
    SocketRecord->pFileHeadInfo->mmapfd = mmapfd;
    close(fd);
    return;    
}

void ReceiveParse(int ClinetFd)
{
    SocketRecord_t *SocketRecord = GetAuthBySockfd(ClinetFd);
    if (SocketRecord == NULL) {
        return;
    }

    //DEBUG_INFO("ClinetFd=%d",ClinetFd);
    if (SocketRecord->pFileHeadInfo != NULL){
        if (SocketRecord->ReceFlag == 0){
            pthread_t RecvFileDataThreadId;
            pthread_create(&RecvFileDataThreadId,NULL,RecvFileDataThread,SocketRecord);//创建新线程接收文件数据
            return;
        }
        else {
            return;
        }
    }

    unsigned char DataType = 0;
    int ret = recvfrom(ClinetFd,&DataType,sizeof(DataType),MSG_DONTWAIT/*非阻塞*/,NULL,0);
    if (ret < 0){
        perror("recvfrom");
        DEBUG_INFO("ret=%d",ret);
        return;
    }

    if(DataType == 0xfe){//文件头
        struct FileHead FileHeadInfo;
        ret = recvfrom(ClinetFd,&FileHeadInfo,sizeof(FileHeadInfo),MSG_DONTWAIT/*非阻塞*/,NULL,0);  
        if (ret < 0){
            perror("recvfrom");
            DEBUG_INFO("ret=%d",ret);
            return;
        }
        sprintf(SocketRecord->FileDir,"%s/%s/%s",FileDataBaseDir,inet_ntoa(SocketRecord->ClientAddr.sin_addr),FileHeadInfo.FileName);
        DEBUG_INFO("FileName=%s,FileSize=%d,FileDir=%s",FileHeadInfo.FileName,FileHeadInfo.FileSize,SocketRecord->FileDir);
        SocketRecord->pFileHeadInfo = (struct FileHead*)malloc(sizeof(struct FileHead));
        memcpy(SocketRecord->pFileHeadInfo,&FileHeadInfo,sizeof(FileHeadInfo));
        CreateClientFile(SocketRecord);
    }
}

void* SeverEpoll(void* userData)
{
	int nfds = -1;
	int i;
	struct epoll_event events[EVENT_SIZE];
   
	while(1) {
		/*-1永久阻塞*/
        nfds = epoll_wait(epfd, events, EVENT_SIZE, -1);
		for(i = 0; i < nfds; ++i) {
        	if (events[i].data.fd == socketRecordHead->SocketFd) {
                CreateSocketRecord();
			}
			else if(events[i].events & EPOLLIN) {
            	if (events[i].data.fd < 0) {
                    continue;
            	}
				ReceiveParse(events[i].data.fd);
			}
			else if(events[i].events & EPOLLHUP) {
                DEBUG_INFO("EPOLLHUP");
                DeleteSocketRecord(events[i].data.fd);
			}
			else if(events[i].events & EPOLLERR) {
				DEBUG_INFO("EPOLLERR");
			}
		}
	}
	
  return NULL;
}

int SeverSend(uint8_t* buf, int len, int ClientFd)
{
	int rtn;
	if (ClientFd) {
		int SendNum = 0;
		int SendLenth = len ;
		while(len) {
			rtn = sendto(ClientFd,buf,len,MSG_DONTWAIT,NULL,0);
			if (rtn <=0) {
				perror("SeverSend error ");
                /* EAGAIN : Resource temporarily unavailable*/
				if(errno == EINTR || errno == EAGAIN) { 
                    #if 0
					if(SendNum++ > 150) {
						DEBUG_INFO("Resource temporarily unavailable\n");
						break;
					}
					usleep(10*1000);
                    #endif
                    break;
				}
				else {
					DEBUG_INFO("Write error, close socket");
					DeleteSocketRecord(ClientFd);
					break;
				}
				rtn = 0;
			}
			len-=rtn;
		}
		return rtn;
	}
	return 0;
}

void CloseSocketFd(void)
{
	int fds[MAX_CLIENTS], idx = 0;
	GetClientFds(fds, MAX_CLIENTS);
    
	while (GetNumClients() > 1) {
		DeleteSocketRecord(fds[idx++]);
	}
	if (fds[0]) {
		close(fds[0]);
	}
}