// �ļ���: stcp_server.h
//
// ����: ����ļ�����������״̬����, һЩ��Ҫ�����ݽṹ�ͷ�����STCP�׽��ֽӿڶ���. ����Ҫʵ��������Щ�ӿ�.

#ifndef STCPSERVER_H
#define STCPSERVER_H

#include <pthread.h>
#include "../common/seg.h"
#include "../common/constants.h"

//FSM��ʹ�õķ�����״̬
#define	CLOSED 1
#define	LISTENING 2
#define	CONNECTED 3
#define	CLOSEWAIT 4

//������������ƿ�. һ��STCP���ӵķ�������ʹ��������ݽṹ��¼������Ϣ.
typedef struct server_tcb {
	unsigned int server_nodeID;        //�������ڵ�ID, ����IP��ַ
	unsigned int server_portNum;       //�������˿ں�
	unsigned int client_nodeID;     //�ͻ��˽ڵ�ID, ����IP��ַ
	unsigned int client_portNum;    //�ͻ��˶˿ں�
	unsigned int state;         	//������״̬
	unsigned int expect_seqNum;     //�������ڴ����������	
	char* recvBuf;                  //ָ����ջ�������ָ��
	unsigned int  usedBufLen;       //���ջ��������ѽ������ݵĴ�С
	pthread_mutex_t* bufMutex;      //ָ��һ����������ָ��, �û��������ڶԽ��ջ������ķ���
} server_tcb_t;

//
//  ���ڷ�������Ӧ�ó����STCP�׽���API. 
//  ===================================
//
//  �����������ṩ��ÿ���������õ�ԭ�Ͷ����ϸ��˵��, ����Щֻ��ָ���Ե�, ����ȫ���Ը����Լ����뷨����ƴ���.
//
//  ע��: ��ʵ����Щ����ʱ, ����Ҫ����FSM�����п��ܵ�״̬, �����ʹ��switch�����ʵ��.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void stcp_server_init(int conn);

// ���������ʼ��TCB��, ��������Ŀ���ΪNULL. �������TCP�׽���������conn��ʼ��һ��STCP���ȫ�ֱ���, 
// �ñ�����Ϊsip_sendseg��sip_recvseg���������. ���, �����������seghandler�߳������������STCP��.
// ������ֻ��һ��seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_sock(unsigned int server_port);

// ����������ҷ�����TCB�����ҵ���һ��NULL��Ŀ, Ȼ��ʹ��malloc()Ϊ����Ŀ����һ���µ�TCB��Ŀ.
// ��TCB�е������ֶζ�����ʼ��, ����, TCB state������ΪCLOSED, �������˿ڱ�����Ϊ�������ò���server_port. 
// TCB������Ŀ������Ӧ��Ϊ�����������׽���ID�������������, �����ڱ�ʶ�������˵�����. 
// ���TCB����û����Ŀ����, �����������-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_accept(int sockfd);

// �������ʹ��sockfd���TCBָ��, �������ӵ�stateת��ΪLISTENING. ��Ȼ��������ʱ������æ�ȴ�ֱ��TCB״̬ת��ΪCONNECTED 
// (���յ�SYNʱ, seghandler�����״̬��ת��). �ú�����һ������ѭ���еȴ�TCB��stateת��ΪCONNECTED,  
// ��������ת��ʱ, �ú�������1. �����ʹ�ò�ͬ�ķ�����ʵ�����������ȴ�.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_recv(int sockfd, void* buf, unsigned int length);

// ��������STCP�ͻ��˵�����. �����STCPʹ�õ��ǵ�����, ���ݴӿͻ��˷��͵���������.
// �ź�/������Ϣ(��SYN, SYNACK��)����˫�򴫵�. �������ÿ��RECVBUF_POLLING_INTERVALʱ��
// �Ͳ�ѯ���ջ�����, ֱ���ȴ������ݵ���, ��Ȼ��洢���ݲ�����1. ����������ʧ��, �򷵻�-1.
//
// ע��: stcp_server_recv�ڷ������ݸ�Ӧ�ó���֮ǰ, �������ȴ��û�������ֽ���(��length)���������.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_close(int sockfd);

// �����������free()�ͷ�TCB��Ŀ. ��������Ŀ���ΪNULL, �ɹ�ʱ(��λ����ȷ��״̬)����1,
// ʧ��ʱ(��λ�ڴ����״̬)����-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void* seghandler(void* arg);

// ������stcp_server_init()�������߳�. �������������Կͻ��˵Ľ�������. seghandler�����Ϊһ������sip_recvseg()������ѭ��, 
// ���sip_recvseg()ʧ��, ��˵����SIP���̵������ѹر�, �߳̽���ֹ. ����STCP�ε���ʱ����������״̬, ���Բ�ȡ��ͬ�Ķ���.
// ��鿴�����FSM���˽����ϸ��.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//



//Defined by Niu in lab4-1
//��������������ڷ������ܵ���Ӧ�ı��ĺ�Ӧ�ý��еĶ�������seghandler�������е��á�
void action(int src_node,seg_t  *seg);


//�����������server���ܷ��İ�.
void Initial_stcp_control_seg(server_tcb_t * tcb,int Signal,seg_t  *syn);


//���������action�������洴�����̣߳�ר�ŵȴ�waitlossʱ�䣬Ȼ�����������closed��״̬��
void *close_wait_handler(void* arg);

//�����µ�tcb����ʼ����������port
int tcbtable_newtcb(unsigned int port);

//��÷�����tcb��ָ��
server_tcb_t * tcbtable_gettcb(int sockfd);

server_tcb_t  *tcbtable_recv_gettcb(seg_t * seg);
//Defined by Niu in lab4-1 end


//Defined by Niu in lab4-2
//�������������ݿ�������Ӧtcb��recvbuf����
void recvBuf_recv(server_tcb_t * tcb,seg_t *seg);


//�����������Ҫ�����ǽ��Ѿ�װ�����ݵ�tcb buf��������ݿ������û�
void recvBuf_copyToClient(server_tcb_t  * tcb,void * buf,unsigned  int length);
//Defined by Niu in lab4-2 End

#endif