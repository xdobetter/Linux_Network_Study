//客户端代码
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <unistd.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <sys/time.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <pthread.h>  
#include <fcntl.h>  
#include <errno.h>  
#include <sys/stat.h>
#define SERV_PORT 8888
#define BUFFER_SIZE 4096 
typedef struct  
{  
    long long cur;  
    int size;   
}thread_data;  
int filefd;  
int file_block_size = 0;  
struct sockaddr_in server_address; 
void *sender(void *s)
{
    thread_data tdata = *((thread_data*)s);  
    long long cur = tdata.cur;  
    int size = tdata.size;    
    char buf[BUFFER_SIZE] = {0};  
    int read_count;  
    char head_buf[29] = {0};  
    snprintf(head_buf,29,"%016lld:%011d",cur,size);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)  
    {  
        perror("socket error");  
        exit(EXIT_SUCCESS);  
    }  

    if (connect(sockfd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0)  
    {   
        exit(EXIT_FAILURE);  
    }  
    send(sockfd, &head_buf, strlen(head_buf), 0);  
    long long  read_size = 0;  
    while (size)  
    { 
        read_count = pread(filefd, buf,(sizeof(buf) < size)?sizeof(buf):size, cur + read_size);  
        if (read_count < 0 && errno == EINTR)  
        {                           //pread 是原子操作 
            puts("break by signal");  
            continue;  
        }  
        else if (read_count == 0)  
        {  
            break;  
        }  
        else if(read_count < 0)  
        {  
            perror("pread"); exit(1);  
        }  
        send(sockfd, buf, read_count, 0);  
        size -= read_count;  
        read_size += read_count;  
    }  
    close(sockfd);  
    printf("cur = %lld, end\n",tdata.cur);  
    free(s);   

    pthread_exit(&sockfd);  
} 
int main(int argc, char *argv[])
{

    struct sockaddr_in servaddr;
    if (argc != 3)  
    {  
        fprintf(stderr, "please input: %s  N filename\n", argv[0]);  
        exit(1);  
    }  
    printf("sizeof(off_t) = %lu\n",sizeof(off_t)); //开启宏开关后 32系统该值为8字节  
    const int n = atoi(argv[1]);  
    const char* filename = argv[2];
    struct stat statbuf;  
    if (lstat(filename, &statbuf) < 0)  
    {  
        perror("lstat error");  
        exit(EXIT_FAILURE);  
    }
    long long file_len = (long long)statbuf.st_size;  
    printf("file len: %lld\n",file_len); 
    file_block_size=file_len/n;
    printf("file_block_size = %d\n", file_block_size);
    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)  
    {  
        perror("socket error");  
        exit(EXIT_SUCCESS);  
    }  
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, "10.76.7.141", &server_address.sin_addr.s_addr);
    server_address.sin_port = htons(SERV_PORT);
    if (connect(sockfd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0)  
    { 
        printf("connect error"); 
        exit(EXIT_FAILURE);  
    }  
    int thread_number=htonl(n); 
    int ret = send(sockfd, &thread_number, sizeof(thread_number), 0);  
    if (ret < 0)  
    {  
        perror("send error");  
        exit(EXIT_FAILURE);  
    }  
    close(sockfd);  
    struct timeval start, end;  
    gettimeofday(&start, NULL);
    pthread_t* tid = (pthread_t*)malloc(n * sizeof(pthread_t));
    //记得释放
    if ( (filefd = open(filename, O_RDONLY)) < 0)  
    {  
        perror("open error");  
        exit(EXIT_FAILURE);  
    }                                  //分块
    long long i;
    for ( i = 0; i < n; ++i)  
    {  

        thread_data* data = (thread_data*)malloc(sizeof(thread_data)); 
        if (i == n - 1 )  
        {  
            data->cur = (long long)i * file_block_size; 
            printf("data->cur = %lld\n",data->cur);  
            data->size = file_len-(long long)i * file_block_size;  
        }  
        else  
        {  
            data->cur = i * file_block_size;  
            data->size = file_block_size;  
        }  
        pthread_create(&tid[i], NULL, sender, (void*)data);  
    }
    for(i=0;i<n;i++)
    {

        void* ret;  
        pthread_join(tid[i], &ret);  
        printf("the thread  %d connecttion socket finished sending\n", i); 
    }
    close(filefd);  
    free(tid); 
    gettimeofday(&end, NULL);  
    double timeuse = 1000000*(end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;  
    timeuse /= 1000000;  
    printf("run time = %f\n", timeuse);  

    exit(EXIT_SUCCESS); 
}
