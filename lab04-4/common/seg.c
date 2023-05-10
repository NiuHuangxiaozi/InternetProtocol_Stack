
#include "seg.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"


//defined in lab4-2
#include <sys/socket.h>
#include "assert.h"

//defined in lab4-2 end




//defined by niu
int print_index=0;


//STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
//参数sip_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t发送成功,就返回1,否则返回-1.
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segPtr)
{
    //三个send
    char start_symbol[2]={'!','&'};
    ssize_t begin_err = send(sip_conn,start_symbol, sizeof(start_symbol), 0);
    assert(begin_err > 0);
    if(begin_err<=0)return -1;


    //打印段的信息
    //print_seg(segPtr);
    //填写校验和
    unsigned short csum_number=checksum(segPtr);
    segPtr->header.checksum=csum_number;

    //封装sendseg_arg_t包含destnode和seg_t
    sendseg_arg_t stcp2sip;
    memset(&stcp2sip,0,sizeof(stcp2sip));

    stcp2sip.nodeID=dest_nodeID;
    memcpy(&(stcp2sip.seg),segPtr,sizeof(*(segPtr)));

    ssize_t seg_err =send(sip_conn,&stcp2sip, sizeof(stcp2sip), 0);
    assert(seg_err > 0);
    if(seg_err<=0)return -1;

    char end_symbol[2]={'!','#'};
    ssize_t end_err = send(sip_conn,end_symbol, sizeof(end_symbol), 0);
    assert(end_err > 0);
    if(end_err<=0)return -1;
    return 1;
}

//STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
//参数sip_conn是STCP进程和SIP进程之间连接的TCP描述符.
//当接收到段时, 使用seglost()来判断该段是否应被丢弃并检查校验和.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int sip_recvseg(int sip_conn, int* src_nodeID, seg_t* segPtr)
{
    //最初的状态
    int cur_state=SEGSTART1;
    //当前接受的字符
    char symbol;
    //判断是否已经接受到段
    int is_received=0;

    //临时的缓冲区
    sendseg_arg_t sendseg_pkt;
    int buf_size=sizeof(sendseg_pkt);
    char temp_buf[buf_size];
    int temp_buf_loc=0;//临时缓冲区的下标指示

    //接受段的FSM
    while(1)
    {
        ssize_t recv_flag=recv(sip_conn,&symbol,sizeof(symbol),0);
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
    //add in lab 4-4
    //将sendseg_pkt从缓冲区拷贝本地临时缓冲区
    memcpy(&sendseg_pkt,temp_buf,temp_buf_loc);

    //拆包拿出段及其源节点ID
    (*src_nodeID)=sendseg_pkt.nodeID;
    memcpy(segPtr,&(sendseg_pkt.seg),sizeof(sendseg_pkt.seg));

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

//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr)
{
    //最初的状态
    int cur_state=SEGSTART1;
    //当前接受的字符
    char symbol;
    //判断是否已经接受到段
    int is_received=0;

    //临时的缓冲区
    sendseg_arg_t sendseg_pkt;
    int buf_size=sizeof(sendseg_pkt);
    char temp_buf[buf_size];
    int temp_buf_loc=0;//临时缓冲区的下标指示

    //接受段的FSM
    while(1)
    {
        ssize_t recv_flag=recv(stcp_conn,&symbol,sizeof(symbol),0);
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
    //add in lab 4-4
    //将sendseg_pkt从缓冲区拷贝本地临时缓冲区
    memcpy(&sendseg_pkt,temp_buf,temp_buf_loc);

    //拆包拿出段及其源节点ID
    (*dest_nodeID)=sendseg_pkt.nodeID;
    memcpy(segPtr,&(sendseg_pkt.seg),sizeof(sendseg_pkt.seg));
    return 1;
}

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr)
{
    //三个send
    char start_symbol[2]={'!','&'};
    ssize_t begin_err = send(stcp_conn,start_symbol, sizeof(start_symbol), 0);
    assert(begin_err > 0);
    if(begin_err<=0)return -1;


    //封装sendseg_arg_t包含destnode和seg_t
    sendseg_arg_t stcp2sip;
    memset(&stcp2sip,0,sizeof(stcp2sip));

    stcp2sip.nodeID=src_nodeID;
    memcpy(&(stcp2sip.seg),segPtr,sizeof(*(segPtr)));

    ssize_t seg_err =send(stcp_conn,&stcp2sip, sizeof(stcp2sip), 0);
    assert(seg_err > 0);
    if(seg_err<=0)return -1;

    char end_symbol[2]={'!','#'};
    ssize_t end_err = send(stcp_conn,end_symbol, sizeof(end_symbol), 0);
    assert(end_err > 0);
    if(end_err<=0)return -1;
    return 1;
}










// 一个段有PKT_LOST_RATE/2的可能性丢失, 或PKT_LOST_RATE/2的可能性有着错误的校验和.
// 如果数据包丢失了, 就返回1, 否则返回0. 
// 即使段没有丢失, 它也有PKT_LOST_RATE/2的可能性有着错误的校验和.
// 我们在段中反转一个随机比特来创建错误的校验和.
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
