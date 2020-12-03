 
#include<netinet/in.h>  // sockaddr_in 
#include<sys/types.h>  // socket 
#include<sys/socket.h>  // socket 
#include<stdio.h>    // printf 
#include<stdlib.h>    // exit 
#include<string.h>    // bzero 
#include<arpa/inet.h> //inet_pton
#include<unistd.h> //close

#define SERVER_PORT 8085
#define BUFFER_SIZE 1024 
#define FILE_NAME_MAX_SIZE 512 
 
int main()
{
	// 声明并初始化一个客户端的socket地址结构 
	struct sockaddr_in client_addr;
	//设置client_addr前size个字节为0
	bzero(&client_addr, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = htons(INADDR_ANY);
	client_addr.sin_port = htons(0);
	// 创建socket，若成功，返回socket描述符 
	int client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_socket_fd < 0)
	{
		perror("Create Socket Failed:");
		exit(1);
	}
	// 绑定客户端的socket和客户端的socket地址结构 非必需 
	if (-1 == (bind(client_socket_fd, (struct sockaddr*)&client_addr, sizeof(client_addr))))
	{
		perror("Client Bind Failed:");
		exit(1);
	}
	// 声明一个服务器端的socket地址结构，并用服务器那边的IP地址及端口对其进行初始化，用于后面的连接 
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	//将IP地址从字符串格式转换成网络地址格式
	if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) == 0)
	{
		perror("Server IP Address Error:");
		exit(1);
	}
	server_addr.sin_port = htons(SERVER_PORT);//将主机字节顺序转换为网络字节顺序
	//和int有点像用来表示长度
	socklen_t server_addr_length = sizeof(server_addr);
	// 向服务器发起连接，连接成功后client_socket_fd代表了客户端和服务器的一个socket连接 
	if (connect(client_socket_fd, (struct sockaddr*)&server_addr, server_addr_length) < 0)
	{
		perror("Can Not Connect To Server IP:");
		exit(0);
	}
 
	// 输入文件名 并放到缓冲区buffer中等待发送 
	char file_name[FILE_NAME_MAX_SIZE + 1];
	char file_name_save[FILE_NAME_MAX_SIZE + 1];
	bzero(file_name, FILE_NAME_MAX_SIZE + 1);
	printf("Please Input File Name On Server:\t");
	// scanf()函数返回成功赋值的数据项数，读到文件末尾出错时则返回EOF
	scanf("%s", file_name);
	char buffer[BUFFER_SIZE];
	bzero(buffer, BUFFER_SIZE);
	//把file_name所指由NULL结束的字符串的前n个字节复制到buffer所指的数组中。
	strncpy(buffer, file_name, strlen(file_name)>BUFFER_SIZE ? BUFFER_SIZE : strlen(file_name));
	// 向client_socket_fd发送buffer中的数据 
	if (send(client_socket_fd, buffer, BUFFER_SIZE, 0) < 0)
	{
		perror("Send File Name Failed:");
		exit(1);
	}
 
	//输入要保存的地址
	printf("Please Input File Name to Save:\t");
	scanf("%s", file_name_save);
	// 打开文件，准备写入 
	FILE *fp = fopen(file_name_save, "w");
	if (NULL == fp)
	{
		printf("File:\t%s Can Not Open To Write\n", file_name_save);
		exit(1);
	}
 
	// 从服务器接收数据到buffer中 
	// 每接收一段数据，便将其写入文件中，循环直到文件接收完并写完为止 
	bzero(buffer, BUFFER_SIZE);
	int length = 0;
	//不论是客户还是服务器应用程序都用recv函数从TCP连接的另一端接收数据
	//用buffer来存放recv函数接收到的数据
	while ((length = recv(client_socket_fd, buffer, BUFFER_SIZE, 0)) > 0)
	{
		//buffer所指的内容输出到fp这个文件中，
		if (fwrite(buffer, sizeof(char), length, fp) < length)
		{
			printf("File:\t%s Write Failed\n", file_name);
			break;
		}
		bzero(buffer, BUFFER_SIZE);
	}
	// 接收成功后，关闭文件，关闭socket 
	printf("Receive File:\t%s From Server IP Successful!\n", file_name);
	//close((int)fp);
	close(client_socket_fd);
	return 0;
}