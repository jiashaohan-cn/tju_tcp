/*****************************************************
 * Filename: timer.c
 * Author: 贾少涵   Date: 2023.9.18
 * Description: 定义头文件"timer.h"中声明的相关函数
******************************************************/

#include "global.h"
#include "timer.h"
#include "log.h"
#include "tju_tcp.h"



// 用于服务器被动调用关闭连接（handle_packet函数中）
void* tju_close_thread(void* arg){
    tju_tcp_t* sock=(tju_tcp_t* )arg;
    tju_close(sock);
}
// 用于在发送缓冲区发送数据的线程（connect和accept函数中）
void* sending_thread(void* arg){
    tju_tcp_t* sock=(tju_tcp_t *)arg;

    while (1){
        // 发送窗口中有未发送的数据 & 首次发送（非重传）
        if (sock->window.wnd_send->window_size\
                &&sock->have_send_len<sock->sending_len\
                &&sock->have_send_len<sock->window.wnd_send->ack_cnt+sock->window.wnd_send->window_size\
                &&!sock->is_retransing){
        
            uint32_t wnd_size=sock->window.wnd_send->window_size;
            uint32_t wnd_ack_cnt=sock->window.wnd_send->ack_cnt;
            uint32_t wnd_base=sock->window.wnd_send->base;
            uint32_t wnd_nextseq=sock->window.wnd_send->nextseq;

            pthread_mutex_lock(&sock->send_lock);   // 加锁
            // 只发送窗口中未发送的数据
            uint32_t tmp_len=min(sock->sending_len,wnd_ack_cnt+wnd_size);
            while (sock->have_send_len<tmp_len){
                char *data=sock->sending_buf+sock->have_send_len;
                int dlen;   // 数据长度
                uint16_t plen;  // 数据包总长
                if (tmp_len-sock->have_send_len<=MAX_DLEN){
                    dlen=tmp_len-sock->have_send_len;
                    plen=DEFAULT_HEADER_LEN+dlen;
                    char *packet1=create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,wnd_nextseq,0,\
                            DEFAULT_HEADER_LEN,plen,NO_FLAG,1,0,data,dlen);
                    sendToLayer3(packet1,plen);
                    _SEND_LOG_(packet1);
                    free(packet1);
                }
                else{
                    dlen=MAX_DLEN;
                    plen=DEFAULT_HEADER_LEN+dlen;
                    char *packet2=create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,wnd_nextseq,0,\
                            DEFAULT_HEADER_LEN,plen,NO_FLAG,1,0,data,dlen);
                    sendToLayer3(packet2,plen);
                    _SEND_LOG_(packet2);
                    free(packet2);
                }
                printf("首次发送 %d 字节大小的报文 seq = %d\n", dlen, wnd_nextseq);

                // 开始计算 SampleRTT
                if (sock->window.wnd_send->is_estimating_rtt == false){
                    sock->window.wnd_send->is_estimating_rtt = TRUE;
                    gettimeofday(&sock->window.wnd_send->send_time, NULL);
                    sock->window.wnd_send->rtt_expect_ack=wnd_nextseq+dlen;
                }

                // 如果发送窗口的base和nextseq一样 说明是窗口的第一个 为其启动计时器
                if (wnd_base == wnd_nextseq)
                {
                    startTimer(sock);
                    sock->is_retransing=TRUE;
                }

                // 更新缓冲区及临时窗口参数
                sock->have_send_len+=dlen;
                wnd_nextseq+=dlen;
            }

            // 更新窗口参数
            sock->window.wnd_send->nextseq=wnd_nextseq;

            pthread_mutex_unlock(&sock->send_lock);  // 解锁
        }
    }
}
void* retrans_thread(void* arg){    // 用于进行超时重传的线程
    tju_tcp_t *sock=(tju_tcp_t *)arg;
    while (1){
        if (RETRANS){
            if (sock->window.wnd_send->window_size==0){
                printf("发送0窗口探测报文\n");

                char* get_swnd_packet=create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,sock->window.wnd_send->base-1,0,\
                        DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,NO_FLAG,1,0,NULL,0);
                sendToLayer3(get_swnd_packet,DEFAULT_HEADER_LEN);
                _SEND_LOG_(get_swnd_packet);
                free(get_swnd_packet);
                startTimer(sock);
                continue;
            }

            // 没有触发超时重传
            if (sock->is_retransing == FALSE){
                RETRANS=FALSE;
                if (sock->window.wnd_send->ack_cnt<sock->have_send_len){
                    printf("发送窗口还有未收到确认的数据\n");
                    sock->is_retransing = TRUE;
                    RETRANS=TRUE;
                }
                continue;
            }

            pthread_mutex_lock(&(sock->send_lock));   // 加锁
            // printf("开始重传操作\n");
            
            // 如果是超时重传的话要更新窗口参数
            if (TIMEOUT_FLAG){
                // 拥塞阈值变为拥塞窗口大小的一半
                sock->window.wnd_send->ssthresh=sock->window.wnd_send->cwnd>>1;
                // 当前窗口大小变为一个MSS
                sock->window.wnd_send->cwnd=MAX_DLEN;
                _CWND_LOG_(sock,TIMEOUT);
                // 更新窗口拥塞控制状态为--慢启动
                sock->window.wnd_send->window_status=SLOW_START;
                // 同时更新发送窗口
                sock->window.wnd_send->window_size=min(sock->window.wnd_send->cwnd,sock->window.wnd_send->rwnd);
                _SWND_LOG_(sock);
            }

            // 声明一些临时变量
            uint32_t wnd_ack_cnt=sock->window.wnd_send->ack_cnt;
            uint32_t have_sent=sock->have_send_len;
            uint32_t wnd_base=sock->window.wnd_send->base;

            while (wnd_ack_cnt<have_sent){
                char* data=sock->sending_buf+wnd_ack_cnt;
                uint32_t dlen;
                if (have_sent-wnd_ack_cnt<=MAX_DLEN){   // 一次发完
                    dlen=have_sent-wnd_ack_cnt;
                    char* packet1=create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,wnd_base,0,\
                            DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN+dlen,NO_FLAG,1,0,data,dlen);
                    sendToLayer3(packet1,DEFAULT_HEADER_LEN+dlen);
                    _SEND_LOG_(packet1);
                    free(packet1);
                }
                else{
                    dlen=MAX_DLEN;
                    char* packet2=create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,wnd_base,0,\
                            DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN+dlen,NO_FLAG,1,0,data,dlen);
                    sendToLayer3(packet2,DEFAULT_HEADER_LEN+dlen);
                    _SEND_LOG_(packet2);
                    free(packet2);
                }
                printf("重新发送 %d 字节大小的报文 seq = %d\n", dlen, wnd_base);

                // 启动计时器
                if (wnd_ack_cnt==sock->window.wnd_send->ack_cnt){
                    startTimer(sock);
                    sock->is_retransing = TRUE;
                }

                // 更新临时参数
                wnd_ack_cnt+=dlen;
                wnd_base+=dlen;
            }
            // printf("重传结束\n");
            // 更新窗口参数
            RETRANS=FALSE;
            sock->is_retransing=FALSE;
            pthread_mutex_unlock(&(sock->send_lock));   // 解锁
        }
    }
}

void startTimer(tju_tcp_t *sock){   // 开启计时器
    struct itimerval tick;
    RETRANS = FALSE;
    TIMEOUT_FLAG = FALSE;

    // 确定超时重传间隔
    memset(&tick, 0, sizeof(tick));
    memcpy(&tick, &sock->window.wnd_send->timeout, sizeof(tick));

    // 开启计时器
    if (setitimer(ITIMER_REAL, &tick, NULL) < 0){
        printf("Set timer failed!\n");
        // exit(-1);
    }
    // printf("START TIMER\n");
}
void stopTimer(void){   // 关闭计时器
    struct itimerval value;
    value.it_value.tv_sec = 0;
    value.it_value.tv_usec = 0;
    value.it_interval.tv_sec = 0;
    value.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &value, NULL);
    // printf("STOP TIMER\n");
}
void timeout_handler(int signo){    // 超时处理函数
    // printf("调用超时处理函数\n");
    RETRANS=TRUE;
    TIMEOUT_FLAG = TRUE;
}

void CalTimeout(tju_tcp_t *sock){   // 计算 SampleRTT 
    // printf("进入计算 SampleRTT 函数\n");

    // 获取发送时间和接收时间
    struct timeval send_time = sock->window.wnd_send->send_time;
    struct timeval local_time;
    gettimeofday(&local_time, NULL);

    // 得到 SampleRTT
    long sampleRTT = (local_time.tv_sec - send_time.tv_sec) * 1000000 + (local_time.tv_usec - send_time.tv_usec);
    // printf("sampleRTT = %ld \n", sampleRTT);

    // 计算期望 RTT
    sock->window.wnd_send->estmated_rtt = 0.875 * sock->window.wnd_send->estmated_rtt + 0.125 * sampleRTT;

    // 计算方差 RTT
    sock->window.wnd_send->dev_rtt = 0.75 * sock->window.wnd_send->dev_rtt + 0.25 * abs(sampleRTT - sock->window.wnd_send->estmated_rtt);

    // 计算超时重传间隔 RTO
    sock->window.wnd_send->timeout.it_value.tv_usec = sock->window.wnd_send->estmated_rtt + 4 * sock->window.wnd_send->dev_rtt;

    _RTTS_LOG_(sock,sampleRTT);
}
