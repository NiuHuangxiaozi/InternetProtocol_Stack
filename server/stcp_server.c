// 文件名: stcp_server.c
//
// 描述: 这个文件包含服务器STCP套接字接口定义. 你需要实现所有这些接口.

#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "stcp_server.h"
#include "../common/constants.h"



//Defined by Niu in lab4-1
#include "unistd.h"
#include "assert.h"
#include "string.h"

server_tcb_t ** server_tcbs=NULL;//服务器的tcb表
int s_sockfd=0; //服务器的son层套接字文件描述符
pthread_t seghandler_tid; //服务器seghandler的线程

extern int print_index;//打印的标号
//Defined by Niu in lab4-1 end




/*面向应用层的接口*/
//
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//

// stcp服务器初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量,
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//

void stcp_server_init(int conn)
{
    //初始化服务器的tcb数组
    server_tcbs=(server_tcb_t**)malloc(MAX_TRANSPORT_CONNECTIONS*sizeof(server_tcb_t*));
    for(int tcb_index=0;tcb_index<MAX_TRANSPORT_CONNECTIONS;tcb_index++)
        server_tcbs[tcb_index]=NULL;

    //保存重叠SON的套接字描述符
    s_sockfd=conn;

    int seghandler_err= pthread_create(&seghandler_tid,NULL,seghandler,NULL);
    pthread_detach(seghandler_tid);
}

// 创建服务器套接字
//
// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port.
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接.
// 如果TCB表中没有条目可用, 这个函数返回-1.

int stcp_server_sock(unsigned int server_port)
{
    //1找寻空闲的TCB条目并创建相应的tcb
    int new_tcb = tcbtable_newtcb(server_port);
    if(new_tcb<0)//失败返回-1
        return -1;
    //获得相应的tcb指针
    server_tcb_t * s_tcb = tcbtable_gettcb(new_tcb);

    //3设置tcb的内容
    s_tcb->client_nodeID =-1;
    s_tcb->server_nodeID = -1;

    s_tcb->expect_seqNum = 0;
    s_tcb->state = CLOSED;

    //指向接收缓冲区的指针
    s_tcb->recvBuf=(char *)malloc(sizeof(char)*RECEIVE_BUF_SIZE);
    memset(s_tcb->recvBuf,0,sizeof(char)*RECEIVE_BUF_SIZE);

    //接收缓冲区中已接收数据的大小
    s_tcb->usedBufLen=0;

    //指向一个互斥量的指针, 该互斥量用于对接收缓冲区的访问
    //为发送缓冲区创建互斥量
    pthread_mutex_t* recvBuf_mutex;
    recvBuf_mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    assert(recvBuf_mutex!=NULL);
    pthread_mutex_init(recvBuf_mutex,NULL);
    s_tcb->bufMutex = recvBuf_mutex;

    return new_tcb;
}

// 接受来自STCP客户端的连接
//
// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后进入忙等待(busy wait)直到TCB状态转换为CONNECTED
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//

int stcp_server_accept(int sockfd)
{
    //获得tcb的指针
    server_tcb_t* tcb=server_tcbs[sockfd];
    //转为listening
    tcb->state=LISTENING;
    //进入忙等待过程
    printf("[%d] | Server enter LISTENING .The listen port is %d\n",print_index,tcb->server_portNum);
    increase_print_index();
    while(1)
    {
        //usleep(ACCEPT_POLLING_INTERVAL);
        if(tcb->state==CONNECTED)//连接已经建立成功
            break;
    }
    return 1;
}

// 接收来自STCP客户端的数据. 请回忆STCP使用的是单向传输, 数据从客户端发送到服务器端.
// 信号/控制信息(如SYN, SYNACK等)则是双向传递. 这个函数每隔RECVBUF_ROLLING_INTERVAL时间
// 就查询接收缓冲区, 直到等待的数据到达, 它然后存储数据并返回1. 如果这个函数失败, 则返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length)
{
    //寻找相应的tcb
    int recvd_length=0;
    server_tcb_t * tcb= tcbtable_gettcb(sockfd);
    if(!tcb)return -1;

    //循环等待，知道收集到所有的字符才返回。
    while(recvd_length<length)
    {
        if(tcb->usedBufLen>=length)
        {
            recvBuf_copyToClient(tcb,buf,length);
            break;
        }
        sleep(RECVBUF_POLLING_INTERVAL);
    }
    return 1;
}

// 关闭STCP服务器
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//

int stcp_server_close(int sockfd)
{
    if(server_tcbs[sockfd]->state!=CLOSED)
        return -1;
    else
    {
        free(server_tcbs[sockfd]->bufMutex);
        free(server_tcbs[sockfd]->recvBuf);
        free(server_tcbs[sockfd]);
        server_tcbs[sockfd]=NULL;
        return 1;
    }
}

// 处理进入段的线程
//
// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环,
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//

void *seghandler(void* arg)
{
    seg_t  recv_seg;
    while(1)
    {
        int recv_flag=sip_recvseg(s_sockfd,&recv_seg);
        if(recv_flag==-1)//重叠网络层断开
        {
            printf("[%d] | SON disconnected seghandler exit!\n",print_index);
            increase_print_index();
            break;
        }
        else if(recv_flag==0)//包丢了
        {
            printf("[%d] | The package be throwed out!\n",print_index);
            increase_print_index();
            //do nothing
        }
        else if(recv_flag==1)//checksum计算错误
        {
            printf("[%d] | The package checksum error!\n",print_index);
            increase_print_index();
        }
        else if(recv_flag==2)//收到了包
        {
            printf("The package Received!\n");
            increase_print_index();
            print_seg(&recv_seg);

            action(&recv_seg);
        }
    }
    pthread_exit(NULL);
    return 0;
}


//Defined by Niu in lab4-1
void Initial_stcp_control_seg(server_tcb_t * tcb,int Signal,seg_t  *syn)
{
    //构造其他必须信息
    syn->header.src_port=tcb->server_portNum; //源端口
    syn->header.dest_port=tcb->client_portNum;//目标的端口号

    switch(Signal) { //type
        case SYN:
            break;
        case SYNACK:
            syn->header.type = SYNACK;
            syn->header.length=0;
            break;
        case FIN:
            break;
        case FINACK:
            syn->header.type = FINACK;
            syn->header.length=0;
            break;
        case DATA:
            break;
        case DATAACK:
            syn->header.type = DATAACK;
            syn->header.seq_num=-1;        //序号
            syn->header.ack_num=tcb->expect_seqNum;  //确认号
            syn->header.length=0;
            break;
        default:
            break;
    }
}

void action(seg_t  *seg)
{
    //找寻相应的sockfd对应下标，得到指针。
    server_tcb_t * tcb=tcbtable_recv_gettcb(seg);
    if(!tcb)return;
    //定义可能要用的计时线程
    pthread_t * fin_tid=NULL;
    if(tcb->state==CONNECTED&&seg->header.type==FIN)
    {
        fin_tid=(pthread_t*) malloc(sizeof(pthread_t));
    }
    //定义要发送的报文
    seg_t server_seg;
    //根据对方发来的报文执行相应的动作
    switch(tcb->state)
    {
        case CLOSED://do nothing
            break;
        case LISTENING:
            if (seg->header.type == SYN) {
                //0设置一些客户端报文的信息到服务器段的tcb当中
                tcb->client_portNum=seg->header.src_port;//port
                tcb->expect_seqNum=seg->header.seq_num;//设置所期望的下一个序号。
                //1
                printf("[%d] | LISTENING :Server receive syn. Then send synback\n",print_index);
                increase_print_index();
                //2
                Initial_stcp_control_seg(tcb, SYNACK, &server_seg);

                sip_sendseg(s_sockfd, &server_seg);
                //3
                tcb->state = CONNECTED;
            }
            break;
        case CONNECTED:
            switch (seg->header.type)
            {
                case SYN:
                    printf("[%d] | CONNECTED :Server receive syn. Then send synback\n",print_index);
                    increase_print_index();


                    Initial_stcp_control_seg(tcb, SYNACK, &server_seg);
                    sip_sendseg(s_sockfd, &server_seg);
                    break;
                case FIN:
                    printf("[%d] | CONNECTED :Server receive fin. Then send finback\n",print_index);
                    increase_print_index();

                    Initial_stcp_control_seg(tcb, FINACK, &server_seg);
                    sip_sendseg(s_sockfd, &server_seg);
                    tcb->state = CLOSEWAIT;

                    //开启一个计时线程
                    int seghandler_err= pthread_create(fin_tid,
                                                       NULL,close_wait_handler,tcb);
                    assert(seghandler_err==0);
                    pthread_detach(*fin_tid);
                    break;
                case DATA:
                    //超出recv的大小，默默丢弃
                    if(seg->header.length+tcb->usedBufLen>RECEIVE_BUF_SIZE)
                    {
                        break;
                    }
                    //序号相等
                    else if(seg->header.seq_num==tcb->expect_seqNum)
                    {
                        //拷贝相关的data和更新usedlen
                        recvBuf_recv(tcb,seg);

                        //发送报文DATAACK，新ACKNUM
                        Initial_stcp_control_seg(tcb, DATAACK, &server_seg);
                        sip_sendseg(s_sockfd, &server_seg);
                    }
                    else if(seg->header.seq_num!=tcb->expect_seqNum)
                    {
                        //发送报文DATAACK，旧ACKNUM
                        Initial_stcp_control_seg(tcb, DATAACK, &server_seg);
                        sip_sendseg(s_sockfd, &server_seg);
                    }
                    break;
            }
            break;
        case CLOSEWAIT:
            if (seg->header.type == FIN)
            {
                printf("[%d] | CLOSEWAIT :Server receive fin. Then send finback\n",print_index);
                increase_print_index();

                Initial_stcp_control_seg(tcb, FINACK, &server_seg);
                sip_sendseg(s_sockfd, &server_seg);
            }
            break;
        default:
            break;
    }
}

void *close_wait_handler(void* arg)
{
    server_tcb_t * tcb=(server_tcb_t*)arg;
    sleep(CLOSEWAIT_TIMEOUT);
    tcb->state=CLOSED;

    tcb->usedBufLen=0;//lab4-2

    printf("[%d] | CLOSEWAIT :Close_wait_timeout.Turn to CLOSED!\n",print_index);
    increase_print_index();

    pthread_exit(NULL);
}

//返回新的tcb的索引号, 如果tcbtable中所有的tcb都已使用了或给定的端口号已被使用, 就返回-1.
int tcbtable_newtcb(unsigned int port)
{
    int i;

    for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++) {
        if(server_tcbs[i]!=NULL&&server_tcbs[i]->server_portNum==port) {
            return -1;
        }
    }

    for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++)
    {
        if(server_tcbs[i]==NULL)
        {
            server_tcbs[i] = (server_tcb_t *) malloc(sizeof(server_tcb_t));
            server_tcbs[i]->server_portNum = port;
            return i;
        }
    }
    return -1;
}

//通过给定的套接字获得tcb.
//如果没有找到tcb, 返回0.
server_tcb_t * tcbtable_gettcb(int sockfd)
{
    if(server_tcbs[sockfd]!=NULL)
        return server_tcbs[sockfd];
    else
        return 0;
}
//在受到相应的段之后，根据段中的一些信息来获取对应的的服务器tcb号
server_tcb_t  *tcbtable_recv_gettcb(seg_t * seg)
{
    int server_tcb_index=-1;
    for(int tcb_index=0;tcb_index<MAX_TRANSPORT_CONNECTIONS;tcb_index++)
    {
        if(seg->header.type==SYN)//半连接状态
        {
            if (server_tcbs[tcb_index] != NULL && \
                server_tcbs[tcb_index]->server_portNum == seg->header.dest_port)
            {
                server_tcb_index = tcb_index;
                break;
            }
        }
        else //全连接状态
        {
            if (server_tcbs[tcb_index] != NULL && \
                server_tcbs[tcb_index]->server_portNum == seg->header.dest_port&&\
                server_tcbs[tcb_index]->client_portNum == seg->header.src_port
                    )
            {
                server_tcb_index = tcb_index;
                break;
            }
        }
    }

    if(server_tcb_index<0)
        return NULL;
    server_tcb_t *tcb=server_tcbs[server_tcb_index];
    return tcb;
}
//Defined by Niu in lab4-1 End


//Defined by Niu in lab4-2
void recvBuf_recv(server_tcb_t * tcb,seg_t *seg)
{
    pthread_mutex_lock(tcb->bufMutex);

    //拷贝到缓冲区
    memcpy(tcb->recvBuf+tcb->usedBufLen,seg->data,seg->header.length);
    //更新参数
    tcb->usedBufLen+=seg->header.length;
    tcb->expect_seqNum+=seg->header.length;

    pthread_mutex_unlock(tcb->bufMutex);
}


void recvBuf_copyToClient(server_tcb_t  * tcb,void * buf,unsigned int length)
{
    pthread_mutex_lock(tcb->bufMutex);
    //copy
    memcpy(buf,tcb->recvBuf,sizeof(char)*length);
    //前移
    memcpy(tcb->recvBuf,tcb->recvBuf+length,tcb->usedBufLen-length);
    //更新长度
    tcb->usedBufLen=tcb->usedBufLen-length;
    //
    pthread_mutex_unlock(tcb->bufMutex);
}
//Defined by Niu in lab4-2 End


