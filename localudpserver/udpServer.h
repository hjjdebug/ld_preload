#ifndef _UDP_SERVER_H
#define _UDP_SERVER_H

#define LOCALUDPFILENAME "/tmp/local_alloc_udpfile.bin"
#define LOCALUDPFILENAME2 "/tmp/local_alloc_udpfile2.bin"

#define T_DEBUGON	200
#define T_DEBUGOFF	201
#define T_INIT		202
#define T_REPORT	203
#define T_RECYCLE	204

#define DEBUGON_CMD \
	TimeLog("debugon"); \
	g_debugon=1

#define DEBUGOFF_CMD \
	TimeLog("debugoff"); \
	g_debugon=0

#define CLEAR_CMD \
	TimeLog("clear.cmd"); \
	g_index = 0; \
	for (auto iter= g_mallocmap.begin(); iter!= g_mallocmap.end(); ++iter) \
	{ \
		delete iter->second.m_udp_packet; \
	} \
	g_mallocmap.clear();

#define STACKCOUNT 30
#pragma pack (1)
struct UDPPacket
{
	unsigned int mask;// 掩码, 确定该包是否是一个有效的UDPPacket
	unsigned char type;// 类型
	long int pointaddr;// 指针地址
	unsigned int size;// 分配的大小
	unsigned char stackcount;// 分配堆栈大小
	long int stack[STACKCOUNT];// 分配堆栈
};
#pragma pack ()


//从发来的packet中,提取了地址信息,添加了时间信息,归纳了类型信息
struct TransPacket
{
    int t_type = false;		// 只有malloc类型(true)和free类型(false), 再添加debugon(2),debugoff(3),init(4),report(5)
	long int t_addr = 0; //做为unorder_map 的键值,从UDPPacket pointaddr copy 而来
    long int t_time = 0; //添加了时间信息
    UDPPacket* t_packet = nullptr;
};

struct MallocPacket
{
	long int index = 0;	// 添加了index
    long int m_time = 0;		//从TransPacket copy 而来
    UDPPacket* m_udp_packet = nullptr; //从TransPacket copy 而来
};

// 回收包要慎用, 不小心会把程序搞死
#pragma pack (1)
struct UDPRecyclePacket
{
	unsigned int mask;// 掩码, 确定该包是否是一个有效的UDPPacket
	unsigned char type;// 类型
	long int pointaddr;// 指针地址
	unsigned int size;// 分配的大小
    long int c_time ;;
};
#pragma pack ()


//extern void RegisterSystemSignalHandler();
void OnUDPThread();
void report();
void recycle();
#endif
