//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程  

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../topology/topology.h"
#include "sip.h"

//defined in lab4-3
#include "sys/time.h"
//defined in lab4-3 end
/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 		//到重叠网络的连接

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT
//成功时返回连接描述符, 否则返回-1
int connectToSON()
{
    //创建sockfd
    int son_start_code= socket(AF_INET, SOCK_STREAM, 0);
    if(son_start_code<0)return -1;

    //建立连接
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(LOCAL_IP);
    servaddr.sin_port = htons(SON_PORT);

    int connect_state = connect(son_start_code,(void *)&servaddr, sizeof(servaddr));
    assert(connect_state>=0);
    if(connect_state<0)return -1;

    return son_start_code;
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间就发送一条路由更新报文
//在本实验中, 这个线程只广播空的路由更新报文给所有邻居, 
//我们通过设置SIP报文首部中的dest_nodeID为BROADCAST_NODEID来发送广播
void* routeupdate_daemon(void* arg) {
	//你需要编写这里的代码.
    sip_pkt_t pkt;
    while(1)
    {
        select(0,0,0,0,&(struct timeval){.tv_sec =ROUTEUPDATE_INTERVAL});
        //todo
		pkt.header.src_nodeID = topology_getMyNodeID();
        //广播路由信息
        son_sendpkt(BROADCAST_NODEID,&pkt,son_conn);
    }
    pthread_exit(NULL);
}

//这个线程处理来自SON进程的进入报文
//它通过调用son_recvpkt()接收来自SON进程的报文
//在本实验中, 这个线程在接收到报文后, 只是显示报文已接收到的信息, 并不处理报文 
void* pkthandler(void* arg) {
	sip_pkt_t pkt;

	while(son_recvpkt(&pkt,son_conn)>0) {
        printf("Routing: received a packet from neighbor %d\n", pkt.header.src_nodeID);
    }
	pthread_exit(NULL);
}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop()
{
    printf("Sip received SIGINT signal!\n");
    close(son_conn);
	//你需要编写这里的代码.
}

int main(int argc, char *argv[]) {
	printf("SIP layer is starting, please wait...\n");

	//初始化全局变量
	son_conn = -1;

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到重叠网络层SON
	son_conn = connectToSON();
	if(son_conn<0) {
		printf("can't connect to SON process\n");
		exit(1);		
	}
	
	//启动线程处理来自SON进程的进入报文
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("SIP layer is started...\n");

	//永久睡眠
	while(1) {
		sleep(60);
	}
}


