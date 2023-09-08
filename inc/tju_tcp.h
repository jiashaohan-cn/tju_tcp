#ifndef _TJU_TCP_H_
#define _TJU_TCP_H_

#include "global.h"
#include "tju_packet.h"
#include "kernel.h"

/*
创建 TCP socket 
初始化对应的结构体
设置初始状态为 CLOSED
*/
tju_tcp_t* tju_socket();

/*
绑定监听的地址 包括ip和端口
*/
int tju_bind(tju_tcp_t* sock, tju_sock_addr bind_addr);

/*
被动打开 监听bind的地址和端口
设置socket的状态为LISTEN
*/
int tju_listen(tju_tcp_t* sock);

/*
接受连接 
返回与客户端通信用的socket
这里返回的socket一定是已经完成3次握手建立了连接的socket
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
tju_tcp_t* tju_accept(tju_tcp_t* sock);


/*
连接到服务端
该函数以一个socket为参数
调用函数前, 该socket还未建立连接
函数正常返回后, 该socket一定是已经完成了3次握手, 建立了连接
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
int tju_connect(tju_tcp_t* sock, tju_sock_addr target_addr);


int tju_send (tju_tcp_t* sock, const void *buffer, int len);
int tju_recv (tju_tcp_t* sock, void *buffer, int len);

/*
关闭一个TCP连接
这里涉及到四次挥手
*/
int tju_close (tju_tcp_t* sock);

int tju_handle_packet(tju_tcp_t* sock, char* pkt);


/*
    ----------------------以下为自定义辅助函数声明-----------------------
*/

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

#define min(x,y) ((x<y)?(x):(y))    // 宏定义 min() 函数

uint16_t get_wnd_free_len(uint8_t* mark);   // 获取窗口前移长度

/*
	-------以下为event日志记录相关内容
*/
long getCurrentTime();      // 返回调用时间
char* getFlagstr(uint8_t n);    // 返回数据报的标志位字符串
FILE* getEventlog();        // 返回将要写入的日志文件

FILE *server_event_log;     // 服务端
FILE *client_event_log;     // 客户端

void init_log();        // 初始化
void close_log();       // 关闭

// 宏定义函数-----EVENT事件 - 打印日志
#define _SEND_LOG_(pkt) \
{\
    fprintf(getEventlog(),"[%ld] [SEND] [seq:%d ack:%d flag:%s]\n",\
    getCurrentTime(),get_seq(pkt),get_ack(pkt),getFlagstr(NO_FLAG));\
}

#define _RECV_LOG_(pkt) \
{\
    fprintf(getEventlog(),"[%ld] [RECV] [seq:%d ack:%d flag:%s]\n",\
    getCurrentTime(),get_seq(pkt),get_ack(pkt),getFlagstr(get_flags(pkt)));\
}

#define _CWND_LOG_(sock) \
{\
    fprintf(getEventlog(),"[%ld] [CWND] [type:%d size:%d]\n",\
    getCurrentTime(),sock->window.wnd_send->window_status,sock->window.wnd_send->rwnd/MAX_DLEN);\
}

#define _RWND_LOG_(sock) \
{\
    fprintf(getEventlog(),"[%ld] [RWND] [size:%d]\n",\
    getCurrentTime(),MAX_SOCK_BUF_SIZE-sock->received_len/MAX_DLEN);\
}

#define _SWND_LOG_(sock) \
{\
    fprintf(getEventlog(),"[%ld] [SWND] [size:%d]\n",\
    getCurrentTime(),sock->window.wnd_send->window_size/MAX_DLEN);\
}

#define _RTTS_LOG_(sock,sampleRTT) \
{\
    float sample=sampleRTT*0.001;\
    float estimated=sock->window.wnd_send->estmated_rtt*0.001;\
    float deviation=sock->window.wnd_send->dev_rtt*0.001;\
    float timeoutinterval=sock->window.wnd_send->timeout.it_value.tv_usec*0.001;\
    fprintf(getEventlog(),"[%ld] [RTTS] [SampleRTT:%f EstimatedRTT:%f DeviationRTT:%f TimeoutInterval:%f]\n",\
    getCurrentTime(),sample,estimated,deviation,timeoutinterval);\
}

#define _DELV_LOG_(seq,len) \
{\
    fprintf(getEventlog(),"[%ld] [DELV] [seq:%d size:%d]\n",\
    getCurrentTime(),seq,len);\
}

#endif

