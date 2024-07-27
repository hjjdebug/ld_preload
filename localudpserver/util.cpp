#include <stdio.h> // for FILE
#include <string.h>
#include <time.h> // for time, localtime, time_t
#include <unordered_map> // fr unordered_map
#include <sys/socket.h> // for AF_LOCAL 
#include <sys/un.h> // for sockaddr_un
#include <vector>
#include <algorithm>
#include "udpServer.h"
#include "util.h"
using namespace std;
extern std::unordered_map<long int, MallocPacket> g_mallocmap;
extern int g_fd_socket2;
long int getsystemtime()
{
	return time(NULL);
	/*
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
	*/
}
//向文件和控制台输出时间和信息
void TimeLog(const char* msg)
{
	time_t t = time(NULL);
	tm timeTm;
	localtime_r(&t, &timeTm);

	FILE* fp = fopen("./log.txt", "a+");

	char timeinfo[256] = "";
	sprintf(timeinfo, "%04d-%02d-%02d_%02d:%02d:%02d", timeTm.tm_year + 1900, timeTm.tm_mon + 1, timeTm.tm_mday, timeTm.tm_hour, timeTm.tm_min, timeTm.tm_sec);	
	char buffer[4096] = "";
	sprintf(buffer, "%s %s\n", timeinfo, msg);
	fwrite(buffer, 1, strlen(buffer), fp);
	fclose(fp);
	
	printf("%s", buffer);
}
bool comp(MallocPacket* p1, MallocPacket*p2)
{
	return p1->index < p2->index;
}
const char *getStringType(int type)
{
	switch(type)
	{
	case 101:
		return "malloc";
	case 102:
		return "calloc";
	case 103:
		return "realloc";
	case 104:
		return "memalign";
	default:
		return "unkown";
	}
}
void report()
{// 打印监听
	TimeLog("report.cmd");

	// 打印信息
	time_t t = time(NULL);
	tm timeTm;
	localtime_r(&t, &timeTm);
	char filename[128] = "";
	sprintf(filename, "./report-%04d-%02d-%02d_%02d:%02d:%02d.txt", timeTm.tm_year + 1900, timeTm.tm_mon + 1, timeTm.tm_mday, timeTm.tm_hour, timeTm.tm_min, timeTm.tm_sec);
	FILE* fp = fopen(filename, "a+");
	// header
//	char header[1024] = "index	time	addr	size	stackcount	stacklist\n";
	char header[1024];
	sprintf(header,"%-10s\t %-10s\t %-16s\t %-10s\t %-10s\t %-10s\t %s\n","index","time","addr","size","type","stackcount","stacklist");
	fwrite(header, 1, strlen(header), fp);
	// body
	char body[2048]; 
	char stackinfo[1024];
	char seg[64];
	struct tm*	p_tm;
	char szTime[64];
	vector<MallocPacket *>vec;
	// 先把g_mallocmap无序映射的数值变成一个数组,排序后再输出
	for (auto iter= g_mallocmap.begin(); iter!= g_mallocmap.end(); ++iter)
	{
		vec.push_back(&iter->second);
	}
	sort(vec.begin(),vec.end(),comp);
//	for (auto iter= vec.begin(); iter!= vec.end(); ++iter) //这里的iter是元素指针
	for (auto iter:vec) // 这里的iter是元素
	{
		body[0]=0;
		stackinfo[0]=0;
		for (int i=0; i<iter->m_udp_packet->stackcount; i++)
		{
			sprintf(seg, "%d:0x%lx ", i, iter->m_udp_packet->stack[i]);
			strcat(stackinfo,seg);
		}
		p_tm=localtime(&iter->m_time);
		sprintf(szTime,"%02d:%02d:%02d",p_tm->tm_hour,p_tm->tm_min,p_tm->tm_sec);
		const char *szType = getStringType(iter->m_udp_packet->type);
		sprintf(body, "%-10ld\t %-10s\t 0x%-14lx\t %-10d\t %-10s\t %-10d\t %s\n", iter->index, szTime, iter->m_udp_packet->pointaddr, iter->m_udp_packet->size, szType,iter->m_udp_packet->stackcount, stackinfo);

		fwrite(body, 1, strlen(body), fp);
	}
	vec.clear();
	fclose(fp);
}


//删除5分钟之前的内存,慎用,已关闭调用
#define DELETE_BEFORE_TIME 30
void recycle()
{
	time_t now = time(NULL);
	UDPRecyclePacket packet;
	long recycle_size=0;
	for (auto iter= g_mallocmap.begin(); iter!= g_mallocmap.end(); ++iter)
	{
		if((now-iter->second.m_time) > DELETE_BEFORE_TIME)
		{
			if(iter->second.m_udp_packet->size >= 1000) continue;
			memset(&packet, 0, sizeof(packet));
			packet.mask = 78543505;			//UDPMASK
			packet.type = 104;
			packet.pointaddr = iter->first;
			packet.size = iter->second.m_udp_packet->size;
			packet.c_time = iter->second.m_time;
			recycle_size+=packet.size;

			struct sockaddr_un addr;
			bzero(&addr, sizeof(addr));
			addr.sun_family = AF_LOCAL;
			strcpy(addr.sun_path, LOCALUDPFILENAME2);
			sendto(g_fd_socket2, &packet, sizeof(packet), 0, (const sockaddr *)&addr, sizeof(addr));
		}
	}
	printf("recycled mem:%ld\n",recycle_size);
}
