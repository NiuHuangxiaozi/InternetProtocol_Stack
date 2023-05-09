
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"



//这个函数动态创建距离矢量表.
//距离矢量表包含n+1个条目, 其中n是这个节点的邻居数,剩下1个是这个节点本身.
//距离矢量表中的每个条目是一个dv_t结构,它包含一个源节点ID和一个有N个dv_entry_t结构的数组, 其中N是重叠网络中节点总数.
//每个dv_entry_t包含一个目的节点地址和从该源节点到该目的节点的链路代价.
//距离矢量表也在这个函数中初始化.从这个节点到其邻居的链路代价使用提取自topology.dat文件中的直接链路代价初始化.
//其他链路代价被初始化为INFINITE_COST.
//该函数返回动态创建的距离矢量表.

//我们会创建一个(n+1)*(N)的矩阵

dv_t* dvtable_create()
{
    //获得邻居个数+1形成row的数目
    int nbr_num=topology_getNbrNum();
    //节点的总个数
    int table_num=topology_getNodeNum();

    //首先创建矩阵的行
    dv_t * vector_table=(dv_t *) malloc(sizeof(dv_t)*(nbr_num+1));

    //首先填写自己的表项
    //1获得自己邻居的nodelist
    int * nbr_list=topology_getNbrArray();
    int * all_list=topology_getNodeArray();

    //添加自己的nodeid
    vector_table[0].nodeID=topology_getMyNodeID();
    //创建对应的所有节点的数组
    vector_table[0].dvEntry=(dv_entry_t*)malloc(sizeof(dv_entry_t)*table_num);

    //向自己的表项里对应的数组里面添加对应的损失
    for(int index=0;index<table_num;index++)
    {
        //获得对应的id
        int temp_node_id=topology_getNodeIDfromip_val(all_list[index]);
        //获得对方的cost
        int cost= topology_getCost(temp_node_id,topology_getMyNodeID());

        //添加id
        vector_table[0].dvEntry[index].nodeID=temp_node_id;
        //添加cost
        vector_table[0].dvEntry[index].cost=cost;
    }


    //我们来填充邻居项的信息
    for(int nbr_index=0;nbr_index<nbr_num;nbr_index++)
    {
        //获得对应的邻居的nodeID
        int nbr_node=topology_getNodeIDfromip_val(nbr_list[nbr_index]);

        //填充dv_t[nbr_index+1].nodeID，+1是因为0代表mynodeid.
        assert(nbr_index+1<nbr_num+1);
        assert(nbr_index+1>0);
        vector_table[nbr_index+1].nodeID=nbr_node;
        //为列创造空间
        vector_table[nbr_index+1].dvEntry=(dv_entry_t*) malloc(sizeof(dv_entry_t)*table_num);
        //初始化上面的列里面的路由矢量信息
        for(int index=0;index<table_num;index++)
        {
            //获得对应的nodeid
            vector_table[nbr_index+1].dvEntry[index].nodeID=topology_getNodeIDfromip_val(all_list[index]);
            vector_table[nbr_index+1].dvEntry[index].cost=INFINITE_COST;
        }
    }
    return vector_table;
}

//这个函数删除距离矢量表.
//它释放所有为距离矢量表动态分配的内存.
void dvtable_destroy(dv_t* dvtable)
{
  return;
}

//这个函数设置距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,并且链路代价也被成功设置了,就返回1,否则返回-1.
int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost)
{
    int nbr_num=topology_getNbrNum();
    int all_num=topology_getNodeNum();
    for(int row=0;row<nbr_num+1;row++)
    {
        if(dvtable[row].nodeID==fromNodeID)
        {
            for(int col=0;col<all_num;col++)
            {
                if(dvtable[row].dvEntry[col].nodeID==toNodeID)
                {
                    dvtable[row].dvEntry[col].cost=cost;
                    return 1;
                }
            }
            break;
        }
    }
  return -1;
}

//这个函数返回距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,就返回链路代价,否则返回INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID)
{
  return 0;
}


/*
 typedef struct distancevectorentry {
	int nodeID;		    //目标节点ID
	unsigned int cost;	//到目标节点的代价
} dv_entry_t;

//一个距离矢量表包含(n+1)个dv_t条目, 其中n是这个节点的邻居数, 剩下的一个是这个节点自身.
typedef struct distancevector {
	int nodeID;		        //源节点ID
	dv_entry_t* dvEntry;	//一个包含N个dv_entry_t的数组, 其中每个成员包含目标节点ID和从该源节点到该目标节点的代价. N是重叠网络中总的节点数.
} dv_t;
 */
//这个函数打印距离矢量表的内容.
void dvtable_print(dv_t *dvtable)
{
  int nbr_num = topology_getNbrNum();
  int node_num = topology_getNodeNum();
  printf("====The %d 's distance vector table====\n", topology_getMyNodeID());
  for (int nbr_index = 0; nbr_index < nbr_num + 1; nbr_index++)
  {
    printf("The Node head %d | dvEntry begin :  ", dvtable[nbr_index].nodeID);
    for (int node_index = 0; node_index < node_num; node_index++)
    {
      printf("|Node %d Target Node %d Cost %d|",

             node_index, dvtable[nbr_index].dvEntry[node_index].nodeID,

             dvtable[nbr_index].dvEntry[node_index].cost);
    }
    printf("   dvEntry end \n");
  }
  printf("=======================================\n");
}

void dvtable_delete_point(dv_t * dvtable,int nodeID)
{
    int nbr_num=topology_getNbrNum();
    int all_num=topology_getNodeNum();
    for(int index=0;index<nbr_num+1;index++)
    {
        if(dvtable[index].nodeID==nodeID)
        {
            for(int tar_index=0;tar_index<all_num;tar_index++)
            {
                dvtable[index].dvEntry[tar_index].cost=INFINITE_COST;
            }
            break;
        }
    }
}
