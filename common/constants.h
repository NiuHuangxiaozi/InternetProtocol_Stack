//文件名: common/constants.h

//描述: 这个文件包含STCP协议使用的常量

#ifndef CONSTANTS_H
#define CONSTANTS_H

//服务器打开的重叠网络层端口号. 客户端将连接到这个端口. 
#define SON_PORT 10000
//这是STCP可以支持的最大连接数. 你的TCB表应包含MAX_TRANSPORT_CONNECTIONS个条目.
#define MAX_TRANSPORT_CONNECTIONS 10
//最大段长度
//MAX_SEG_LEN = 1500 - sizeof(stcp header) - sizeof(sip header)
#define MAX_SEG_LEN  1464
//数据包丢失率为10%
#define PKT_LOSS_RATE 0.1
//SYN_TIMEOUT值, 单位为纳秒
#define SYN_TIMEOUT 100000000
//FIN_TIMEOUT值, 单位为纳秒
#define FIN_TIMEOUT 100000000
//stcp_client_connect()中的最大SYN重传次数
#define SYN_MAX_RETRY 5
//stcp_client_disconnect()中的最大FIN重传次数
#define FIN_MAX_RETRY 5
//服务器CLOSEWAIT超时值, 单位为秒
#define CLOSEWAIT_TIMEOUT 1
//stcp_server_accept()函数使用这个时间间隔来忙等待TCB状态转换, 单位为微秒
#define ACCEPT_POLLING_INTERVAL 100



//defined by niu
//服务器的IP
#define TAR_IP "127.0.0.1"

#define SON_MAX_WAITTING_LIST 10 //服务器对于9009端口的最多等待数

#define SYN_TEST_TIME 500 //500微妙探测一次是否收到了SYNACK
#define FIN_TEST_TIME 500 //500微妙探测一次是否收到了FINACK
//defined by niu end
#endif
