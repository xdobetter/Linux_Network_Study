#include "client.h"

int  sockefd;
#define PORT 9090
#define SERVER_IP "127.0.0.1"
//文件映射地址
char *mmapfd = NULL;
//创建文件
#define FILEDIR "../file"
#define FILESIZE 512*1024*1024

void SocketInit(void)
{
    struct sockaddr_in cliaddr;
     /* 创建 socket */
    if ((sockefd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    //初始化服务器的ip地址与端口
    memset(&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    cliaddr.sin_port = htons(PORT);

    /* connect server  */
    if (connect(sockefd, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
}

void* ClientPollThread(void *arg)
{
    struct pollfd poll_fd[1];
	poll_fd[0].fd = sockefd;
    poll_fd[0].events = POLLIN;
    int i = 0;
    int ret = -1;

    while(1) {
        int ret = poll(poll_fd,1,-1);
        if (ret < 0){
            perror("poll");
            continue;
        }
         
        if (ret == 0){
            DEBUG_INFO("poll timeout!\n");
            continue;
        }

		if (poll_fd[0].revents == POLLIN){
			unsigned char recv_buf[128] = {0};
			//ret = recvfrom(sockefd,&recv_buf,sizeof(recv_buf),MSG_DONTWAIT/*非阻塞*/,NULL,0); 
			//DEBUG_INFO("client recv len=%d,recv_buf=%s",ret,recv_buf);
		}
    }
}

//发送文件信息
void SendFileHeadInfo(const char* FileName,int Filesize)
{
    unsigned char type = 0xfe;//文件头type
    struct FileHead FileHeadInfo;
    strncpy(FileHeadInfo.FileName,FileName,strlen(FileName));
    FileHeadInfo.FileSize = Filesize;
    char *buf = (char *)malloc(sizeof(type)+sizeof(FileHeadInfo));
    memcpy(buf,&type,sizeof(type));
    memcpy(buf+sizeof(type),&FileHeadInfo,sizeof(FileHeadInfo));
    int ret = send(sockefd,buf,sizeof(type)+sizeof(FileHeadInfo),0);
    DEBUG_INFO("ret=%d",ret);
}

//发送文件body信息
void SendFileBody(void)
{
    //发送文件数据
    #define SEND_SIZE 1024
    int size = FILESIZE;
    char *mfd = mmapfd;
    while(size > 0){
        int len = send(sockefd,mfd,SEND_SIZE,0);
        //DEBUG_INFO("len=%d",len);
        if (len > 0){
           mfd+=len; 
           size-=len;
        }
        else{
            DEBUG_INFO("send file body failed");
            usleep(10*1000);
        }
    }

    DEBUG_INFO("size=%d",size);
    int ret = munmap(mmapfd,FILESIZE);
    if (ret < 0){
        perror("munmap");
        DEBUG_INFO("munmap failed");
        return;
    }
}

void CrateFile(void)
{
    time_t cur_time;
    struct tm *local_time;
    cur_time = time(NULL);
    local_time = localtime(&cur_time);
    char FileName[64] = {0};
    sprintf(FileName, "%d%d%d_%d%d%d", local_time->tm_year+1900,local_time->tm_mon+1,local_time->tm_mday,local_time->tm_hour,local_time->tm_min,local_time->tm_sec); 
    
    char FileDirName[128] = {0};
    sprintf(FileDirName,"%s/%s",FILEDIR,FileName);
    int fd = open(FileDirName,O_RDWR|O_CREAT|O_TRUNC,0664);
    if(fd <0){
        perror("open");
        DEBUG_INFO("open file failed 1");
        return;
    }
    #if 1
    int pos = lseek(fd,FILESIZE-1,SEEK_SET);
    DEBUG_INFO("pos=%d",pos);
    write(fd,"F",1);
    #endif
    #if 0
    ftruncate(fd,FILESIZE);
    #endif
    #if 0
    int i = 0;
    for(i; i < FILESIZE;i++){
        if (i == 0) {
           write(fd,"A",1); 
           continue;
        }
        if(i == FILESIZE-1){
           write(fd,"Z",1); 
           break;
        }
        write(fd,"F",1);
    }
    close(fd);
    #endif

    //把文件映map映射到内存
    fd = open(FileDirName,O_RDWR);
    if(fd < 0){
        perror("open");
        DEBUG_INFO("open file failed 2");
        return;
    }
    struct stat filestat;
	fstat(fd ,&filestat);
    DEBUG_INFO("%ld,%d",filestat.st_size,FILESIZE);
    mmapfd =(char *) mmap(NULL, filestat.st_size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
    if (mmapfd == NULL){
        perror("mmap");
        DEBUG_INFO("mmap file failed");
        return;
    }
    close(fd);

    SendFileHeadInfo(FileName,FILESIZE);
}

void exit_client(void)
{
    close(sockefd);
    //exit(0);
}

void HelpCmd(void)
{
    #define CMD_BUF_SIZE 1024*1024*4
    fprintf (stderr, 
    "---------------------------------------------\n"
    "----0 connect sever--------------------------\n"
    "----1 create file and send file head info----\n"
	"----2 send file body-------------------------\n"
	"----3 exit client----------------------------\n"
	"---------------------------------------------\n"
	, CMD_BUF_SIZE);
}

void ConnecServer(void)
{
    SocketInit();
}

int main(void)
{
    pthread_t ClientPollThreadId;
    pthread_create(&ClientPollThreadId,NULL,ClientPollThread,NULL);
    while(1){
        int num = -1;
        HelpCmd();
        scanf("%d",&num);
        if (num == 0){//连接server
            ConnecServer();
        }
        else if (num == 1){//创建文件并发送文件头信息
            CrateFile();
        }
        else if(num == 2){//发送文件body部分
            SendFileBody();
        }
        else if(num == 3){//关闭连接
           exit_client(); 
        }
    }
    return 0;
}