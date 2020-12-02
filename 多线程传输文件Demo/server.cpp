//服务端代码
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
#define MAX_LISTEN 2048 
typedef struct  
{  
    int socket;  
    int i;   
}thread_data;
int filefd; 
long long ato_ll(const char* p_str) 
{  

    long long result = 0;  
    long long mult = 1;  
    unsigned int len = strlen(p_str);
    unsigned int i;  

    for (i=0; i<len; ++i)  
    {  
        char the_char = p_str[len-(i+1)];  
        long long val;  
        if (the_char < '0' || the_char > '9')  
        {  
            return 0;  
        }  
        val = the_char - '0';  
        val *= mult;  
        result += val;  
        mult *= 10;  
    }  
    return result;  
}  
void* receive(void* s)  
{  
    thread_data tdata = *((thread_data*)s);  
    int sock=tdata.socket;
    int thread_order=tdata.i; 
    printf("thread_order = %d\n",thread_order);  
    char buf[BUFFER_SIZE];   
    char head_buf[29] = {0};  
    int ret = recv(sock, head_buf, sizeof(head_buf) - 1, MSG_WAITALL);   
    if (ret < 0)  
    {  
        fprintf(stderr, "thread %d recv error number %d: %s\n",  
            thread_order, errno, strerror(errno));  
        exit(EXIT_FAILURE);  
    }  
    char* cur_ptr = head_buf;  
    char* bk = strchr(head_buf,':');  
    if(bk!=NULL)  
    {  
        *bk = '\0';  
    }  
    char* size_ptr = bk + 1;  

    long long cur = ato_ll(cur_ptr);  
    int size = atoi(size_ptr);  
    printf("thread %d cur = %lld size = %d\n",thread_order,cur,size);  
    while (size)  
    {  
        ret = read(sock, buf, BUFFER_SIZE);  
        if (ret < 0 && errno ==EINTR)  
        {  
            puts("break by signal");  
            continue;  
        }  
        else if (ret == 0)  
        {  
            break;  
        }  
        else if(ret < 0)  
        {  
            perror("read");  
            exit(1);  
        }  
        if(pwrite(filefd, buf, ret, cur) < 0)  
        {  
            perror("pwrite");  
            exit(1);  
        }  
        cur += ret;  
        size -= ret;  
    }  

    close(sock);  
    fprintf(stderr, "thread %d finished receiving\n", thread_order);  
    free(s);  
    pthread_exit(&thread_order);  
}  

int main(void)
{
    struct sockaddr_in server_address;  
    memset(&server_address, 0, sizeof(server_address));  
    server_address.sin_family = AF_INET;  
    server_address.sin_port = htons(SERV_PORT);  
    server_address.sin_addr.s_addr = INADDR_ANY;  
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);  
    if (listenfd < 0)  
    {  
        perror("socket error");  
        exit(EXIT_FAILURE);  
    }  

    int reuse = 1;   
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)  
    {  
        perror("setsockopt SO_REUSEADDR error");  
        exit(EXIT_FAILURE);  
    }  

    if (bind(listenfd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0)  
    {  
        perror("bind error");  
        exit(EXIT_FAILURE);  
    }  

    if (listen(listenfd, MAX_LISTEN) < 0) 
    {  
        perror("listen error");  
        exit(EXIT_FAILURE);  
    }  
    else  
    {  
        printf("listen success\n");  
    }  
    int connfd = accept(listenfd, NULL, NULL);  
    if (connfd < 0)  
    {  
        perror("accept error");  
        exit(EXIT_FAILURE);  
    }
    int thread_number = 0;  
    int ret = recv(connfd, &thread_number, sizeof(thread_number), MSG_WAITALL);  
    close(connfd);
    if (ret < 0)  
    {  
        perror("recv MSG_WAITALL error");  
        exit(EXIT_FAILURE);  
    }  
    thread_number = ntohl(thread_number); 
    printf("thread_number = %d\n",thread_number);
    if ( (filefd = open("receive_file", O_WRONLY | O_CREAT |O_TRUNC, 0777)) < 0)  
    {  
        perror("open error");  
        exit(EXIT_FAILURE);  
    }  
    else  
    {  
        printf("open success\n");  
    } 
    struct timeval start, end;  
    gettimeofday(&start, NULL);
    pthread_t* tid = (pthread_t*)malloc(thread_number * sizeof(pthread_t));  
    if(tid==NULL)  
    {  
        perror("malloc::");  
        exit(1);  
    }  
    printf("thread_number = %d\n",thread_number);  
    int i=0;
    for ( i = 0; i < thread_number; ++i)  
    {  
        connfd = accept(listenfd, NULL, NULL);  
        if (connfd < 0)  
        {  
            perror("accept error");  
            exit(EXIT_FAILURE);  
        }  
        thread_data* data = (thread_data*)malloc(sizeof(thread_data));
        data->socket=connfd;
        data->i=i; 
        pthread_create(&tid[i], NULL, receive, (void*)data);  

    } 
    for(i=0;i<thread_number;i++)
    {  
        char *ret;  
        pthread_join(tid[i], (void**)&ret);  
        printf("thread %d finished receiving\n", i);  
    }  
    close(connfd);
    close(listenfd); 
    free(tid);    
    gettimeofday(&end, NULL);  
    double timeuse = 1000000*(end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;  
    timeuse /= 1000000;  
    printf("run time = %f\n", timeuse);  

    exit(EXIT_SUCCESS); 
}