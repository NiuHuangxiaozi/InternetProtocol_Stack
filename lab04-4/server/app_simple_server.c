//文件名: server/app_simple_server.c

//描述: 这是简单版本的服务器程序代码. 服务器首先连接到本地SIP进程. 然后它调用stcp_server_init()初始化STCP服务器. 
//它通过两次调用stcp_server_sock()和stcp_server_accept()创建2个套接字并等待来自客户端的连接. 服务器然后接收来自两个连接的客户端发送的短字符串. 
//最后, 服务器通过调用stcp_server_close()关闭套接字, 并断开与本地SIP进程的连接.

//输入: 无

//输出: STCP服务器状态

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "../common/constants.h"
#include "stcp_server.h"

//创建两个连接, 一个使用客户端端口号87和服务器端口号88. 另一个使用客户端端口号89和服务器端口号90.
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90
//在接收到字符串后, 等待15秒, 然后关闭连接.
#define WAITTIME 15

//这个函数连接到本地SIP进程的端口SIP_PORT. 如果TCP连接失败, 返回-1. 连接成功, 返回TCP套接字描述符, STCP将使用该描述符发送段.
int connectToSIP()
{
    //创建sockfd
    int server_sockfd= socket(PF_INET,SOCK_STREAM,0);
    assert(server_sockfd>=0);
    if(server_sockfd==-1)return -1;

    //BIND到指定的端口
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(SIP_PORT);

    int ser_bind_val=bind(server_sockfd,(const struct sockaddr *)(&addr),sizeof(addr));
    assert(ser_bind_val>=0);
    if(ser_bind_val<0)return -1;

    //对端口进行监听
    int ser_liston_val=listen(server_sockfd,10);
    assert(ser_liston_val>=0);
    if(ser_liston_val<0)return -1;

    //接受客户端套接字
    int addr_length = 0;
    int new_socket = accept(server_sockfd,(struct sockaddr *)&addr, (socklen_t *)&addr_length);
    assert(new_socket>=0);
    if(new_socket<0)return -1;

    return new_socket;
    //你需要编写这里的代码.
}

//这个函数断开到本地SIP进程的TCP连接. 
void disconnectToSIP(int sip_conn)
{
    close(sip_conn);
	//你需要编写这里的代码.
}

int main() {
	//用于丢包率的随机数种子
	srand(time(NULL));

	//连接到SIP进程并获得TCP套接字描述符
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("can not connect to the local SIP process\n");
	}

	//初始化STCP服务器
	stcp_server_init(sip_conn);

	//在端口SERVERPORT1上创建STCP服务器套接字 
	int sockfd= stcp_server_sock(SERVERPORT1);
	if(sockfd<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//监听并接受来自STCP客户端的连接 
	stcp_server_accept(sockfd);

	//在端口SERVERPORT2上创建另一个STCP服务器套接字
	int sockfd2= stcp_server_sock(SERVERPORT2);
	if(sockfd2<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//监听并接受来自STCP客户端的连接 
	stcp_server_accept(sockfd2);

	char buf1[6];
	char buf2[7];
	int i;
	//接收来自第一个连接的字符串
	for(i=0;i<5;i++) {
		stcp_server_recv(sockfd,buf1,6);
		printf("recv string: %s from connection 1\n",buf1);
	}
	//接收来自第二个连接的字符串
	for(i=0;i<5;i++) {
		stcp_server_recv(sockfd2,buf2,7);
		printf("recv string: %s from connection 2\n",buf2);
	}

	sleep(WAITTIME);

	//关闭STCP服务器 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				
	if(stcp_server_close(sockfd2)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				

	//断开与SIP进程之间的连接
	disconnectToSIP(sip_conn);
}
