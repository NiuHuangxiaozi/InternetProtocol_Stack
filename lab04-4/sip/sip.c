//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程  

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "sip.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"


//defined in lab4-4

#include "math.h"
#include "stdlib.h"
#include <sys/select.h>
//defined in lab4-4 end


//SIP层等待这段时间让SIP路由协议建立路由路径. 
#define SIP_WAITTIME 60

/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 			//到重叠网络的连接
int stcp_conn;			//到STCP的连接
nbr_cost_entry_t* nct;			//邻居代价表
dv_t* dv;				//距离矢量表
pthread_mutex_t* dv_mutex;		//距离矢量表互斥量
routingtable_t* routingtable;		//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT.
//成功时返回连接描述符, 否则返回-1.
int connectToSON()
{
    //创建sockfd
    int son_start_code= socket(AF_INET, SOCK_STREAM, 0);
    if(son_start_code<0)return -1;

    //建立连接
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(LOCAL_IP);
    servaddr.sin_port = htons(SON_PORT);

    int connect_state = connect(son_start_code,(void *)&servaddr, sizeof(servaddr));
    assert(connect_state>=0);
    if(connect_state<0)return -1;

    return son_start_code;
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.路由更新报文包含这个节点
//的距离矢量.广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
void* routeupdate_daemon(void* arg)
{
    //你需要编写这里的代码.
    sip_pkt_t pkt;
    while(1)
    {
        select(0,0,0,0,&(struct timeval){.tv_sec =ROUTEUPDATE_INTERVAL});
        //填充报文的关键信息
        create_routeupdate_pkt(&pkt);
        //广播路由信息
        son_sendpkt(BROADCAST_NODEID,&pkt,son_conn);
    }
    pthread_exit(NULL);
}

//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表.
void* pkthandler(void* arg)
{
    sip_pkt_t pkt;
    while(son_recvpkt(&pkt,son_conn)>0)
    {
        printf("Routing: received a packet from neighbor %d\n", pkt.header.src_nodeID);
        //分析进行不同的处理
        //不是自己的报文
        if(pkt.header.dest_nodeID!=topology_getMyNodeID())
        {
            pthread_mutex_lock(routingtable_mutex);
            int nextID=routingtable_getnextnode(routingtable,pkt.header.dest_nodeID);
            pthread_mutex_unlock(routingtable_mutex);

            if(nextID==-1)//丢弃
            {
                printf("sip throw out pkt\n");
            }
            else
            {
                son_sendpkt(nextID,&pkt,son_conn);
            }
        }
        else if(pkt.header.type==SIP)//sip报文，向上转发
        {
            seg_t pkt2Sip;
            memcpy(&pkt2Sip,pkt.data,pkt.header.length);

            //todo
            int flag=forwardsegToSTCP(stcp_conn,pkt.header.src_nodeID,&pkt2Sip);
            assert(flag==1);
        }
        else if(pkt.header.type==ROUTE_UPDATE)//更新路由表
        {
            //更新矢量表和路由表
            update_dv_table(pkt.header.src_nodeID,&pkt);
        }
        else if(pkt.header.type==POINT_KILLED)//有一个邻居去世了
        {
            //将路由表中下目的地为src_node的表象删除
            //将矢量表中mynodeID-src_node设置为正无穷
            routingtable_dest_delete(routingtable,pkt.header.src_nodeID);
            dvtable_setcost(dv,topology_getMyNodeID(),pkt.header.src_nodeID,INFINITE_COST);

            //同时将相应的矢量表设置为正无穷
            dvtable_delete_point(dv,pkt.header.src_nodeID);
            //最后将矢量表对应的邻居的那一项全部设置为正无穷
            dv_routingtable_delete_nextnode(pkt.header.src_nodeID);
        }
        else
            printf("Unknown pkt \n");
    }
    printf("sip pkthandler killed\n");
    pthread_exit(NULL);
}


//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop()
{
	//你需要编写这里的代码.
  return;
}

//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t. 
//接收的段被封装进数据报(一个段在一个数据报中), 然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.
void waitSTCP()
{
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    //BIND到指定的端口
    int addr_lenth = 0;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SIP_PORT);

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET,SO_REUSEADDR,(const void *)&opt, sizeof(opt));

    int bind_val = bind(sockfd, (const struct sockaddr *) (&addr), sizeof(addr));
    assert(bind_val >= 0);

    //对CONNECTION_PORT端口进行监听
    int liston_val = listen(sockfd, 10);
    assert(liston_val >= 0);

    fd_set fdset;
    while(1)
    {
        printf("sip:waiting for new stcp!\n");
        FD_ZERO(&fdset);               // clear set to zero
        FD_SET(sockfd, &fdset); // add server socket fd to fdset
        int max_fd = sockfd+1;

        int activity = select(max_fd, &fdset, NULL, NULL, NULL);
        if (activity < 0)// select被信号中断，继续调用select
            continue;
        else if(activity == 0)//超时
            continue;
        else
        {
            if (FD_ISSET(sockfd, &fdset))
            {
                int new_socket;
                // accept a client socket
                if ((new_socket = accept(sockfd,
                                         (struct sockaddr *)&addr, (socklen_t *)&addr_lenth)) < 0)
                {
                    perror("accept error");
                    continue;
                }
                //stcp连接的套接字
                stcp_conn=new_socket;
                //上层传来的stcp报文的缓存
                seg_t segPtr;
                int next_node=0;
                while (1)
                {
                    int flag = getsegToSend(stcp_conn,&next_node,&segPtr);
                    if (flag == -1)
                    {
                        printf("sip:stcp is disconnected!\n");
                        break;
                    }
                    else if (flag == 1)
                    {
                        printf("sip receive pkt from stcp \n");
                        //从数据中拷贝sip报文
                        sip_pkt_t sip2son;
                        memcpy(&sip2son,segPtr.data,segPtr.header.length);

                        int son2sip_flag = son_sendpkt(next_node,&sip2son,son_conn);
                        assert(son2sip_flag == 1);
                    }
                }

            }
        }
    }
}

int main(int argc, char *argv[])
{
	printf("SIP layer is starting, pls wait...\n");

	//初始化全局变量

    //初始化邻居代价表
	nct = nbrcosttable_create();

    //创建距离矢量表
	dv = dvtable_create();

	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);

    //创建路由表
	routingtable = routingtable_create();

	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
    //下游
	son_conn = -1;
    //上游
	stcp_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到本地SON进程 
	son_conn = connectToSON();
	if(son_conn<0) {
		printf("can't connect to SON process\n");
		exit(1);		
	}
	
	//启动线程处理来自SON进程的进入报文 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("SIP layer is started...\n");
	printf("waiting for routes to be established\n");
	sleep(SIP_WAITTIME);
	routingtable_print(routingtable);

	//等待来自STCP进程的连接
	printf("waiting for connection from STCP process\n");
	waitSTCP(); 

}




//defined in lab4-4
void update_dv_table(int src_node,sip_pkt_t * sippkt)
{
    pthread_mutex_lock(dv_mutex);
    pkt_routeupdate_t routeupdate_pkt;
    memcpy(&routeupdate_pkt,sippkt->data,sippkt->header.length);

    //item_length这个值其实是图中全部的节点数
    int item_length=routeupdate_pkt.entryNum;
    int nbr_num=topology_getNbrNum();

    //先更新邻居的路由信息
    for(int row=1;row<nbr_num+1;row++)
    {
        if(dv[row].nodeID==src_node)
        {
            for(int index=0;index<item_length;index++)
            {
                dv[row].dvEntry[index].nodeID=routeupdate_pkt.entry[index].nodeID;
                dv[row].dvEntry[index].cost=routeupdate_pkt.entry[index].cost;
            }
        }
    }

    //更新自己的cost
    for(int l_index=0;l_index<item_length;l_index++)
    {
        //获得新的cost
        int dest_node=routeupdate_pkt.entry[l_index].nodeID;
        int cost=routeupdate_pkt.entry[l_index].cost;

        //首先调用相应的API查看我们最短路径的下一跳是否是有依赖关系的，
        //如果有那么就不是最小值，而是直接相加
        int next_node=routingtable_getnextnode(routingtable,dest_node);
        if(next_node==-1||next_node!=src_node)
        {
            update_min(src_node,dest_node,cost);
        }
        else
        {
            update_direct(src_node,dest_node,cost);
        }
    }

    pthread_mutex_unlock(dv_mutex);
}
void update_min(int src_node,int dest_node,int cost)
{
    int all_nums=topology_getNodeNum();
    //遍历这个矩阵，我们获取两个中间cost
    int m_src_cost=0;
    int m_dest_cost=0;
    for(int tar_index=0;tar_index<all_nums;tar_index++)
    {
        if(dv[0].dvEntry[tar_index].nodeID==src_node)
        {
            m_src_cost=dv[0].dvEntry[tar_index].cost;
        }
        if(dv[0].dvEntry[tar_index].nodeID==dest_node)
        {
            m_dest_cost=dv[0].dvEntry[tar_index].cost;
        }
    }
    //求的最小cost
    int min_dis=m_dest_cost>m_src_cost+cost ? m_src_cost+cost : m_dest_cost;

    //保存最小值
    for(int tar_index=0;tar_index<all_nums;tar_index++)
    {
        if(dv[0].dvEntry[tar_index].nodeID==dest_node)
        {
            if(dv[0].dvEntry[tar_index].cost!=min_dis)
            {
                update_routing_table(dest_node,src_node);
            }
            dv[0].dvEntry[tar_index].cost=min_dis;
            break;
        }
    }
}
void update_direct(int src_node,int dest_node,int cost)
{
    int all_nums=topology_getNodeNum();

    printf("good message quick! bad message slow!\n");
    //求dis
    int dis= topology_getCost(src_node,topology_getMyNodeID())+cost;
    //如果最小值已经不可达，那么就要删除对应的路由表
    if(dis>=INFINITE_COST)
    {
        dis=INFINITE_COST;
        routingtable_dest_delete(routingtable,dest_node);
    }
    //保存dis
    for(int tar_index=0;tar_index<all_nums;tar_index++)
    {
        if(dv[0].dvEntry[tar_index].nodeID==dest_node)
        {
            if(dv[0].dvEntry[tar_index].cost!=dis)
            {
                dv[0].dvEntry[tar_index].cost=dis;
            }
            break;
        }
    }
}
void update_routing_table(int target_node,int new_next_node)
{
    //更新路由表
    pthread_mutex_lock(routingtable_mutex);

    //hash寻找对应的下标
    int hash_index=target_node%MAX_ROUTINGTABLE_SLOTS;

    routingtable_entry_t * head=routingtable->hash[hash_index];

    while(head!=NULL)
    {
        if(head->destNodeID==target_node)
        {
            head->nextNodeID=new_next_node;
            break;
        }
        head=head->next;
    }

    if(head==NULL)//说明没有找到这样的条目
    {
        routingtable_entry_t * routine_entry=(routingtable_entry_t *)
                malloc(sizeof(routingtable_entry_t));
        routine_entry->destNodeID=target_node;
        routine_entry->nextNodeID=new_next_node;
        routine_entry->next=NULL;

        //头部插入
        if(routingtable->hash[hash_index]==NULL)
        {
            routingtable->hash[hash_index]=routine_entry;
        }
        else
        {
            routine_entry->next=routingtable->hash[hash_index];
            routingtable->hash[hash_index]=routine_entry;
        }
    }
    pthread_mutex_unlock(routingtable_mutex);
}

void create_routeupdate_pkt(sip_pkt_t * pkt)
{
    pkt->header.src_nodeID = topology_getMyNodeID();
    pkt->header.type=ROUTE_UPDATE;
    pkt->header.dest_nodeID=BROADCAST_NODEID;

    //发送的节点数
    int node_num=topology_getNodeNum();
    pkt_routeupdate_t update_route_pkt;
    update_route_pkt.entryNum=node_num;
    for(int i=0;i<node_num;i++)
    {
        update_route_pkt.entry[i].cost=dv[0].dvEntry[i].cost;
        update_route_pkt.entry[i].nodeID=dv[0].dvEntry[i].nodeID;
    }

    memcpy(pkt->data,&update_route_pkt,sizeof(update_route_pkt));

    pkt->header.length=sizeof(update_route_pkt);

}

void dv_routingtable_delete_nextnode(int next_node)
{
    for(int index=0;index<MAX_ROUTINGTABLE_SLOTS;index++)
    {
        if(routingtable->hash[index]==NULL)
            continue;
        routingtable_entry_t * pre=routingtable->hash[index];
        routingtable_entry_t * after=routingtable->hash[index];

        assert(pre!=NULL);
        assert(after!=NULL);

        while(after!=NULL)
        {
            routingtable_entry_t *new_next=after->next;
            if(after->nextNodeID==next_node)
            {
                dvtable_setcost(dv,topology_getMyNodeID(),after->destNodeID,INFINITE_COST);
                free(after);
                pre->next=new_next;
                after=new_next;
            }
            else
            {
                if(pre==after)
                    after=after->next;
                else
                {
                    after=after->next;
                    pre=pre->next;
                }
            }
        }
    }
}
//defined in lab4-4 end

