/**************************************************************************
 * Filename: syn.h
 * Author: 贾少涵   Date: 2023.9.22
 * Description:
 *      · 定义半连接队列结构体并声明半连接队列及相关变量
 *      · 定义全连接队列并声明相关变量
 *      · 声明半连接队列和全连接队列的操作函数：初始化、加入元素和取出元素
 * Modification:
 *      2023.9.22--------添加头文件注释
**************************************************************************/

#ifndef SYN_H
#define SYN_H


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

void init_queue();                            // 初始化
void en_syn_queue(tju_tcp_t* sock, char* pkt);// 将socket加入半连接队列
tju_tcp_t* get_from_syn(char* pkt);           // 从半连接队列中取出socket
void en_accept_queue(tju_tcp_t* sock);        // 将socket加入全连接队列
tju_tcp_t* get_from_accept();                 // 从全连接队列中取出socket

// 以下线程自动平台用不了
void* syn_retrans_thread(void* arg);    // 半连接队列维护线程（包括超时重传）

#endif
