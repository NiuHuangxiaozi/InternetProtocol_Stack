#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "stcp_client.h"

//defiend by niu
#include "assert.h"
#include "unistd.h"
extern int print_index;
/*面向应用层的接口*/

//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// stcp客户端初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

//Defined by Niu
//定义tcb表[Niu]
client_tcb_t ** client_tcbs=NULL;
pthread_t seghandler_tid;
int Son_sockfd=0;

//Defined by Niu End

void stcp_client_init(int conn)
{
    //初始化tcb表
    client_tcbs=(client_tcb_t**)malloc(MAX_TRANSPORT_CONNECTIONS*sizeof(client_tcb_t*));
    for(int tcb_index=0;tcb_index<MAX_TRANSPORT_CONNECTIONS;tcb_index++)
        client_tcbs[tcb_index]=NULL;

    //保存重叠SON的套接字描述符
    Son_sockfd=conn;

    //启用seghandler
    int seghandler_err= pthread_create(&seghandler_tid,NULL,seghandler,NULL);
    pthread_detach(seghandler_tid);
}

// 创建一个客户端TCB条目, 返回套接字描述符
//
// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_sock(unsigned int client_port) {
    //1找寻空闲的TCB条目
    int new_tcb=-1;
    for(int tcb_index=0;tcb_index<MAX_TRANSPORT_CONNECTIONS;tcb_index++)
    {
      if (client_tcbs[tcb_index] == NULL) {
          new_tcb = tcb_index;
          break;
      }
    }
    if(new_tcb==-1)return -1;
    //2创建TCB
    client_tcbs[new_tcb]=(client_tcb_t*) malloc(sizeof(client_tcb_t));
    //3设置tcb的内容
    client_tcbs[new_tcb]->state=CLOSED;
    client_tcbs[new_tcb]->client_portNum=client_port;

    return new_tcb;
}

// 连接STCP服务器
//
// 这个函数用于连接服务器. 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_connect(int sockfd, unsigned int server_port)
{
    //找到tcb的条目
    client_tcb_t * tcb=client_tcbs[sockfd];
    //设置服务器端口号
    tcb->server_portNum=server_port;

    //发送的数据段
    seg_t syn;
    //剩余重传有效次数
    int rest_retry=SYN_MAX_RETRY;
    //FSM
    while(1) {
        switch (tcb->state) {
            case CLOSED:
                //初始化SYN报文
                Initial_stcp_control_seg(tcb, SYN, &syn);
                //发送报文
                sip_sendseg(Son_sockfd, &syn);
                //状态转移
                tcb->state=SYNSENT;
                printf("[%d] Client send syn :client portNum %d:\n",print_index,tcb->client_portNum);
                increase_print_index();
                break;
            case SYNSENT:
                //循环不断等待SYNACK报文
                printf("[%d] enter synsent loop %d\n",print_index,tcb->client_portNum);
                increase_print_index();
                while (rest_retry > 0)
                {
                    //设置定时器
                    int test_times=(SYN_TIMEOUT/1000)/SYN_TEST_TIME;
                    int cur_test_times=0;
                    while(tcb->state ==SYNSENT&&cur_test_times<test_times)
                    {
                        cur_test_times++;
                        usleep(SYN_TEST_TIME);
                    }
                    //收到了报文
                    if (tcb->state==CONNECTED)
                    {
                        printf("[%d] Client received package\n",print_index);
                        increase_print_index();
                        return 1;
                    }
                    rest_retry--;

                    printf("[%d] synack timeout! %d\n",print_index,tcb->client_portNum);
                    increase_print_index();
                    if(rest_retry>0)
                    {
                        printf("[%d] Recend SYN Package!  %d\n",print_index,tcb->client_portNum);
                        increase_print_index();
                        //初始化SYN报文
                        Initial_stcp_control_seg(tcb, SYN, &syn);
                        //发送报文
                        sip_sendseg(Son_sockfd, &syn);
                    }
                }
                printf("[%d] SYN_MAX_RETRY exceed %d \n",print_index,tcb->client_portNum);
                increase_print_index();
                tcb->state=CLOSED;
                return -1;
                break;
            case CONNECTED:
                break;
            case FINWAIT:
                break;
            default:
                break;
        }
    }
    return 0;
}

// 发送数据给STCP服务器
//
// 这个函数发送数据给STCP服务器. 你不需要在本实验中实现它。
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_send(int sockfd, void* data, unsigned int length) {
	return 1;
}

// 断开到STCP服务器的连接
//
// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN segment给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd)
{
    //找到tcb的条目
    client_tcb_t * tcb=client_tcbs[sockfd];

    //发送的数据段
    seg_t syn;
    //fin剩余重传有效次数
    int rest_retry=FIN_MAX_RETRY;

    //FSM
    while(1) {
        switch (tcb->state) {
            case CONNECTED:

                printf("[%d] | Client send fin :client portNum %d:\n",print_index,tcb->client_portNum);
                increase_print_index();

                //初始FIN报文
                Initial_stcp_control_seg(tcb, FIN, &syn);
                //发送报文
                sip_sendseg(Son_sockfd, &syn);
                //状态转移
                tcb->state=FINWAIT;
                break;
            case FINWAIT:
                //循环不断等待FINACK报文
                printf("[%d] | Enter Fin_wait loop %d\n",print_index,tcb->client_portNum);
                increase_print_index();


                while (rest_retry > 0)
                {
                    //设置定时器
                    int test_times=(FIN_TIMEOUT/1000)/FIN_TEST_TIME;
                    int cur_test_times=0;
                    while(tcb->state ==FINWAIT&&cur_test_times<test_times)
                    {
                        cur_test_times++;
                        usleep(FIN_TEST_TIME);
                    }
                    //收到了报文
                    if (tcb->state==CLOSED)return 1;
                    rest_retry--;

                    printf("[%d] | finack timeout!  %d\n",print_index,tcb->client_portNum);
                    increase_print_index();


                    if(rest_retry>0)
                    {
                        printf("[%d] | Recend FIN Package! %d\n",print_index,tcb->client_portNum);
                        increase_print_index();

                        //初始化FIN报文
                        Initial_stcp_control_seg(tcb, FIN, &syn);
                        //发送报文
                        sip_sendseg(Son_sockfd, &syn);
                    }
                }

                printf("[%d] | FIN_MAX_RETRY exceed  %d\n",print_index,tcb->client_portNum);
                increase_print_index();


                tcb->state=CLOSED;
                return -1;
                break;
            default:
                break;
        }
    }
    return 0;
}

// 关闭STCP客户
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd)
{
    //1
    printf("[%d] | STCP sockfd Delete!\n",print_index);
    increase_print_index();

    //2
    if(client_tcbs[sockfd]->state!=CLOSED)
        return -1;
    else
    {
        free(client_tcbs[sockfd]);
        client_tcbs[sockfd]=NULL;
        return 1;
    }
}

// 处理进入段的线程
//
// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void *seghandler(void* arg)
{
    seg_t  recv_seg;
    while(1)
    {
        int recv_flag=sip_recvseg(Son_sockfd,&recv_seg);
        if(recv_flag==-1)//重叠网络层断开
        {
            printf("[%d] | SON disconnected seghandler exit!\n",print_index);
            increase_print_index();

            break;
        }
        else if(recv_flag==0)//包丢了
        {
            //do nothing
            printf("[%d] | client missing package\n",print_index);
            increase_print_index();
        }
        else if(recv_flag==1)//收到了包
        {
            printf("[%d] | client received package\n",print_index);
            increase_print_index();

            action(&recv_seg);
        }
    }
    pthread_exit(NULL);
    return 0;
}






//defined by niu
void Initial_stcp_control_seg(client_tcb_t * tcb,int Signal,seg_t  *syn)
{
    //构造其他必须信息
    syn->header.src_port=tcb->client_portNum; //源端口
    syn->header.dest_port=tcb->server_portNum;//目标的端口号
    //syn->header.seq_num=0;        //序号
    //syn->header.ack_num=0;       //确认号

    switch(Signal) { //type
        case SYN:
            syn->header.type = SYN;
            syn->header.length=0;
            break;
        case SYNACK:
            break;
        case FIN:
            syn->header.type = FIN;
            syn->header.length=0;
            break;
        case FINACK:
            break;
        case DATA:
            break;
        case DATAACK:
            break;
        default:
            break;
    }
}

void action(seg_t  *seg)
{
    int client_tcb_index=-1;
    for(int tcb_index=0;tcb_index<MAX_TRANSPORT_CONNECTIONS;tcb_index++)
    {
        if(client_tcbs[tcb_index]!=NULL&&\
           client_tcbs[tcb_index]->client_portNum==seg->header.dest_port&&\
           client_tcbs[tcb_index]->server_portNum==seg->header.src_port)
        {
            client_tcb_index=tcb_index;
            break;
        }
    }

    switch(seg->header.type)
    {
        case SYN:
            break;
        case SYNACK:
            if(client_tcbs[client_tcb_index]->state==SYNSENT)
            {
                client_tcbs[client_tcb_index]->state=CONNECTED;
            }
            break;
        case FIN:
            break;
        case FINACK:
            if(client_tcbs[client_tcb_index]->state==FINWAIT)
            {
                client_tcbs[client_tcb_index]->state=CLOSED;
            }
            break;
        case DATA:
            break;
        case DATAACK:
            break;
        default:
            break;
    }
}


//defined by niu end



