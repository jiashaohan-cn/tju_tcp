/***********************************************************
 * Filename: log.h 
 * Author: 贾少涵   Date: 2023.9.18 
 * Description:
 *      · 声明日志记录相关的全局变量、初始化函数、辅助函数
 *      · 定义一些宏定义函数，用于event事件的日志记录
 * Modification: 
 *      2023.9.22--------添加头文件注释
 ***********************************************************/

#ifndef LOG_H
#define LOG_H


long getCurrentTime();      // 返回调用时间
// char* getFlagstr(uint8_t n);    // 返回数据报的标志位字符串-------(不再使用)
FILE* getEventlog();        // 返回将要写入的日志文件

FILE *server_event_log;     // 服务端
FILE *client_event_log;     // 客户端

void init_log();        // 初始化
void close_log();       // 关闭


// 宏定义函数-----EVENT事件 - 打印日志
#define _SEND_LOG_(pkt) \
{\
    fprintf(getEventlog(),"[%ld] [SEND] [seq:%d ack:%d flag:%d length:%d]\n",\
    getCurrentTime(),get_seq(pkt),get_ack(pkt),get_flags(pkt),get_plen(pkt)-get_hlen(pkt));\
}

#define _RECV_LOG_(pkt) \
{\
    fprintf(getEventlog(),"[%ld] [RECV] [seq:%d ack:%d flag:%d length:%d]\n",\
    getCurrentTime(),get_seq(pkt),get_ack(pkt),get_flags(pkt),get_plen(pkt)-get_hlen(pkt));\
}

#define _CWND_LOG_(sock,type) \
{\
    fprintf(getEventlog(),"[%ld] [CWND] [type:%d size:%d]\n",\
    getCurrentTime(),type,sock->window.wnd_send->cwnd);\
}

#define _RWND_LOG_(sock) \
{\
    fprintf(getEventlog(),"[%ld] [RWND] [size:%d]\n",\
    getCurrentTime(),sock->window.wnd_send->rwnd);\
}

#define _SWND_LOG_(sock) \
{\
    fprintf(getEventlog(),"[%ld] [SWND] [size:%d]\n",\
    getCurrentTime(),sock->window.wnd_send->window_size);\
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
