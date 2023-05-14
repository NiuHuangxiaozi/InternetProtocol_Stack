//
// 文件名: seg.c

// 描述: 这个文件包含用于发送和接收STCP段的接口sip_sendseg() and sip_rcvseg(), 及其支持函数的实现. 
#include "errno.h"
#include "seg.h"
#include "stdio.h"
#include "time.h"
#include <sys/socket.h>
#include "assert.h"
#include "stdlib.h"
#include "string.h"


//defined by niu
int print_index=0;
//
//
//  用于客户端和服务器的SIP API 
//  =======================================
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: sip_sendseg()和sip_recvseg()是由网络层提供的服务, 即SIP提供给STCP.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

// 通过重叠网络(在本实验中，是一个TCP连接)发送STCP段. 因为TCP以字节流形式发送数据, 
// 为了通过重叠网络TCP连接发送STCP段, 你需要在传输STCP段时，在它的开头和结尾加上分隔符. 
// 即首先发送表明一个段开始的特殊字符"!&"; 然后发送seg_t; 最后发送表明一个段结束的特殊字符"!#".  
// 成功时返回1, 失败时返回-1. sip_sendseg()首先使用send()发送两个字符, 然后使用send()发送seg_t,
// 最后使用send()发送表明段结束的两个字符.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int sip_sendseg(int connection, seg_t* segPtr)
{
    //三个send
    char start_symbol[2]={'!','&'};
    ssize_t begin_err = send(connection,start_symbol, sizeof(start_symbol), 0);
    assert(begin_err > 0);
    if(begin_err<=0)return -1;

    //打印段的信息
    print_seg(segPtr);

    ssize_t seg_err =
            send(connection,segPtr, sizeof(segPtr->header)+segPtr->header.length, 0);
    assert(seg_err > 0);
    if(seg_err<=0)return -1;

    char end_symbol[2]={'!','#'};
    ssize_t end_err = send(connection,end_symbol, sizeof(end_symbol), 0);
    assert(end_err > 0);
    if(end_err<=0)return -1;

    return 1;
}

// 通过重叠网络(在本实验中，是一个TCP连接)接收STCP段. 我们建议你使用recv()一次接收一个字节.
// 你需要查找"!&", 然后是seg_t, 最后是"!#". 这实际上需要你实现一个搜索的FSM, 可以考虑使用如下所示的FSM.
// SEGSTART1 -- 起点 
// SEGSTART2 -- 接收到'!', 期待'&' 
// SEGRECV -- 接收到'&', 开始接收数据
// SEGSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 这里的假设是"!&"和"!#"不会出现在段的数据部分(虽然相当受限, 但实现会简单很多).
// 你应该以字符的方式一次读取一个字节, 将数据部分拷贝到缓冲区中返回给调用者.
//
// 注意: 还有一种处理方式可以允许"!&"和"!#"出现在段首部或段的数据部分. 具体处理方式是首先确保读取到!&，然后
// 直接读取定长的STCP段首部, 不考虑其中的特殊字符, 然后按照首部中的长度读取段数据, 最后确保以!#结尾.
//
// 注意: 在你剖析了一个STCP段之后,  你需要调用seglost()来模拟网络中数据包的丢失. 
// 在sip_recvseg()的下面是seglost()的代码.
// 
// 如果段丢失了, 就返回1, 否则返回0.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int sip_recvseg(int connection, seg_t* segPtr)
{

    //最初的状态
    int cur_state=SEGSTART1;
    //当前接受的字符
    char symbol;
    //判断是否已经接受到段
    int is_received=0;

    //临时的缓冲区
    int buf_size=sizeof(*segPtr);
    char temp_buf[buf_size];
    int temp_buf_loc=0;//临时缓冲区的下标指示

    //接受段的FSM
    while(1)
    {

        ssize_t recv_flag=recv(connection,&symbol,sizeof(symbol),0);
        //printf("error: %s \n", strerror(errno));
        //重叠网络层断开了
        if(recv_flag==0)return -1;
        switch (cur_state)
        {
                case SEGSTART1:
                    if (symbol == '!')
                        cur_state = SEGSTART2;
                    break;
                case SEGSTART2:
                    if (symbol == '&')
                        cur_state = SEGRECV;
                    else
                        cur_state = SEGSTART1;
                    break;
                case SEGRECV:
                        if(symbol == '!')
                            cur_state = SEGSTOP1;
                        else
                        {
                            memcpy((char *) (temp_buf + temp_buf_loc), &symbol, 1);
                            temp_buf_loc++;
                        }
                    break;
                case SEGSTOP1:
                    if (symbol == '#')
                    {
                        is_received=1;
                    }
                    else
                    {
                        char pre_sym='!';
                        memcpy((char *) (temp_buf + temp_buf_loc), &pre_sym, 1);
                        temp_buf_loc++;
                        memcpy((char *) (temp_buf + temp_buf_loc), &symbol, 1);
                        temp_buf_loc++;
                    }
                    break;
                default:
                    break;
        }
        if(is_received==1)
            break;
    }
    //丢包

    if(seglost(segPtr)==1)
    {
        printf("[%d] | The package has been throw out!\n",print_index);
        increase_print_index();
        return 0;
    }


    //将seg从缓冲区拷贝到上一层中
    memcpy(segPtr,temp_buf,temp_buf_loc);
    printf("[%d] | receive package [sip_recvseg]\n",print_index);
    increase_print_index();
    print_seg(segPtr);
    return 1;
}

int seglost(seg_t* segPtr)
{
	int random = rand()%100;
	if(random<PKT_LOSS_RATE*100)
		return 1;
	else 
		return 0;

    //后面是校验和
}




//defined by niu
void print_seg(seg_t *seg)
{
    printf("seg information |src_port : %d | des_port : %d | type : %d \n",
           seg->header.src_port,seg->header.dest_port,seg->header.type);
}
void increase_print_index()
{
    print_index++;
}

