#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>	// 使用 bool 类型
#include <string.h>
#include <unistd.h>
#include "global.h"
#include <pthread.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <signal.h>		// 使用 signal 函数

#define SERVER_IP "172.17.0.3"
#define CLIENT_IP "172.17.0.2"

// 初始化序列号(随机)----三次挥手建立连接使用
#define SERVER_ISN 0
#define CLIENT_ISN 0

// 关闭连接序列号（随机）----四次挥手关闭连接使用
#define FIN_SEQ 0

// 重传线程信号
bool RETRANS;
// 超时标志
bool TIMEOUT_FLAG;

// 单位是byte
#define SIZE32 4
#define SIZE16 2
#define SIZE8  1

// 一些Flag
#define NO_FLAG 0
#define NO_WAIT 1
#define TIMEOUT 2
#define TRUE 1
#define FALSE 0

// 定义最大包长 防止IP层分片
#define MAX_DLEN 1375 	// 最大包内数据长度
#define MAX_LEN 1400 	// 最大包长度

// 最大发送窗口大小
#define MAX_SWINDOW_SIZE 32*MAX_DLEN
// 最大接收窗口大小
#define MAX_RWINDOW_SIZE 32*MAX_DLEN

// 最大发送和接收缓冲区大小
#define MAX_SOCK_BUF_SIZE 6000*MAX_LEN	// 发送缓冲区和接收缓冲区的大小至少为 5000 个 MSS

// TCP socket 状态定义
#define CLOSED 0
#define LISTEN 1
#define SYN_SENT 2
#define SYN_RECV 3
#define ESTABLISHED 4
#define FIN_WAIT_1 5
#define FIN_WAIT_2 6
#define CLOSE_WAIT 7
#define CLOSING 8
#define LAST_ACK 9
#define TIME_WAIT 10

// TCP 拥塞控制状态
#define SLOW_START 0
#define CONGESTION_AVOIDANCE 1
#define FAST_RECOVERY 2

// 最短超时重传间隔 (200毫秒)
#define TCP_RTO_MIN 200000

// TCP 发送窗口
// 注释的内容如果想用就可以用 不想用就删掉 仅仅提供思路和灵感
typedef struct {
	uint32_t window_size;	// 当前发送窗口大小-----------------------------------滑动窗口
	uint32_t ack_cnt;		// 缓冲区中已确认的字节数------------------------------滑动窗口
  	uint32_t base;		// 发送窗口起始位置序号------------------------------------滑动窗口
    uint32_t nextseq;   // 下一个待发送序号----------------------------------------滑动窗口
	uint32_t same_ack_cnt;	// 连续重复ack数量------------------------------------快速重传
	uint64_t estmated_rtt;	// 期望 RTT-------------------------------------------计算RTO
	uint64_t dev_rtt;		// 方差 RTT-------------------------------------------计算RTO
	bool is_estimating_rtt;	// 是否在测量 SampleRTT-------------------------------计算RTO
	uint32_t rtt_expect_ack;	// 用来测量RTT的报文期待的ACK号--------------------计算RTO
  	struct timeval send_time;	// 记录发送时间-----------------------------------计算RTO
  	struct itimerval timeout;		// 记录超时重传间隔---------------------------记录RTO
  	uint32_t rwnd; 		// 发送端接收窗口大小----拥塞窗口--------------------------流量控制
  	int window_status;	// 该窗口拥塞控制状态-------------------------------------拥塞控制
  	uint32_t ssthresh; 	// 拥塞阈值-----------------------------------------------拥塞控制
} sender_window_t;

// TCP 接受窗口
// 注释的内容如果想用就可以用 不想用就删掉 仅仅提供思路和灵感
typedef struct {
	uint32_t remain_size;	// 接收窗口剩余空间大小
	char* recv_buf;			// 接收窗口缓冲区
//   received_packet_t* head;
//   char buf[TCP_RECVWN_SIZE];
	uint8_t* mark;	// 标记位
  	uint32_t expect_seq;	// 期望序号
} receiver_window_t;

// TCP 窗口 每个建立了连接的TCP都包括发送和接受两个窗口
typedef struct {
	sender_window_t* wnd_send;
  	receiver_window_t* wnd_recv;
} window_t;

typedef struct {
	uint32_t ip;
	uint16_t port;
} tju_sock_addr;


// TJU_TCP 结构体 保存TJU_TCP用到的各种数据
typedef struct {
	int state; // TCP的状态

	tju_sock_addr bind_addr; // 存放bind和listen时该socket绑定的IP和端口
	tju_sock_addr established_local_addr; // 存放建立连接后 本机的 IP和端口
	tju_sock_addr established_remote_addr; // 存放建立连接后 连接对方的 IP和端口

	pthread_mutex_t send_lock; // 发送数据锁
	char* sending_buf; // 发送数据缓存区
	int sending_len; // 发送数据缓存长度
	int have_send_len;	// 已发送数据长度

	pthread_mutex_t recv_lock; // 接收数据锁
	char* received_buf; // 接收数据缓存区
	int received_len; // 接收数据缓存长度

	pthread_cond_t wait_cond; // 可以被用来唤醒recv函数调用时等待的线程

	window_t window; // 发送和接受窗口

	bool is_retransing;		// 表明是否在重传

	char* packet_FIN;	// FIN或FIN_ACK包（关闭连接超时重传用）

} tju_tcp_t;

#endif