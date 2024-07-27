#include <stdio.h> // for printf, sprintf, remove, fwrite
#include <unistd.h> // for unlink
#include <sys/socket.h> // for socket, bind AF_LOCAL, recvfrom
#include <sys/un.h> // for sockaddr_un
#include <thread> // for thread
#include <unordered_map> //for unordered_map
#include <mutex> // for mutex
#include <chrono> // for milliseconds
#include <list> // for list
#include <signal.h> // for signal, SIG_IGN, SIGHUP
#include "udpServer.h"
#include "util.h"

//hjj change

int g_threadrun = 1;
int g_fd_socket = -1;  // 
int g_fd_socket2 = -1; // server发回客户端的socket

long int g_index = 0;
int g_debugon =1;

std::mutex g_listlock;
int g_listcount = 0;		// list 也可以不要g_translistcount
std::list<TransPacket> g_list;
#define LISTADDONE_TRANSPORT_PACKET \
	g_listlock.lock(); \
	g_list.push_back(t_pack); \
	g_listcount++; \
	g_listlock.unlock();

std::unordered_map<long int, MallocPacket> g_mallocmap;

#define BUFFER_SIZE 300
void OnUDPThread()
{
	char buf[BUFFER_SIZE] = "";
	struct sockaddr_un client_addr;
	socklen_t client_len = sizeof(client_addr);
	TransPacket t_pack;
	TimeLog("OnUDPThread start");
	//核心是构建t_pack 并保存到链表g_list
	while (g_threadrun)
	{
		bzero(buf, sizeof(buf));
		//接受的是UDPPacket
		int n=recvfrom(g_fd_socket, buf, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_len);
		if (n<=0)
		{
			TimeLog("client udp recvfrom error");
			break;
		}
//		printf("receive data ... \n");
		int mask = *((int*)buf);
		if (mask != 78543505) continue;

		char* p = (buf + 4);
		unsigned char type = p[0];
		//带内存分配的需要有负载,不带内存分配的不需要负载
		if (type == 101 || type == 102 || type == 103 ||type == 104)
		{// malloc
			UDPPacket* pUDPPacket = new UDPPacket();
			memcpy(pUDPPacket, buf, sizeof(UDPPacket));	// 把发来的udp包保存下来.

            t_pack.t_type = 1;
			p = (p + 1);
			long int addr = *((long int*)p);
			t_pack.t_addr = addr;
            t_pack.t_time = getsystemtime();		//添加时间信息. 并提取了udp包地址信息和类型信息构建t_pack
            t_pack.t_packet = pUDPPacket;

			LISTADDONE_TRANSPORT_PACKET;
		}
		else if (type == 105)
		{// free

            t_pack.t_type = 0;
			p = (p + 1);
			long int addr = *((long int*)p);
			t_pack.t_addr = addr;

			LISTADDONE_TRANSPORT_PACKET;
		}
		else if(type >=T_DEBUGON && type <=T_RECYCLE)
		{
			switch(type)
			{
				case T_DEBUGON:
					t_pack.t_type = 2;
					break;
				case T_DEBUGOFF:
					t_pack.t_type = 3;
					break;
				case T_INIT:
					t_pack.t_type = 4;
					break;
				case T_REPORT:
					t_pack.t_type = 5;
					break;
				case T_RECYCLE:
					t_pack.t_type = 6;
					break;
				default:
					t_pack.t_type = -1;
			}
			if(t_pack.t_type != -1)
			{
				LISTADDONE_TRANSPORT_PACKET;
			}
		}
	}
	TimeLog("OnUDPThread end");
}


int main()
{
	printf("===============================\n");

	printf("Usesage:\n");
	printf("touch debugon.cmd open Console debug, defaut on .\n");
	printf("touch debugoff.cmd close Console debug .\n");
	printf("touch clear.cmd clear current stack and listen.\n");
	printf("touch report.cmd began report.\n");
	printf("===============================\n");


//	RegisterSystemSignalHandler();
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);


	TimeLog("start create local udp socket");
	g_fd_socket = socket(AF_LOCAL, SOCK_DGRAM, 0);
	if (g_fd_socket < 0)
	{
		TimeLog("local udp socket create failed");
		return 0;
	}
	TimeLog("create local udp socket succeed");

	TimeLog("start bind local udp socket");
	unlink(LOCALUDPFILENAME);
	struct sockaddr_un servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_LOCAL;
	strcpy(servaddr.sun_path, LOCALUDPFILENAME);
	if (bind(g_fd_socket, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
		TimeLog("local udp bind failed");
		return 0;
	}
	TimeLog("bind local udp socket succeed");

	TimeLog("start create receive thread");
	std::thread udpthread(OnUDPThread);
	udpthread.detach();
	TimeLog("create receive thread succeed");

//	g_fd_socket2 = socket(AF_LOCAL, SOCK_DGRAM, 0);
// 核心是处理g_list, 将t_pack 转成m_pack存储到map,或从map中删除, map 就是我们要的东西.
	TransPacket t_pack;
	MallocPacket m_pack;
	while (true)
	{
		for (int i=0; i<1000; i++) // 最多处理1000个trans 包,然后检查指令
		{
			bool has = false;
			g_listlock.lock();
			if (g_listcount > 0)
			{
				g_listcount--;
				t_pack = g_list.front();
				g_list.pop_front();
				has = true;
			}
			g_listlock.unlock();

			if (!has) break;

			// 插入,由t_pack 构建m_pack并插入map
            if (t_pack.t_type==1)
			{// malloc
				auto iter =g_mallocmap.find(t_pack.t_addr);
                if (iter != g_mallocmap.end())
				{
					printf("alloc> addess redudant!: %ld\n", t_pack.t_addr);
                    //删除以前分配的包
					delete iter->second.m_udp_packet;  // second 是MallocPacket,删除其对应的udpPacket
                    g_mallocmap.erase(t_pack.t_addr); // 删除map 项
				}

				//构建m_pack
				m_pack.index = g_index;
				m_pack.m_time = t_pack.t_time;
				m_pack.m_udp_packet = t_pack.t_packet;
				g_mallocmap[t_pack.t_addr] = m_pack;
				g_index++;

				if(g_debugon)
				{
					char szTime[64];
					struct tm *p_tm=localtime(&m_pack.m_time);
					sprintf(szTime,"%02d:%02d:%02d",p_tm->tm_hour,p_tm->tm_min,p_tm->tm_sec);
					printf("alloc> i:%ld tp:%d addr:%p s:%d skc:%d t:%s\n",\
							m_pack.index,\
							m_pack.m_udp_packet->type,\
							(void *)t_pack.t_addr,\
							m_pack.m_udp_packet->size, \
							m_pack.m_udp_packet->stackcount, \
							szTime);
				}
			}
			else if(t_pack.t_type==0) //删除m_pack 并从map中移除
			{// free
                auto iter = g_mallocmap.find(t_pack.t_addr);
                if (iter == g_mallocmap.end())
				{
					printf("free> addr none exist: %p\n", (void *)t_pack.t_addr); // m_mallocmap 没有记录该地址
				}
				else //从g_mallocmap 中删除该地址
				{
					if(g_debugon) // free 打印的type 是查到的内存中分配的type,地址，大小也是一样
					{
						printf("free> i:%ld tp:%d addr:%p s:%d\n",\
							(*iter).second.index,\
							(*iter).second.m_udp_packet->type,\
							(void *)t_pack.t_addr,\
							(*iter).second.m_udp_packet->size);
					}
					delete iter->second.m_udp_packet;  // second 是MallocPacket
                    g_mallocmap.erase(t_pack.t_addr);
				}
			}
			else
			{  //判断传输包中是否有CMD命令
				switch(t_pack.t_type)
				{
					case 2:
						DEBUGON_CMD;
						break;
					case 3:
						DEBUGOFF_CMD;
						break;
					case 4:
						CLEAR_CMD;
						break;
					case 5:
						report();
						break;
					case 6:
						recycle();
						break;
					default:
						;
				}
			}
		}

		// 判断控制台中有无CMD指令
		if (remove("quit.cmd") != -1)
		{// 退出循环
			TimeLog("quit.cmd");
			g_threadrun = 0;	// 线程退出
			break;	//主线程退出
		}
		if(remove("debugon.cmd")!=-1)
		{
			DEBUGON_CMD;
		}
		else if(remove("debugoff.cmd")!=-1)
		{
			DEBUGOFF_CMD;
		}
		if (remove("init.cmd") != -1)
		{// 初始化g_index 及g_mallocmap
			CLEAR_CMD;
		}
		if (remove("clear.cmd") != -1)
		{// 初始化g_index 及g_mallocmap
			CLEAR_CMD;
		}
		if (remove("report.cmd") != -1)
		{
			report();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	TimeLog("end");
	return 0;
}

