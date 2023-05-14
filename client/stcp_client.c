#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "stcp_client.h"


//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

//Defiend by Niu in lab1
#include "assert.h"
#include "unistd.h"
#include "string.h"


client_tcb_t ** client_tcbs=NULL;//定义tcb表
pthread_t seghandler_tid;
int Son_sockfd=0;
extern int print_index;  //in order to print information
//Defined by Niu in lab1 End

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
    //1找寻空闲的TCB条目并创建相应的tcb
    int new_tcb = tcbtable_newtcb(client_port);

    if(new_tcb<0)//失败返回-1
        return -1;
    //2得到TCB
    client_tcb_t* my_clienttcb = tcbtable_gettcb(new_tcb);
    //3设置tcb的内容 client的port已经在tcbtable_newtcb函数里面设置

    my_clienttcb->client_nodeID =-1;
    my_clienttcb->server_nodeID = -1;

    my_clienttcb->next_seqNum = 0;
    my_clienttcb->state = CLOSED;

    my_clienttcb->sendBufHead = 0;
    my_clienttcb->sendBufunSent = 0;
    my_clienttcb->sendBufTail = 0;

    my_clienttcb->unAck_segNum = 0;

    //为发送缓冲区创建互斥量
    pthread_mutex_t* sendBuf_mutex;
    sendBuf_mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    assert(sendBuf_mutex!=NULL);
    pthread_mutex_init(sendBuf_mutex,NULL);
    my_clienttcb->bufMutex = sendBuf_mutex;

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
    //通过sockfd获得tcb
    client_tcb_t* tcb;
    tcb = tcbtable_gettcb(sockfd);
    if(!tcb)return -1;

    //发送的数据段
    seg_t syn;

    //剩余重传有效次数
    int rest_retry=SYN_MAX_RETRY;

    //FSM
    switch (tcb->state) {
            case CLOSED:
                //初始化SYN报文
                tcb->server_portNum = server_port;

                Initial_stcp_control_seg(tcb, SYN, &syn);
                //发送报文
                sip_sendseg(Son_sockfd, &syn);

                printf("[%d] Client Send Syn :client portNum %d:\n",print_index,tcb->client_portNum);
                increase_print_index();

                //状态转移
                tcb->state=SYNSENT;

                //超时时重新尝试
                while(rest_retry>0)
                {
                        select(0,0,0,0,&(struct timeval){.tv_usec = SYN_TIMEOUT/1000});
                        if(tcb->state == CONNECTED)
                        {
                                return 1;
                        }
                        else
                        {
                            sip_sendseg(Son_sockfd, &syn);
                            rest_retry--;
                        }
                }
                //状态转换
                tcb->state = CLOSED;
                return -1;
                break;
            case SYNSENT:
                return -1;
                break;
            case CONNECTED:
                return -1;
                break;
            case FINWAIT:
                return -1;
                break;
            default:
                return -1;
                break;
    }
}

// 发送数据给STCP服务器
int stcp_client_send(int sockfd, void* data, unsigned int length)
{
    //通过sockfd获得tcb
    client_tcb_t* clienttcb;
    clienttcb = tcbtable_gettcb(sockfd);
    if(!clienttcb)
        return -1;

    int segNum;
    int i;
    switch(clienttcb->state)
    {
        case CLOSED:
            return -1;
        case SYNSENT:
            return -1;
        case CONNECTED:
            //使用提供的数据创建段
            segNum = length/MAX_SEG_LEN;
            if(length%MAX_SEG_LEN)
                segNum++;
            for(i=0;i<segNum;i++)
            {
                segBuf_t* newBuf = (segBuf_t*) malloc(sizeof(segBuf_t));
                assert(newBuf!=NULL);
                bzero(newBuf,sizeof(segBuf_t));
                newBuf->seg.header.src_port = clienttcb->client_portNum;
                newBuf->seg.header.dest_port = clienttcb->server_portNum;
                if(length%MAX_SEG_LEN!=0 && i==segNum-1)
                    newBuf->seg.header.length = length%MAX_SEG_LEN;
                else
                    newBuf->seg.header.length = MAX_SEG_LEN;
                newBuf->seg.header.type = DATA;
                char* datatosend = (char*)data;
                memcpy(newBuf->seg.data,&datatosend[i*MAX_SEG_LEN],newBuf->seg.header.length);
                sendBuf_addSeg(clienttcb,newBuf);
            }

            //发送段直到未确认段数目达到GBN_WINDOW为止, 如果sendBuf_timer没有启动, 就启动它.
            sendBuf_send(clienttcb);
            return 1;
        case FINWAIT:
            return -1;
        default:
            return -1;
    }
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
    //通过sockfd获得tcb
    client_tcb_t* tcb;
    tcb = tcbtable_gettcb(sockfd);
    if(!tcb)return -1;

    //发送的数据段
    seg_t fin;
    //FSM
    while(1) {
        switch (tcb->state)
        {
            case CLOSED:
                return -1;
                break;
            case SYNSENT:
                return -1;
                break;
            case CONNECTED:
                //初始FIN报文
                bzero(&fin,sizeof(fin));
                Initial_stcp_control_seg(tcb, FIN, &fin);

                //发送FIN
                sip_sendseg(Son_sockfd, &fin);

                printf("[%d] | Client send fin :client portNum %d:\n",print_index,tcb->client_portNum);
                increase_print_index();
                //状态转换
                tcb->state = FINWAIT;
                printf("[%d] | CLIENT: FINWAIT\n",print_index);

                //如果超时就重发
                int retry = FIN_MAX_RETRY;
                while(retry>0) {
                    select(0,0,0,0,&(struct timeval){.tv_usec = FIN_TIMEOUT/1000});
                    if(tcb->state == CLOSED) {
                        tcb->server_nodeID = -1;
                        tcb->server_portNum = 0;
                        tcb->next_seqNum = 0;
                        sendBuf_clear(tcb);
                        return 1;
                    }
                    else {
                        printf("[%d] | CLIENT: FIN RESENT\n",print_index);
                        sip_sendseg(Son_sockfd, &fin);
                        retry--;
                    }
                }
                tcb->state = CLOSED;
                return -1;
                break;
            case FINWAIT:
                return -1;
                break;
            default:
                return -1;
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
    //通过sockfd获得tcb
    client_tcb_t* clienttcb;
    clienttcb = tcbtable_gettcb(sockfd);
    if(!clienttcb)
        return -1;

    switch(clienttcb->state) {
        case CLOSED:
            free(clienttcb->bufMutex);
            free(client_tcbs[sockfd]);
            client_tcbs[sockfd]=NULL;
            return 1;
        case SYNSENT:
            return -1;
        case CONNECTED:
            return -1;
        case FINWAIT:
            return -1;
        default:
            return -1;
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
        else if(recv_flag==1)//checksum计算错误
        {
            printf("[%d] | The package checksum error!\n",print_index);
            increase_print_index();
        }
        else if(recv_flag==2)//收到了包
        {
            printf("[%d] | client received package\n",print_index);
            increase_print_index();

            action(&recv_seg);
        }
    }
    pthread_exit(NULL);
    return 0;
}






//Defined by Niu in lab4-1

//从tcbtable中获得一个新的tcb, 使用给定的端口号分配给客户端端口.
//返回新的tcb的索引号, 如果tcbtable中所有的tcb都已使用了或给定的端口号已被使用, 就返回-1.
int tcbtable_newtcb(unsigned int port) {
    int i;

    for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++) {
        if(client_tcbs[i]!=NULL&&client_tcbs[i]->client_portNum==port) {
            return -1;
        }
    }

    for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++) {
        if(client_tcbs[i]==NULL) {
            client_tcbs[i] = (client_tcb_t*) malloc(sizeof(client_tcb_t));
            client_tcbs[i]->client_portNum = port;
            return i;
        }
    }
    return -1;
}

//通过给定的套接字获得tcb.
//如果没有找到tcb, 返回0.
client_tcb_t* tcbtable_gettcb(int sockfd)
{
    if(client_tcbs[sockfd]!=NULL)
        return client_tcbs[sockfd];
    else
        return 0;
}

void Initial_stcp_control_seg(client_tcb_t * tcb,int Signal,seg_t  *syn)
{
    //清零
    bzero(syn,sizeof(*syn));
    //构造其他必须信息
    syn->header.src_port=tcb->client_portNum; //源端口
    syn->header.dest_port=tcb->server_portNum;//目标的端口号
    //syn->header.seq_num=0;        //序号
    //syn->header.ack_num=0;       //确认号

    switch(Signal) { //type
        case SYN:
            syn->header.type = SYN;
            syn->header.length=0;
            syn->header.seq_num=0;
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

void action(seg_t  *segBuf)
{
    //找到tcb来处理段
    client_tcb_t* my_clienttcb = tcbtable_gettcbFromPort(segBuf->header.dest_port);
    if(!my_clienttcb)
    {
        printf("CLIENT: NO PORT FOR RECEIVED SEGMENT\n");
        return;
    }
    //段处理
    switch(my_clienttcb->state) {
        case CLOSED:
            break;
        case SYNSENT:
            //&&my_clienttcb->server_nodeID==src_nodeID
            if(segBuf->header.type==SYNACK&&my_clienttcb->server_portNum==segBuf->header.src_port)
            {
                printf("CLIENT: SYNACK RECEIVED\n");
                my_clienttcb->state = CONNECTED;
                printf("CLIENT: CONNECTED\n");
            }
            else
                printf("CLIENT: IN SYNSENT, NON SYNACK SEG RECEIVED\n");
            break;
        case CONNECTED:
            //&&my_clienttcb->server_nodeID==src_nodeID
            if(segBuf->header.type==DATAACK&&\
               my_clienttcb->server_portNum==segBuf->header.src_port)
               {
                    if(my_clienttcb->sendBufHead!=NULL&&
                    segBuf->header.ack_num >= my_clienttcb->sendBufHead->seg.header.seq_num)
                    {
                        //接收到ack, 更新发送缓冲区
                        sendBuf_recvAck(my_clienttcb, segBuf->header.ack_num);
                        //发送在发送缓冲区中的新段
                        sendBuf_send(my_clienttcb);
                    }
                }
            else {
                printf("CLIENT: IN CONNECTED, NON DATAACK SEG RECEIVED\n");
            }
            break;
        case FINWAIT:
            //&&my_clienttcb->server_nodeID==src_nodeID
            if(segBuf->header.type==FINACK&&
               my_clienttcb->server_portNum==segBuf->header.src_port)
            {
                printf("CLIENT: FINACK RECEIVED\n");
                my_clienttcb->state = CLOSED;
                printf("CLIENT: CLOSED\n");
            }
            else
                printf("CLIENT: IN FINWAIT, NON FINACK SEG RECEIVED\n");
            break;
    }
}




//通过给定的客户端端口号获得tcb.
//如果没有找到tcb, 返回0.
client_tcb_t* tcbtable_gettcbFromPort(unsigned int clientPort)
{
    int i;
    for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++) {
        if(client_tcbs[i]!=NULL && client_tcbs[i]->client_portNum==clientPort) {
            return client_tcbs[i];
        }
    }
    return 0;
}
//funcs Defined by niu in lab 4-1 End


//funcs Defined by niu in lab 4-2
/*******************************************************/
//
// 用于发送缓冲区操作的辅助函数
//
/*******************************************************/

//这个线程持续轮询发送缓冲区以触发超时事件. 如果发送缓冲区非空, 它应一直运行.
//如果(当前时间 - 第一个已发送但未被确认段的发送时间) > DATA_TIMEOUT, 就调用sendBuf_timeout().
void* sendBuf_timer(void* clienttcb)
{
    client_tcb_t* my_clienttcb = (client_tcb_t*) clienttcb;
    while(1)
    {
        select(0,0,0,0,&(struct timeval){.tv_usec = SENDBUF_POLLING_INTERVAL/1000});

        //获得现在的时间
        struct timeval currentTime;
        gettimeofday(&currentTime,NULL);

        //如果unAck_segNum为0, 意味着发送缓冲区中没有段, 退出.
        if(my_clienttcb->unAck_segNum == 0) {
            pthread_exit(NULL);
        }
        else if(my_clienttcb->sendBufHead->sentTime>0 && my_clienttcb->sendBufHead->sentTime<currentTime.tv_sec*1000000+currentTime.tv_usec-DATA_TIMEOUT) {
            sendBuf_timeout(my_clienttcb);
        }
    }
}
//当超时事件发生时, 这个函数被调用.
//重新发送在clienttcb的发送缓冲区中所有已发送但未被确认段.
void sendBuf_timeout(client_tcb_t* clienttcb)
{
    pthread_mutex_lock(clienttcb->bufMutex);
    segBuf_t* bufPtr=clienttcb->sendBufHead;
    int i;
    for(i=0;i<clienttcb->unAck_segNum;i++)
    {
        printf("sendBuf timeout | \n");
        print_seg(&(bufPtr->seg));
        sip_sendseg(Son_sockfd, &(bufPtr->seg));
        struct timeval currentTime;
        gettimeofday(&currentTime,NULL);
        bufPtr->sentTime = currentTime.tv_sec*1000000+ currentTime.tv_usec;
        bufPtr = bufPtr->next;
    }
    pthread_mutex_unlock(clienttcb->bufMutex);
}

//添加一个段到发送缓冲区中
//你应该初始化segBuf中必要的字段, 然后将segBuf添加到clienttcb使用的发送缓冲区链表中.
void sendBuf_addSeg(client_tcb_t* clienttcb, segBuf_t* newSegBuf)
{
    pthread_mutex_lock(clienttcb->bufMutex);
    if(clienttcb->sendBufHead==0) {
        newSegBuf->seg.header.seq_num = clienttcb->next_seqNum;
        clienttcb->next_seqNum += newSegBuf->seg.header.length;
        newSegBuf->sentTime = 0;
        clienttcb->sendBufHead= newSegBuf;
        clienttcb->sendBufunSent = newSegBuf;
        clienttcb->sendBufTail = newSegBuf;
    } else {
        newSegBuf->seg.header.seq_num = clienttcb->next_seqNum;
        clienttcb->next_seqNum += newSegBuf->seg.header.length;
        newSegBuf->sentTime = 0;
        clienttcb->sendBufTail->next = newSegBuf;
        clienttcb->sendBufTail = newSegBuf;
        if(clienttcb->sendBufunSent == 0)
            clienttcb->sendBufunSent = newSegBuf;
    }
    pthread_mutex_unlock(clienttcb->bufMutex);
}

//发送clienttcb的发送缓冲区中的段, 直到已发送但未被确认段的数量到达GBN_WINDOW为止.
//如果需要, 就启动sendBuf_timer.
void sendBuf_send(client_tcb_t* clienttcb)
{
    pthread_mutex_lock(clienttcb->bufMutex);

    while(clienttcb->unAck_segNum<GBN_WINDOW && clienttcb->sendBufunSent!=0) {
        sip_sendseg(Son_sockfd, (seg_t*)clienttcb->sendBufunSent);
        struct timeval currentTime;
        gettimeofday(&currentTime,NULL);
        clienttcb->sendBufunSent->sentTime = currentTime.tv_sec*1000000+ currentTime.tv_usec;
        //在发送了第一个数据段之后应启动sendBuf_timer
        if(clienttcb->unAck_segNum ==0) {
            pthread_t timer;
            pthread_create(&timer,NULL,sendBuf_timer, (void*)clienttcb);
        }
        clienttcb->unAck_segNum++;

        if(clienttcb->sendBufunSent != clienttcb->sendBufTail)
            clienttcb->sendBufunSent= clienttcb->sendBufunSent->next;
        else
            clienttcb->sendBufunSent = 0;
    }
    pthread_mutex_unlock(clienttcb->bufMutex);
}

//当clienttcb转换到CLOSED状态时, 这个函数被调用.
//它删除clienttcb的发送缓冲区链表, 清除发送缓冲区指针.
void sendBuf_clear(client_tcb_t* clienttcb)
{
    pthread_mutex_lock(clienttcb->bufMutex);
    segBuf_t* bufPtr = clienttcb->sendBufHead;
    while(bufPtr!=clienttcb->sendBufTail) {
        segBuf_t* temp = bufPtr;
        bufPtr = bufPtr->next;
        free(temp);
    }
    free(clienttcb->sendBufTail);
    clienttcb->sendBufunSent = 0;
    clienttcb->sendBufHead = 0;
    clienttcb->sendBufTail = 0;
    clienttcb->unAck_segNum = 0;
    pthread_mutex_unlock(clienttcb->bufMutex);
}


//当接收到一个DATAACK时, 这个函数被调用.
//你应该更新clienttcb中的指针, 释放所有已确认segBuf.
void sendBuf_recvAck(client_tcb_t* clienttcb, unsigned int ack_seqnum)
{
    pthread_mutex_lock(clienttcb->bufMutex);

    //如果所有段都被确认了
    if(ack_seqnum>clienttcb->sendBufTail->seg.header.seq_num)
        clienttcb->sendBufTail = 0;

    segBuf_t* bufPtr= clienttcb->sendBufHead;
    while(bufPtr && bufPtr->seg.header.seq_num<ack_seqnum) {
        clienttcb->sendBufHead = bufPtr->next;
        segBuf_t* temp = bufPtr;
        bufPtr = bufPtr->next;
        free(temp);
        clienttcb->unAck_segNum--;
    }
    pthread_mutex_unlock(clienttcb->bufMutex);
}

//Defined by Niu in lab4-2 End