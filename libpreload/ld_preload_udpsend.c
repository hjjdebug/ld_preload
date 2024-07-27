#define _GNU_SOURCE
#include <dlfcn.h>       // for dlsym, RTLD_NEXT
#include <stdio.h>       // for NULL, size_t
#include <stdlib.h>      // for calloc, malloc, realloc, free, posix_memalign
#include <string.h>      // for memset
#include <sys/socket.h>  // for socket, AF_LOCAL, SOCK_DGRAM
#include <unistd.h>      // for sbrk
#include "ld_preload_udpsend.h"
#define LOCALUDPFILENAME "/tmp/local_alloc_udpfile.bin"

// 本地变量, 以下5个变量都是是否记录的依据
static int g_logflag = 1;// 记录日志开关
static int g_logminsize = 1;// 记录分配的最小字节数-小于该数字则不需要记录
static int g_logmaxsize = 10000000;// 记录分配的最大字节数-大于该数据则不需要记录
static  long int g_addrlow = 0;// 记录分配的内存区间段-最低内存区间（0表示不用设置）
static  long int g_addrhigh = 0;// 记录分配的内存区间段-最高内存区间（0表示不用设置）

static int g_hookinited = 0;
static int g_fd_socket = -1;
static struct sockaddr_un g_server_addr;
static void* (*g_malloc_real)(size_t) = NULL;
static void* (*g_calloc_real)(size_t,size_t) = NULL;
static void* (*g_realloc_real)(void*, size_t) = NULL;
static void (*g_free_real)(void*) = NULL;
static int (*g_posix_memalign_real)(void**,size_t align, size_t size) = NULL;
//static void* (*g_memalign_real)(size_t, size_t) = NULL;
//static void* (*g_valloc_real)(size_t) = NULL;
//static void (*g_udpDebugType)(void) = NULL;

// 101:malloc
// 102:calloc
// 103:realloc
// 104:posix_memalign
// 105:free
enum
{
	MALLOC_TYPE = 101,
	CALLOC_TYPE,
	REALLOC_TYPE,
	POSIX_MEMALIGN_TYPE,
	FREE_TYPE,
}Send_Type;
//保证该函数按正规调用栈来执行,不要被inline展开
#define STACKCALL __attribute__((regparm(1),noinline))
// 函数功能, 取到ebp寄存器值(本函数的基址),返回*ebp,即调用函数的基地址
static void** STACKCALL getEBP(void) {
	void** ebp = NULL;
	__asm__ __volatile__("mov %%rbp, %0;\n\t"
		:"=m"(ebp)
		: 
		: "memory");
	return (void**)(*ebp);
}

//功能：返回调用栈的大小及调用地址数组.
//它是如何实现这个神秘的功能的？请开下面描述
// my_backtrace 调用getEBP,得到my_backtrace 基地址，保存的是调用my_backtrace函数的调用地址
// 例如 以malloc钩子为例，第一层保存的是malloc钩子的调用地址
// 由my_backtrace 的基地址，可以依次向上追溯调用者的栈帧地址，并保存调用者的地址
// my_backtrace->malloc->main.c xxx 行
// 再向上因函数栈尺度太长而不再保存，循环退出

static int my_backtrace(void** caller_addr, int frame_capacity)
{
	int stack_count = 0; //堆栈大小
	void** base;
	void** ret = NULL;
	unsigned  long func_frame_distance = 0;
	if (caller_addr == NULL || frame_capacity <= 0) return 0;
	base = getEBP(); //获取到my_backtrace 的基地址
	func_frame_distance = (unsigned  long)(*base) - (unsigned  long)base;//计算栈帧长度
	while (base && stack_count < frame_capacity
		   && (func_frame_distance < (1ULL << 24))//假设自己的调用距离都小于16M, 系统调用main(),计算的调用距离是大于16M的
		   && (func_frame_distance > 0))
	{
		ret = base + 1;
		caller_addr[stack_count++] = *ret; //函数返回地址
		base = (void**)(*base);
		func_frame_distance = (unsigned  long)(*base) - (unsigned  long)base;
	}
	return stack_count;
}

static void  init_hooking()
{
	if(g_hookinited==1) return;

	g_malloc_real = dlsym(RTLD_NEXT, "malloc");
	g_calloc_real = dlsym(RTLD_NEXT, "calloc");
	g_realloc_real = dlsym(RTLD_NEXT, "realloc");
	g_free_real = dlsym(RTLD_NEXT, "free");
	g_posix_memalign_real = dlsym(RTLD_NEXT,"posix_memalign");

	//初始化server_addr
	bzero(&g_server_addr, sizeof(g_server_addr));
	g_server_addr.sun_family = AF_LOCAL;
	strcpy(g_server_addr.sun_path,LOCALUDPFILENAME);
	
	g_hookinited = 1;
	g_fd_socket = socket(AF_LOCAL, SOCK_DGRAM, 0);
/*
	printf("start init_hooking\n");
	printf("g_malloc_real at %p\n", g_malloc_real);
	printf("g_calloc_real at %p\n", g_calloc_real);
	printf("g_realloc_real at %p\n", g_realloc_real);
	printf("g_free_real at %p\n", g_free_real);
	printf("init_hooking() call finish!!!\n");
	printf("create local udp=%d\n", g_fd_socket);
	*/
}

void* malloc(size_t size)
{
	//printf("malloc(size_t size=%d) call\n", size);
	if (g_malloc_real == NULL) 
		init_hooking();
	struct UDPMallocPacket packet; //放在外边的好处是调试时可见范围大！

	void* ret = g_malloc_real(size);
    //printf("malloc(size_t size=%d) call, ptr=%p\n", size, ret);
	if (g_logflag==1 && size>=g_logminsize && size<=g_logmaxsize)
	{
		if (g_addrlow!=0 && ( long int)ret<g_addrlow)
		{
			return ret;
		}
		if (g_addrhigh!=0 && ( long int)ret>g_addrhigh)
		{
			return ret;
		}

		memset(&packet, 0, sizeof(packet));
		packet.mask = UDPMASK;
		packet.type = MALLOC_TYPE;
		packet.pointaddr = ( long int)(ret);
		packet.size = size;

//		CONSTRUCT_CALL_STACK;
		packet.stackcount = my_backtrace((void **)&packet.stack[0], STACKCOUNT); 
		
		SEND_PACKET;
	}

	return ret;
}

//dlsym 会调用calloc
void* calloc(size_t num, size_t size)
{
	struct UDPMallocPacket packet; //放在外边的好处是调试时可见范围大！
	
	void* ret = NULL;
	if (g_hookinited == 0)
	{
		ret = sbrk(num*size);
		//printf("calloc(size_t num=%d, size_t size=%d) call sbrk, ptr=%p\n", num, size, ret);
	}
	else
	{
		ret = g_calloc_real(num,size);

		if (g_logflag==1 && num*size>=g_logminsize && num*size<=g_logmaxsize)
		{
			if (g_addrlow!=0 && ( long int)ret<g_addrlow)
			{
				return ret;
			}
			if (g_addrhigh!=0 && ( long int)ret>g_addrhigh)
			{
				return ret;
			}

			memset(&packet, 0, sizeof(packet));
			packet.mask = UDPMASK;
			packet.type = CALLOC_TYPE;
			packet.pointaddr = ( long int)(ret);
			packet.size = size;

			CONSTRUCT_CALL_STACK;
			SEND_PACKET;
		}
	}

	return ret;
}


void* realloc(void* ptr, size_t size)
{
	//printf("realloc(void* ptr, size_t size=%d) call\n", size);
	if (g_realloc_real== NULL) 
	{
		init_hooking();
	}
	if (ptr!=NULL)
	{
		struct UDPFreePacket packet;
		if (g_logflag == 1)
		{
			memset(&packet, 0, sizeof(packet));
			packet.mask = UDPMASK;
			packet.type = FREE_TYPE;
			packet.pointaddr = ( long int)ptr;
			SEND_PACKET;
		}
	}
	
	void* ret = g_realloc_real(ptr, size);
	//printf("realloc, size=%d, oldptr=%p, newptr=%p\n", size, ptr, ret);
	struct UDPMallocPacket packet;
	if (g_logflag==1 && size>=g_logminsize && size<=g_logmaxsize)
	{
		if (g_addrlow!=0 && ( long int)ret<g_addrlow)
		{
			return ret;
		}
		if (g_addrhigh!=0 && ( long int)ret>g_addrhigh)
		{
			return ret;
		}

		memset(&packet, 0, sizeof(packet));
		packet.mask = UDPMASK;
		packet.type = REALLOC_TYPE;
		packet.pointaddr = ( long int)(ret);
		packet.size = size;

		CONSTRUCT_CALL_STACK;
		SEND_PACKET;
	}

	return ret;
}

void free(void* ptr)
{
	if (ptr == NULL)
		return;
	//printf("free call,ptr=%p\n", ptr);

	struct UDPFreePacket packet;
	if (g_logflag == 1)
	{
		if (g_addrlow!=0 && ( long int)ptr<g_addrlow)
		{
			g_free_real(ptr);
			return;
		}
		if (g_addrhigh!=0 && ( long int)ptr>g_addrhigh)
		{
			g_free_real(ptr);
			return;
		}

		memset(&packet, 0, sizeof(packet));
		packet.mask = UDPMASK;
		packet.type = FREE_TYPE;
		packet.pointaddr = ( long int)ptr;
		SEND_PACKET;
	}

	g_free_real(ptr);
    //printf("free call end,ptr=%p\n", ptr);
}


int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	int ret = 0;
	if (g_hookinited == 0)
	{
		init_hooking();
	}
	ret=g_posix_memalign_real(memptr,alignment,size);

	struct UDPMallocPacket packet;
	if (g_logflag==1 && size>=g_logminsize && size<=g_logmaxsize)
	{
		if (g_addrlow!=0 && ( long int)ret<g_addrlow)
		{
			return ret;
		}
		if (g_addrhigh!=0 && ( long int)ret>g_addrhigh)
		{
			return ret;
		}

		memset(&packet, 0, sizeof(packet));
		packet.mask = UDPMASK;
		packet.type = POSIX_MEMALIGN_TYPE;
		packet.pointaddr = ( long int)(*memptr);
		packet.size = size;

		CONSTRUCT_CALL_STACK;
		SEND_PACKET;
	}

	return ret;
}

void udpDebugType(int type)
{   //DEBUGON:200, DEBUGOFF:201, INIT:202, REPORT:203
	if(type<200 || type >204) return; //200,201,202,203

	struct UDPManagePacket packet;
	memset(&packet, 0, sizeof(packet));
	packet.mask = UDPMASK;
	packet.type = type;

	SEND_PACKET;
}
