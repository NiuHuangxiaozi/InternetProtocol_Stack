#include "neighbortable.h"
#include "../topology/topology.h"
#include "stdlib.h"
#include "stdio.h"


int main()
{
    //启动重叠网络初始化工作
    printf("Overlay network: Node %d initializing...\n",topology_getMyNodeID());

    //创建一个邻居表
    nbr_entry_t *nt = nt_create();
    for(int i=0;i<3;i++)
    {
        printf("NODE Ip %d\n",nt[i].nodeIP);
        printf("NODE Id %d\n",nt[i].nodeID);
    }
    return 0;
}