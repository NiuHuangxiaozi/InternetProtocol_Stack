
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat. 
nbr_cost_entry_t* nbrcosttable_create() {
  //获得邻居的数量
  int nbr_num = topology_getNbrNum();

  //创建合适长度的邻居代价表
  nbr_cost_entry_t *nbr_array = (nbr_cost_entry_t *)
          malloc(sizeof(nbr_cost_entry_t) * nbr_num);

  //先拿到自己的NodeID和邻居节点的nodeIDs
  int myNodeID=topology_getMyNodeID();
  int * nbr_nodeIDs=topology_getNbrArray();

  //遍历这个vector添加nodeID和cost
  for (int nbr_index = 0; nbr_index < nbr_num; nbr_index++)
  {
    int nbr_nodeID=topology_getNodeIDfromip_val(nbr_nodeIDs[nbr_index]);
    int cost = topology_getCost(myNodeID,nbr_nodeID);

    //初始化邻居代价表
    nbr_array[nbr_index].nodeID=nbr_nodeID;
    nbr_array[nbr_index].cost=cost;
  }

  return nbr_array;
}

//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
  return;
}

//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
  return 0;
}
/*
 typedef struct neighborcostentry {
	unsigned int nodeID;	//邻居的节点ID
	unsigned int cost;	    //到该邻居的直接链路代价
} nbr_cost_entry_t;

 */
//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t* nct)
{
    printf("====The %d 's nbr cost table====\n",topology_getMyNodeID());
    int nbrs_num=topology_getNbrNum();
    for(int index=0;index<nbrs_num;index++)
    {
        printf("nbr index %d | nodeID %d | cost %d \n",index,nct[index].nodeID,nct[index].cost);
    }
    printf("================================\n");
}
