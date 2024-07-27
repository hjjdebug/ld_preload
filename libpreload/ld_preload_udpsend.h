#ifndef _MALLOC_PRELOAD_LOCALUDP_H
#define _MALLOC_PRELOAD_LOCALUDP_H
#include <sys/un.h> //for struct sockaddr_un

#define STACKCOUNT 30
#pragma pack (1)
struct UDPMallocPacket
{
	unsigned int mask;// 掩码
	unsigned char type;// 类型
	long int pointaddr;// 指针地址
	unsigned int size;// 分配的大小
	unsigned char stackcount;// 分配堆栈的大小
	long int stack[STACKCOUNT];// 分配堆栈
};
#pragma pack ()

#pragma pack (1)
struct UDPFreePacket
{
	unsigned int mask;// 掩码
	unsigned char type;// 类型
	long int pointaddr;// 指针地址
}packet;
#pragma pack ()

#pragma pack (1)
struct UDPManagePacket
{
	unsigned int mask;// 掩码
	unsigned char type;// 类型, 200(debugon),201(debugoff),202(init),203(report)
};
#pragma pack ()

#define UDPMASK 78543505
//此处的packet 是各种类型的packet!!
#define SEND_PACKET														\
	sendto(g_fd_socket, &packet, sizeof(packet),0,&g_server_addr,sizeof(g_server_addr));


#define CONSTRUCT_CALL_STACK											\
	void* caller_addr[STACKCOUNT];										\
	packet.stackcount = (unsigned char)my_backtrace(caller_addr, STACKCOUNT); \
	unsigned char i = 0;												\
	for (i = 0; i < packet.stackcount; i++)								\
	{																	\
		packet.stack[i] = ( long int)caller_addr[i];					\
	}

	

#endif
