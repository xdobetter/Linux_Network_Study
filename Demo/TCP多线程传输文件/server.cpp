#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<string.h> //bzero
#include<unistd.h> //close

#define FILE_NAME_MAX_SIZE 512 
char buf[1024];//缓冲区
int ad[10];//最大连接数
const int port = 8085;
const char* ip="127.0.0.1";
 
void *thread_fun1(void *arg)
{
    //while(1)
     //    {
    //         memset(buf, '\0', sizeof(buf));//清空buf
    //         ssize_t _s=read(ad[(int)arg],buf,sizeof(buf)-1);
    //         if(_s>0)
    //         {
    //            buf[_s]=0;
    //            printf("client# %s\n",buf);   
    //         }
    //         else
    //         {
    //            printf("client is quit!\n");
    //            break;
    //         }
    //         write(ad[(int)arg],buf,strlen(buf));
    //     }
    // return NULL;
    while(1)
    {
 
    
        if (recv(ad[*((int*)arg)], buf, 1024, 0) < 0)
		{
			printf("Server Recieve Data Failed:");
			break;
		}
 
		// 然后从buffer(缓冲区)拷贝到file_name中 
		char file_name[FILE_NAME_MAX_SIZE + 1];
		bzero(file_name, FILE_NAME_MAX_SIZE + 1);
		strncpy(file_name, buf, strlen(buf)>FILE_NAME_MAX_SIZE ? FILE_NAME_MAX_SIZE : strlen(buf));
		printf("%s\n", file_name);
		// 打开文件并读取文件数据 
		FILE *fp = fopen(file_name, "r");
		if (NULL == fp)
		{
			printf("File:%s Not Found\n", file_name);
		}
		else
		{
 
			bzero(buf,1024);
			int length = 0;
			// 每读取一段数据，便将其发送给客户端，循环直到文件读完为止 
			while ((length = fread(buf, sizeof(char), 1024, fp)) > 0)
			{
				if (send(ad[*((int*)arg)], buf, length, 0) < 0)
				{
					printf("Send File:%s Failed./n", file_name);
					break;
				}
				bzero(buf, 1024);
			}
			// 关闭文件 
			fclose(fp);
			printf("File:%s Transfer Successful!\n", file_name);
		}
		// 关闭与客户端的连接 
		//close(ad[*((int*)arg)]);
    }   
}
 
// void *thread_fun2(void *arg)
// {
//     while(1)
//         {
//             memset(buf, '\0', sizeof(buf));//清空buf
//             ssize_t _s=write(ad[(int)arg],buf,strlen(buf));
//             if(_s>0)
//             {
//                buf[_s]=0;
//                printf("client# %s\n",buf);   
//             }
//             else
//             {
//                printf("client is quit!\n");
//                break;
//             }
//         }
//     return NULL;
// }
 
int main(int argc,char* argv[])
{
    
 
    int sock=socket(AF_INET,SOCK_STREAM,0);//打开一个网络通信窗口
    if(sock<0)
    {
        perror("socket");
        exit(1);
    }
 
    struct sockaddr_in local;//服务端
    local.sin_family=AF_INET;   //地址族
    local.sin_port=htons(port);//16位TCP/UDP端口号
    //local.sin_addr.s_addr=inet_addr(ip);//32位IP地址
    local.sin_addr.s_addr=htonl(INADDR_ANY);//默认采用本机的IP地址
    socklen_t len=sizeof(local);
 
   if(bind(sock,(struct sockaddr*)&local,len)<0)//绑定一个固定的网络地址和端口号
   {
     perror("bind");
     exit(2);
    }
 
    if(listen(sock,5)<0)//导致套接口从closed状态换到listen状态
    {
        perror("listen");
        exit(3);
    }
 
    pthread_t tid[10];
    int err;
    int i=0;
    //socklen_t len=sizeof(struct sockaddr_in);
 
    while(1)
    {
        struct sockaddr_in remote;//用于客户端的socket定义和赋值
        socklen_t remote_len = sizeof(remote);
        ad[i]=accept(sock,(struct sockaddr*)&remote,&remote_len);
        if(ad[i]<0)
        {
            perror("accept");
            continue;
        }
        // printf("client,ip:%s,port:%d\n",inet_ntoa(remote.sin_addr)
        //        ,ntohs(remote.sin_port));
        printf("pthread_create start!");
        err=pthread_create(&tid[i],NULL,thread_fun1,(void *)i);
        printf("pthread_create end!");
        //err=pthread_create(&tid[i],NULL,thread_fun2,(void *)i);
        if(err !=0)
        {
            printf("create new thread failed\n");
            close(ad[i]);
        }
        i++;
    }  
    close(sock);  
    return 0;
}