
/**
 * @file 5-3_my_testlisten.cpp
 * @author dobetter (db.xi@zju.edu.cn)
 * @brief 服务器端程序，这是一个测试backlog的大小对服务器连接状态的程序
 * 在监听队列中，处于ESTABLISHED状态的连接只有backlog+1个，其他的都处于SYN_RCVD状态
 * @version 0.1
 * @date 2020-11-17
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool stop = false;
static void handle_term(int sig)
{
    stop = true;
}
int main(int argc, char *argv[])
{
    signal(SIGTERM, handle_term);
    if (argc <= 3)
    {
        printf("usage: %s ip_address port_number backlog\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int backlog = atoi(argv[3]);
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_port = htons(port);
    int ret = bind(sock, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(sock, backlog);
    assert(ret != -1);
    while (!stop)
    {
        sleep(1);
    }
    close(sock);
    return 0;
}