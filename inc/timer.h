/**************************************************************************
 * Filename: timer.h
 * Author: 贾少涵   Date: 2023.9.18
 * Description:
 *      · 声明发送线程和超时重传线程以及用于服务器关闭socket的辅助线程
 *      · 声明计时器相关函数：
 *          （1）开启计时器
 *          （2）关闭计时器
 *          （3）超时处理函数
 *      · 声明用于计算SampleRTT、RTO的函数
 * Modification:
 *      2023.9.22--------添加头文件注释
***************************************************************************/

#ifndef TIME_H
#define TIME_H

#include "global.h"


// 服务器用于关闭某个socket所用的线程
void* tju_close_thread(void* arg);

// 用于在发送缓冲区发送数据的线程
void* sending_thread(void* arg);
// 用于进行超时重传的线程
void* retrans_thread(void* arg);

// 计时器函数
void startTimer(tju_tcp_t *sock);   // 开启计时器
void stopTimer(void);               // 关闭计时器
void timeout_handler(int signo);    // 超时处理函数

void CalTimeout(tju_tcp_t *sock);   // 计算 SampleRTT 


#endif
