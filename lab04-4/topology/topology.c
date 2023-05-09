//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 

#include "topology.h"
//defined in lab4-3
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "../common/constants.h"
#include "errno.h"
//defiend in lab4-3 end

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) 
{
    char temp_id[16];
    strcpy(temp_id,hostname);

    uint8_t addr[4] = {0};
    sscanf(temp_id, "%d.%d.%d.%d", &addr[0],&addr[1],&addr[2],&addr[3]);
    int answer=addr[3];
    return answer;
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr)
{
    uint8_t nodeID= (uint8_t)((addr->s_addr >> 24u)  & 0xFFu);
    int answer=nodeID;
    return answer;
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
{
    return topology_getNodeIDfromname(L_IP);
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{
    int answer=0;
    int item_nums=topology_getLine();
    FILE *fp;
    fp=fopen(FILE_PATH, "r");
    if(fp)
    {
        char ip1[16];
        char ip2[16];
        char cost[16];
        while(item_nums>0)
        {
            fscanf(fp, "%s",ip1);
            fscanf(fp, "%s",ip2);
            fscanf(fp, "%s",cost);
            printf("%s\n",ip1);
            if(strcmp(ip1,L_IP)==0)
                answer++;
            else if(strcmp(ip2,L_IP)==0)
                answer++;
            item_nums--;
        }
        fclose(fp);
    }
    return answer;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{
    //定义ipset集合
    IP_node * head=NULL;
    int answer=0;

    //找寻节点
    int item_nums=topology_getLine();
    FILE *fp;
    fp=fopen(FILE_PATH, "r");
    if(fp)
    {
        char ip1[16];
        char ip2[16];
        char cost[16];
        while(item_nums>0)
        {
            fscanf(fp, "%s",ip1);
            fscanf(fp, "%s",ip2);
            fscanf(fp, "%s",cost);
            head=NodeID_addin_list(head,ip1,&answer);
            head=NodeID_addin_list(head,ip2,&answer);
            item_nums--;
        }
        fclose(fp);
    }
    return answer;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID.
int* topology_getNodeArray()
{
    char ip1[16];
    char ip2[16];
    int cost;
    //定义ipset集合
    IP_node * head=NULL;
    int node_num=0;

    int lines=topology_getLine();
    //读取文件
    FILE *fp;
    fp=fopen(FILE_PATH, "r");
    if(fp)
    {
        while(lines>0)
        {
            fscanf(fp, "%s",ip1);
            fscanf(fp, "%s",ip2);
            fscanf(fp, "%d",&cost);
            head=NodeID_addin_list(head,ip1,&node_num);
            head=NodeID_addin_list(head,ip2,&node_num);
            lines--;
        }
        fclose(fp);
    }

    int * ans_list=(int*)malloc(sizeof(int)*node_num);
    IP_node * pt=head;
    int Node_index=0;
    while(pt!=NULL)
    {
        ans_list[Node_index]=topology_ip2long(pt->ip);
        Node_index++;
        pt=pt->next;
    }
    return ans_list;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray()
{
    char ip1[16];
    char ip2[16];
    int cost;
    //定义ipset集合
    IP_node * head=NULL;
    int node_num=0;

    int lines=topology_getLine();
    //读取文件
    FILE *fp;
    fp=fopen(FILE_PATH, "r");
    if(fp)
    {
        while(lines>0)
        {
            fscanf(fp, "%s",ip1);
            fscanf(fp, "%s",ip2);
            fscanf(fp, "%d",&cost);
            if(strcmp(ip1,L_IP)==0)
                head=NodeID_addin_list(head,ip2,&node_num);
            else if(strcmp(ip2,L_IP)==0)
                head=NodeID_addin_list(head,ip1,&node_num);
            lines--;
        }
        fclose(fp);
    }

    int * ans_list=(int*)malloc(sizeof(int)*node_num);
    IP_node * pt=head;
    int Node_index=0;

    while(pt!=NULL)
    {
        ans_list[Node_index]=topology_ip2long(pt->ip);
        Node_index++;
        pt=pt->next;
    }
    return ans_list;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
    //自己到自己为0
    if(fromNodeID==toNodeID)return 0;

    char ip1[16];
    char ip2[16];
    int cost;
    int item_nums=topology_getLine();
    FILE *fp;
    fp=fopen(FILE_PATH, "r");
    if(fp)
    {
        while(item_nums>0)
        {
            fscanf(fp, "%s",ip1);
            fscanf(fp, "%s",ip2);
            fscanf(fp, "%d",&cost);
            int Node1=topology_getNodeIDfromname(ip1);
            int Node2=topology_getNodeIDfromname(ip2);

            if((Node1==fromNodeID&&Node2==toNodeID)
                ||(Node2==fromNodeID&&Node1==toNodeID))
            {
                return cost;
            }
            item_nums--;
        }
        fclose(fp);
    }
    return INFINITE_COST;
}


//define in lab4-3
//这个函数获得指定ip对应的 struct in_addr* addr 形式
unsigned int topology_ip2long(char* ip)
{

    char temp_ip[16];
    strcpy(temp_ip,ip);
    uint8_t addr[4] = {0};
    sscanf(temp_ip, "%d.%d.%d.%d", &addr[0],&addr[1],&addr[2],&addr[3]);
    return *(uint32_t*)addr;
}

int topology_getLine()
{
    int c;
    FILE *fp;
    int lines=0;
    fp=fopen(FILE_PATH, "r");
    if(fp)
    {
        while((c=fgetc(fp)) != EOF)
        {
            if (c == '\n') lines++;
        }
        fclose(fp);
    }
    return lines;
}

//返回此时加入了几个节点
IP_node * NodeID_addin_list(IP_node * head ,char * ip,int * cur_add)
{
    int flag=0;
    IP_node  *pt=head;
    while(pt!=NULL)
    {
        if(strcmp(pt->ip,ip)==0) {
            flag = 1;
            break;
        }
        pt=pt->next;
    }
    if(flag==0)
    {
        IP_node * new_ip=(IP_node *)malloc(sizeof(IP_node));
        strcpy(new_ip->ip,ip);
        new_ip->next=NULL;
        if(head==NULL)
        {
            head=new_ip;
        }
        else
        {
            new_ip->next=head;
            head=new_ip;
        }
        (*cur_add)++;
    }
    return head;
}
//defined in lab4-3 end


