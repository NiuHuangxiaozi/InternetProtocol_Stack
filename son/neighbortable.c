//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API

#include "neighbortable.h"

//defined in lab4-3
#include "../topology/topology.h"
#include "stdlib.h"
#include "stdio.h"
//defined in lab4-3 end

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t* nt_create()
{
    //获得邻居数量
    int nbr_nums=topology_getNbrNum();
    //创建邻居表
    nbr_entry_t * nbrs_table=(nbr_entry_t*)malloc(sizeof(nbr_entry_t)*nbr_nums);
    //然后填写邻居表
    int *ip_val_list=topology_getNbrArray();
    for(int index=0;index<nbr_nums;index++)
    {
        struct in_addr addr;
        addr.s_addr = ip_val_list[index];
        nbrs_table[index].nodeIP = addr.s_addr;
        nbrs_table[index].nodeID = topology_getNodeIDfromip(&addr);
        nbrs_table[index].conn = -1;
    }
    return nbrs_table;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt)
{
  return;
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{
  return 0;
}
