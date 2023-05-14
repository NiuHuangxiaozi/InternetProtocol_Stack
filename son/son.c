//文件名: son/son.c
//
//描述: 这个文件实现SON进程 
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程. 
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中. 

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "son.h"
#include "../topology/topology.h"
#include "neighbortable.h"


//defined in lab4-3
#include "assert.h"
#include <sys/select.h>
//defined in lab4-3 end
//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 60

/**************************************************************/
//声明全局变量
/**************************************************************/

//将邻居表声明为一个全局变量 
nbr_entry_t *nt;
//将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn;

/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止. 
void *waitNbrs(void *arg) {
    //你需要编写这里的代码.
    //获取我自己的ID
    int myID = topology_getMyNodeID();
    int nt_length = topology_getNbrNum();

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    //BIND到指定的端口
    int addr_lenth = 0;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(CONNECTION_PORT);

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET,SO_REUSEADDR,(const void *)&opt, sizeof(opt));

    int bind_val = bind(sockfd, (const struct sockaddr *) (&addr), sizeof(addr));
    assert(bind_val >= 0);

    //对CONNECTION_PORT端口进行监听
    int liston_val = listen(sockfd, 10);
    assert(liston_val >= 0);

    //遍历邻居表等待接入
    for (int nbr_index = 0; nbr_index < nt_length; nbr_index++) 
    {
        if (nt[nbr_index].nodeID > myID)
        {

            fd_set fdset;                   // define fd set
            while(1)
            {
                FD_ZERO(&fdset);               // clear set to zero
                FD_SET(sockfd, &fdset); // add server socket fd to fdset
                int max_fd = sockfd+1; 

                int activity = select(max_fd, &fdset, NULL, NULL, NULL);
                if (activity < 0)// select被信号中断，继续调用select
                    continue;
                else if(activity == 0)//超时
                    continue;
                else
                {
                    if (FD_ISSET(sockfd, &fdset))
                    {
                            int new_socket;
                            // accept a client socket
                            if ((new_socket = accept(sockfd, 
                                (struct sockaddr *)&addr, (socklen_t *)&addr_lenth)) < 0)
                                {
                                    perror("accept error");
                                    exit(EXIT_FAILURE);
                                }
                            //接受客户端套接字,往邻居表里面填写新的port号码
                            nt[nbr_index].conn=new_socket;
                            break;
                    }
                }
            }
        }
    }
    pthread_exit(NULL);
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {
    //你需要编写这里的代码.
    //获得邻居的数目
    int myID = topology_getMyNodeID();
    int nt_length = topology_getNbrNum();

    for (int nbr_index = 0; nbr_index < nt_length; nbr_index++) {
        if (nt[nbr_index].nodeID < myID) {
            //创建sockfd
            int son_start_code = socket(AF_INET, SOCK_STREAM, 0);
            assert(son_start_code >= 0);
            if (son_start_code < 0)return -1;

            //建立连接
            struct sockaddr_in less_node_vaddr;
            less_node_vaddr.sin_family = AF_INET;
            less_node_vaddr.sin_addr.s_addr = nt[nbr_index].nodeIP;
            less_node_vaddr.sin_port = htons(CONNECTION_PORT);

            int connect_state = connect(son_start_code, (void *) &less_node_vaddr, sizeof(less_node_vaddr));
            assert(connect_state >= 0);
            if (connect_state < 0)return -1;
            //保存相应的sockfd的conn
            nt[nbr_index].conn = son_start_code;
        }
    }
    return 1;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的. 
void *listen_to_neighbor(void *arg) {
    //获得邻居表的下标，明确要监视的目标
    int nbr_index = *((int *) arg);
    //定义缓冲区
    sip_pkt_t other_node_sip_pkt;
    while (1) {
        int flag = recvpkt(&other_node_sip_pkt, nt[nbr_index].conn);
        if (flag == -1) {
            printf("Overlayer is disconnected!\n");
            nt[nbr_index].nodeID = -1;
            nt[nbr_index].nodeIP = -1;
            close(nt[nbr_index].conn);
            break;
        } else if (flag == 1) {
            printf("Node %d receive pkt \n", nt[nbr_index].nodeID);
            int son2sip_flag = forwardpktToSIP(&other_node_sip_pkt, sip_conn);
            assert(son2sip_flag == 1);
        }
    }
    pthread_exit(NULL);
}


//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP() {
    //你需要编写这里的代码.
    //创建sockfd
    int server_sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(server_sockfd >= 0);
    if (server_sockfd < 0) {
        printf("SON socket failed!\n");
        return;
    }
    //BIND到指定的端口
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SON_PORT);

    int ser_bind_val = bind(server_sockfd, (const struct sockaddr *) (&addr), sizeof(addr));
    assert(ser_bind_val >= 0);
    if (ser_bind_val < 0) {
        printf("SON bind failed!\n");
        return;
    }

    //对端口进行监听
    int ser_listen_val = listen(server_sockfd, 10);
    assert(ser_listen_val >= 0);
    if (ser_listen_val < 0) {
        printf("SON listen failed!\n");
        return;
    }

    //接受客户端套接字
    int addr_length = 0;
    int new_socket = accept(server_sockfd, (struct sockaddr *) &addr, (socklen_t *) &addr_length);
    assert(new_socket >= 0);
    if (new_socket < 0)
    {
        printf("SON accept failed!\n");
        return;
    }
    //给sip和son之间的套接字变量赋值
    sip_conn=new_socket;

    //
    sip_pkt_t sip2son_pkt;
    int nextNode=-1;
    while(1)
    {
        int sip2son_flag=getpktToSend(&sip2son_pkt,&nextNode,sip_conn);
        assert(sip2son_flag==1);

        //发送报文
        if(nextNode==BROADCAST_NODEID)//群发
        {
            //获得邻居数
            int nbrnum=topology_getNbrNum();

            //循环遍历群发
            for(int nbr_index=0;nbr_index<nbrnum;nbr_index++)
            {
                if (nt[nbr_index].nodeID > 0)
                {
                    int sendflag=sendpkt(&sip2son_pkt,nt[nbr_index].conn);
                    assert(sendflag==1);
                }
            }
        }
        else//单点发送
        {

        }
    }
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop()
{
    printf("Son stop will free all resources!\n");
    int nbrnum=topology_getNbrNum();
    //循环遍历群发
    for(int nbr_index=0;nbr_index<nbrnum;nbr_index++)
    {
        close(nt[nbr_index].conn);
    }
    free(nt);
    close(sip_conn);
    sip_conn=-1;
}

int main() {
    //启动重叠网络初始化工作
    printf("Overlay network: Node %d initializing...\n", topology_getMyNodeID());

    //创建一个邻居表
    nt = nt_create();
    //将sip_conn初始化为-1, 即还未与SIP进程连接
    sip_conn = -1;

    //注册一个信号句柄, 用于终止进程
    signal(SIGINT, son_stop);

    //打印所有邻居
    int nbrNum = topology_getNbrNum();
    int i;
    for (i = 0; i < nbrNum; i++) {
        printf("Overlay network: neighbor %d:%d\n", i + 1, nt[i].nodeID);
    }

    //启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
    pthread_t waitNbrs_thread;
    pthread_create(&waitNbrs_thread, NULL, waitNbrs, (void *) 0);

    //等待其他节点启动
    sleep(SON_START_DELAY);

    //连接到节点ID比自己小的所有邻居
    connectNbrs();

    //等待waitNbrs线程返回
    pthread_join(waitNbrs_thread, NULL);

    //此时, 所有与邻居之间的连接都建立好了

    //创建线程监听所有邻居
    for (i = 0; i < nbrNum; i++) {
        int *idx = (int *) malloc(sizeof(int));
        *idx = i;
        pthread_t nbr_listen_thread;
        pthread_create(&nbr_listen_thread, NULL, listen_to_neighbor, (void *) idx);
    }
    printf("Overlay network: node initialized...\n");
    printf("Overlay network: waiting for connection from SIP process...\n");

    //等待来自SIP进程的连接
    waitSIP();
}
