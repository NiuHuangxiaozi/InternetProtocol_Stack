
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"

//makehash()是由路由表使用的哈希函数.
//它将输入的目的节点ID作为哈希键,并返回针对这个目的节点ID的槽号作为哈希值.
int makehash(int node)
{
  return 0;
}

//这个函数动态创建路由表.表中的所有条目都被初始化为NULL指针.
//然后对有直接链路的邻居,使用邻居本身作为下一跳节点创建路由条目,并插入到路由表中.
//该函数返回动态创建的路由表结构.
/*
 typedef struct routingtable {
	routingtable_entry_t* hash[MAX_ROUTINGTABLE_SLOTS];
} routingtable_t;
 typedef struct routingtable_entry {
	int destNodeID;		//目标节点ID
	int nextNodeID;		//报文应该转发给的下一跳节点ID
	struct routingtable_entry* next;	//指向在同一个路由表槽中的下一个routingtable_entry_t
} routingtable_entry_t;
 */

routingtable_t* routingtable_create()
{
  //创建
  routingtable_t * routine_table=(routingtable_t *) malloc(sizeof(routingtable_t));
  //全部初始化为NULL
  for(int index=0;index<MAX_ROUTINGTABLE_SLOTS;index++)
  {
    routine_table->hash[index]=NULL;
  }
  //建立邻居之间的直接连接
  int nbr_num=topology_getNbrNum();
  int * nbr_list=topology_getNbrArray();
  for(int nbr_index=0;nbr_index<nbr_num;nbr_index++)
  {
    routingtable_entry_t * routine_entry=(routingtable_entry_t *)
            malloc(sizeof(routingtable_entry_t));
    routine_entry->destNodeID=nbr_list[nbr_index];
    routine_entry->nextNodeID=nbr_list[nbr_index];
    routine_entry->next=NULL;

    //HASH PROCESS
    int hash_index=nbr_list[nbr_index]%MAX_ROUTINGTABLE_SLOTS;
    //get according POINT

    //头部插入
    if(routine_table->hash[hash_index]==NULL)
    {
        routine_table->hash[hash_index]=routine_entry;
    }
    else
    {
        routine_entry->next=routine_table->hash[hash_index];
        routine_table->hash[hash_index]=routine_entry;
    }
  }
  return routine_table;
}

//这个函数删除路由表.
//所有为路由表动态分配的数据结构将被释放.
void routingtable_destroy(routingtable_t* routingtable)
{
  return;
}

//这个函数使用给定的目的节点ID和下一跳节点ID更新路由表.
//如果给定目的节点的路由条目已经存在, 就更新已存在的路由条目.如果不存在, 就添加一条.
//路由表中的每个槽包含一个路由条目链表, 这是因为可能有冲突的哈希值存在(不同的哈希键, 即目的节点ID不同, 可能有相同的哈希值, 即槽号相同).
//为在哈希表中添加一个路由条目:
//首先使用哈希函数makehash()获得这个路由条目应被保存的槽号.
//然后将路由条目附加到该槽的链表中.
void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID)
{
  return;
}

//这个函数在路由表中查找指定的目标节点ID.
//为找到一个目的节点的路由条目, 你应该首先使用哈希函数makehash()获得槽号,
//然后遍历该槽中的链表以搜索路由条目.如果发现destNodeID, 就返回针对这个目的节点的下一跳节点ID, 否则返回-1.
int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID)
{
    int hash_index=destNodeID%MAX_ROUTINGTABLE_SLOTS;
    assert(hash_index<MAX_ROUTINGTABLE_SLOTS);
    assert(hash_index>=0);

    routingtable_entry_t * head=routingtable->hash[hash_index];
    while(head!=NULL)
    {
        if(head->destNodeID==destNodeID)
            return head->nextNodeID;
        head=head->next;
    }
  return -1;
}

/*
 typedef struct routingtable {
	routingtable_entry_t* hash[MAX_ROUTINGTABLE_SLOTS];
} routingtable_t;
 */
/*
 typedef struct routingtable_entry {
	int destNodeID;		//目标节点ID
	int nextNodeID;		//报文应该转发给的下一跳节点ID
	struct routingtable_entry* next;	//指向在同一个路由表槽中的下一个routingtable_entry_t
} routingtable_entry_t;
 */
//这个函数打印路由表的内容
void routingtable_print(routingtable_t* routingtable)
{
    printf("====The %d 's routingtable ====\n",topology_getMyNodeID());
    for(int index=0;index<MAX_ROUTINGTABLE_SLOTS;index++)
    {
        if(routingtable->hash[index]==NULL)
        {
            printf("Hash item :NULL \n");
        }
        else
        {
            printf("Hash item : \n");
            routingtable_entry_t * head=routingtable->hash[index];
            while(head!=NULL)
            {
                printf("| destNodeID %d , nextNodeID %d | ",head->destNodeID,head->destNodeID);
                head=head->next;
            }
        }
    }
    printf("==============================\n");
}


//defined in lab4-4
void routingtable_dest_delete(routingtable_t* routingtable,int dest_node)
{
    int hash_index=dest_node%MAX_ROUTINGTABLE_SLOTS;
    assert(hash_index<MAX_ROUTINGTABLE_SLOTS);
    assert(hash_index>=0);

    routingtable_entry_t * head=routingtable->hash[hash_index];

    if(head!=NULL&&head->destNodeID==dest_node)
    {
        routingtable_entry_t * new_head=head->next;
        free(head);
        routingtable->hash[hash_index]=new_head;
        return;
    }
    while(head->next!=NULL)
    {
        if (head->next->destNodeID == dest_node)
        {
            routingtable_entry_t * new_next=head->next->next;
            free(head->next);
            head->next=new_next;
            break;
        }
        head = head->next;
    }
}


//defiend in lab4-4 end
