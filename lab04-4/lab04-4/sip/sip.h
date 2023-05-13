//文件名: sip/sip.h
//
//描述: 这个文件定义用于SIP进程的数据结构和函数.  

#ifndef NETWORK_H
#define NETWORK_H

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT.
//成功时返回连接描述符, 否则返回-1.
int connectToSON();

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.路由更新报文包含这个节点的距离矢量.
//广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
void* routeupdate_daemon(void* arg);

//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表. 
void* pkthandler(void* arg); 

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop();

//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t. 
//接收的段被封装进数据报(一个段在一个数据报中), 然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.
void waitSTCP();



//defined in lab4-4
//这个函数会更新dv表
void update_dv_table(int src_node,sip_pkt_t * sippkt);

//这个函数会在cost缩短的时候及时更新
void update_routing_table(int target_node,int new_next_node);

//构造广播的报文
void create_routeupdate_pkt(sip_pkt_t * pkt);

//更新路由表和矢量表（方式一）
void update_min(int src_node,int dest_node,int cost);

//更新路由表和矢量表（方式二）好消息传得快，坏消息传得慢
void update_direct(int src_node,int dest_node,int cost);

//将下一条为nextnode的路由表项删除，dv表设置为INF
void dv_routingtable_delete_nextnode(int next_node);
//defined in lab4-4 end
#endif
