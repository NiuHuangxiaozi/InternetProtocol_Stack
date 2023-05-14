// 文件名: common/pkt.c

#include "pkt.h"
#include "stdlib.h"

//define in lab4-3
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "assert.h"
//defined in lab4-3 end

// son_sendpkt()由SIP进程调用, 其作用是要求SON进程将报文发送到重叠网络中. SON进程和SIP进程通过一个本地TCP连接互连.
// 在son_sendpkt()中, 报文及其下一跳的节点ID被封装进数据结构sendpkt_arg_t, 并通过TCP连接发送给SON进程. 
// 参数son_conn是SIP进程和SON进程之间的TCP连接套接字描述符.
// 当通过SIP进程和SON进程之间的TCP连接发送数据结构sendpkt_arg_t时, 使用'!&'和'!#'作为分隔符, 按照'!& sendpkt_arg_t结构 !#'的顺序发送.
// 如果发送成功, 返回1, 否则返回-1.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn)
{

  //准备报文
  sendpkt_arg_t sip2son;
  sip2son.nextNodeID=nextNodeID;
  sip2son.pkt=*pkt;
  //发送报文
  //三个send
  char start_symbol[2]={'!','&'};
  ssize_t begin_err = send(son_conn,start_symbol, sizeof(start_symbol), 0);
  assert(begin_err > 0);
  if(begin_err<=0) return -1;

  size_t sip_pkt_err = send(son_conn,&sip2son,sizeof(sip2son), 0);
  assert(sip_pkt_err > 0);
  if(sip_pkt_err<=0) return -1;

  char end_symbol[2]={'!','#'};
  ssize_t end_err = send(son_conn,end_symbol, sizeof(end_symbol), 0);
  assert(end_err > 0);
  if(end_err<=0)return -1;

  return 1;
}

// son_recvpkt()函数由SIP进程调用, 其作用是接收来自SON进程的报文. 
// 参数son_conn是SIP进程和SON进程之间TCP连接的套接字描述符. 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
  //最初的状态
  int cur_state=SEGSTART1;
  //当前接受的字符
  char symbol;
  //判断是否已经接受到段
  int is_received=0;

  //临时的缓冲区
  int buf_size=sizeof(*pkt);
  char temp_buf[buf_size];
  int temp_buf_loc=0;//临时缓冲区的下标指示

  //接受段的FSM
  while(1)
  {
    ssize_t recv_flag=recv(son_conn,&symbol,sizeof(symbol),0);
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
  memcpy(pkt,temp_buf,temp_buf_loc);
  return 1;
}

// 这个函数由SON进程调用, 其作用是接收数据结构sendpkt_arg_t.
// 报文和下一跳的节点ID被封装进sendpkt_arg_t结构.
// 参数sip_conn是在SIP进程和SON进程之间的TCP连接的套接字描述符. 
// sendpkt_arg_t结构通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 如果成功接收sendpkt_arg_t结构, 返回1, 否则返回-1.
/*
 typedef struct sendpktargument {
  int nextNodeID;        //下一跳的节点ID
  sip_pkt_t pkt;         //要发送的报文
} sendpkt_arg_t;
 */
int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn)
{
    //最初的状态
    int cur_state=SEGSTART1;
    //当前接受的字符
    char symbol;
    //判断是否已经接受到段
    int is_received=0;

    //临时的缓冲区
    sendpkt_arg_t temp_pkt;
    int buf_size=sizeof(temp_pkt);
    char temp_buf[buf_size];
    int temp_buf_loc=0;//临时缓冲区的下标指示

    //接受段的FSM
    while(1)
    {
        ssize_t recv_flag=recv(sip_conn,&symbol,sizeof(symbol),0);
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
    //将seg从缓冲区拷贝到sendpkt中
    memcpy(&temp_pkt,temp_buf,temp_buf_loc);
    //剥离出sip报文
    memcpy(pkt,&(temp_pkt.pkt),sizeof(temp_pkt.pkt));
    //剥离出要发送的节点
    (*nextNode)=temp_pkt.nextNodeID;
    return 1;
}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的. 
// SON进程调用这个函数将报文转发给SIP进程. 
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符. 
// 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
    //发送报文
    //三个send
    char start_symbol[2]={'!','&'};
    ssize_t begin_err = send(sip_conn,start_symbol, sizeof(start_symbol), 0);
    assert(begin_err > 0);
    if(begin_err<=0) return -1;

    size_t sip_pkt_err = send(sip_conn,pkt,sizeof(*pkt), 0);
    assert(sip_pkt_err > 0);
    if(sip_pkt_err<=0) return -1;

    char end_symbol[2]={'!','#'};
    ssize_t end_err = send(sip_conn,end_symbol, sizeof(end_symbol), 0);
    assert(end_err > 0);
    if(end_err<=0)return -1;
    return 1;
}

// sendpkt()函数由SON进程调用, 其作用是将接收自SIP进程的报文发送给下一跳.
// 参数conn是到下一跳节点的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居节点之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
    //发送报文
    //三个send
    char start_symbol[2]={'!','&'};
    ssize_t begin_err = send(conn,start_symbol, sizeof(start_symbol), 0);
    assert(begin_err > 0);
    if(begin_err<=0) return -1;

    size_t sip_pkt_err = send(conn,pkt,sizeof(*pkt), 0);
    assert(sip_pkt_err > 0);
    if(sip_pkt_err<=0) return -1;

    char end_symbol[2]={'!','#'};
    ssize_t end_err = send(conn,end_symbol, sizeof(end_symbol), 0);
    assert(end_err > 0);
    if(end_err<=0)return -1;
    return 1;
}

// recvpkt()函数由SON进程调用, 其作用是接收来自重叠网络中其邻居的报文.
// 参数conn是到其邻居的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int recvpkt(sip_pkt_t* pkt, int conn)
{
    //最初的状态
    int cur_state=SEGSTART1;
    //当前接受的字符
    char symbol;
    //判断是否已经接受到段
    int is_received=0;

    //临时的缓冲区
    int buf_size=sizeof(*pkt);
    char temp_buf[buf_size];
    int temp_buf_loc=0;//临时缓冲区的下标指示

    //接受段的FSM
    while(1)
    {
        ssize_t recv_flag=recv(conn,&symbol,sizeof(symbol),0);
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
    memcpy(pkt,temp_buf,temp_buf_loc);
    return 1;
}
