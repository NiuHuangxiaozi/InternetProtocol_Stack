//文件名: common/constants.h

//描述: 这个文件包含常量

#ifndef CONSTANTS_H
#define CONSTANTS_H

/*******************************************************************/
//重叠网络参数
/*******************************************************************/

//这个端口号用于重叠网络中节点之间的互联
#define CONNECTION_PORT 3000

//这个端口号由SON进程打开, 并由SIP进程连接
#define SON_PORT 3500

//最大SIP报文数据长度: 1500 - sizeof(sip header)
#define MAX_PKT_LEN 1488 

/*******************************************************************/
//网络层参数
/*******************************************************************/
//重叠网络支持的最大节点数 
#define MAX_NODE_NUM 10

//无穷大的链路代价值, 如果两个节点断开连接了, 它们之间的链路代价值就是INFINITE_COST
#define INFINITE_COST 999

//这是广播节点ID. 如果SON进程从SIP进程处接收到一个目标节点ID为BROADCAST_NODEID的报文, 它应该将该报文发送给它的所有邻居
#define BROADCAST_NODEID 9999

//路由更新广播间隔, 以秒为单位
#define ROUTEUPDATE_INTERVAL 15

//defined in lab4-3
#define LOCAL_IP "127.0.0.1"
#define L_IP "192.168.56.101"

//defined in lab4-3 end
#endif
