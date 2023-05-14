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
    //print_seg(segPtr);
    //填写校验和
    unsigned short csum_number=checksum(segPtr);
    segPtr->header.checksum=csum_number;



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
        //printf("C\n");
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
                    cur_state = SEGRECV;
                }
                break;
            default:
                break;
        }
        if(is_received==1)
            break;
    }
    //将seg从缓冲区拷贝到上一层中
    memcpy(segPtr,temp_buf,temp_buf_loc);
    //丢包
    if(seglost(segPtr)==1)
        return 0;
    //计算校验和
    if(checkchecksum(segPtr)==-1)
    {
        return 1;
    }
    else
    {
        return 2;
    }
}











int seglost(seg_t* segPtr) {
	int random = rand()%100;
	if(random<PKT_LOSS_RATE*100) {
		//50%可能性丢失段
		if(rand()%2==0)
        {
			//printf("seg lost!!!\n");
            return 1;
		}
		//50%可能性是错误的校验和
		else
        {

			//获取数据长度
			int len = sizeof(stcp_hdr_t)+segPtr->header.length;
			//获取要反转的随机位
			int errorbit = rand()%(len*8);
			//反转该比特
			char* temp = (char*)segPtr;
			temp = temp + errorbit/8;
			*temp = *temp^(1<<(errorbit%8));
			return 0;
		}
	}
	return 0;
}



//Defined by Niu in lab4-1
void print_seg(seg_t *seg)
{
    printf("seg information |src_port : %d | des_port : %d | type : %d |seqNum %d \n",
           seg->header.src_port,seg->header.dest_port,seg->header.type,seg->header.seq_num);
}
void increase_print_index()
{
    print_index++;
}
//Defined by Niu in lab4-1  end

//这个函数计算指定段的校验和.
//校验和覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment)
{
    unsigned short answer=0;
    segment->header.checksum=0;

    char * csum_buf=NULL;
    switch(segment->header.type)
    {
        case SYN:
        case SYNACK:
        case FIN:
        case FINACK:
        case DATAACK:
            csum_buf=(char*) malloc(sizeof(segment->header));
            memcpy(csum_buf,segment,sizeof(segment->header));
            answer=csum((unsigned short *)csum_buf,sizeof(segment->header));
            free(csum_buf);
            break;
        case DATA:
            csum_buf=(char*) malloc(sizeof(segment->header)+ segment->header.length);
            memcpy(csum_buf,segment,sizeof(segment->header)+segment->header.length);
            answer=csum((unsigned short *)csum_buf,sizeof(segment->header)+segment->header.length);
            free(csum_buf);
            break;
        default:
            break;
    }
  return answer;
}

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1
int checkchecksum(seg_t* segment)
{
    //结果
    unsigned short answer=0;
    //保存得到的checksum
    unsigned short recv_csum=segment->header.checksum;

    //将原来的checksum清零
    segment->header.checksum=0;
    //定义buf,拷贝相应的内容
    char * csum_buf=NULL;
    switch(segment->header.type)
    {
        case SYN:
        case SYNACK:
        case FIN:
        case FINACK:
        case DATAACK:
            csum_buf=(char*) malloc(sizeof(segment->header));
            memcpy(csum_buf,segment,sizeof(segment->header));
            answer=ccsum((unsigned short *)csum_buf,sizeof(segment->header));
            free(csum_buf);
            if((recv_csum+answer)==0xFFFF)
                return 1;
            else
                return -1;
        case DATA:
            csum_buf=(char*) malloc(sizeof(segment->header)+ segment->header.length);
            memcpy(csum_buf,segment,sizeof(segment->header)+segment->header.length);
            answer=ccsum((unsigned short *)csum_buf,sizeof(segment->header)+segment->header.length);
            free(csum_buf);
            //printf("DATA %d | %d\n",recv_csum,answer);
            if((recv_csum+answer)==0xFFFF)
                return 1;
            else
                return -1;
        default:
            break;
    }
  return 0;
}

/* Checksum a block of data */
//load on https://www.rfc-editor.org/rfc/rfc1071
unsigned short csum (unsigned short *packet, int packlen)
{
    register unsigned long sum = 0;

    while (packlen > 1) {

        sum+= *(packet++);
        packlen-=2;
    }

    if (packlen > 0)
        sum += *(unsigned char *)packet;

    /* TODO: this depends on byte order */
    if( packlen > 0 )
        sum += * (unsigned char *) packet;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (unsigned short) ~sum;
}

unsigned short ccsum (unsigned short *packet, int packlen)
{
    register unsigned long sum = 0;

    while (packlen > 1) {

        sum+= *(packet++);
        packlen-=2;
    }

    if (packlen > 0)
        sum += *(unsigned char *)packet;

    /* TODO: this depends on byte order */
    if( packlen > 0 )
        sum += * (unsigned char *) packet;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (unsigned short)sum;
}
