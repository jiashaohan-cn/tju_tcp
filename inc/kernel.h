#ifndef _KERNEL_H_
#define _KERNEL_H_

#include "global.h"
#include "tju_packet.h"
#include <unistd.h>
#include "tju_tcp.h"

#define MAX_SOCK 32
tju_tcp_t* listen_socks[MAX_SOCK];
tju_tcp_t* established_socks[MAX_SOCK];

/*
-------自定义-------
*/
typedef struct 
{
  tju_tcp_t* sock;
  clock_t last_ack_time;  // 超时重传用
  uint16_t remands;   // 剩余发送次数
  char* packet_SYN_ACK; // 指向待发送SYN_ACK报文
} syn_elem;    // 半连接队列元素结构体

#define SYN_DEFAULT_REMANDS 10     // 半连接队列中syn_ack初始化发送次数

syn_elem syn_queue[MAX_SOCK];     // 半连接队列
uint16_t syn_num;       // 半连接队列元素个数
pthread_mutex_t syn_queue_lock;
tju_tcp_t* accept_queue[MAX_SOCK];  // 全连接队列
uint16_t accept_num;    // 全连接队列元素个数
pthread_mutex_t accept_queue_lock;

void init_queue();    // 初始化
void en_syn_queue(tju_tcp_t* sock, char* pkt);   // 将socket加入半连接队列
tju_tcp_t* get_from_syn(char* pkt);    // 从半连接队列中取出socket
void en_accept_queue(tju_tcp_t* sock);  // 将socket加入全连接队列
tju_tcp_t* get_from_accept();       // 从全连接队列中取出socket

// 以下线程自动平台用不了
void* syn_retrans_thread(void* arg);    // 半连接队列维护线程（包括超时重传）

/*
模拟Linux内核收到一份TCP报文的处理函数
*/
void onTCPPocket(char* pkt);


/*
以用户填写的TCP报文为参数
根据用户填写的TCP的目的IP和目的端口,向该地址发送数据报
*/
void sendToLayer3(char* packet_buf, int packet_len);


/*
开启仿真, 运行起后台线程
*/
void startSimulation();


/*
 使用UDP进行数据接收的线程
*/
void* receive_thread(void * in);

// 接受UDP的socket的标识符
int BACKEND_UDPSOCKET_ID;


/*
 linux内核会根据
 本地IP 本地PORT 远端IP 远端PORT 计算hash值 四元组 
 找到唯一的那个socket

 (实际上真正区分socket的是五元组
  还有一个协议字段
  不过由于本项目是TCP 协议都一样, 就没必要了)
*/
int cal_hash(uint32_t local_ip, uint16_t local_port, uint32_t remote_ip, uint16_t remote_port);

#endif