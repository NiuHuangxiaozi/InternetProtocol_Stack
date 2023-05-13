//文件名: topology/topology.h
//
//描述: 这个文件声明一些用于解析拓扑文件的辅助函数 

#ifndef TOPOLOGY_H 
#define TOPOLOGY_H
#include <netdb.h>

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname); 

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr);

//从一个int ip的值返回对应得nodeID
int topology_getNodeIDfromip_val(int ip_val);


//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID();

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum(); 

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数. 
int topology_getNodeNum();

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID.  
int* topology_getNodeArray(); 

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray(); 

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID);


//defiend in lab4-3

#define FILE_PATH "./topology/topology.dat"

typedef struct IP_node
{
    char ip[16];
    struct IP_node * next;
} IP_node;


//这个函数获得指定ip对应的 struct in_addr* addr 形式
unsigned int topology_ip2long(char* ip);

//这个函数获得dat行数
int topology_getLine();

//加入一个ip struct 的node,返回此时list有几个节点，相当于set操作
IP_node * NodeID_addin_list(IP_node * head ,char * ip,int * length);
//defined in lab4-3
#endif
