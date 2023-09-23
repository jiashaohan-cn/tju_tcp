/***************************************************
 * Filename: log.c
 * Author: 贾少涵   Date: 2023.9.18
 * Description: 定义头文件"log.h"中声明的相关函数
***************************************************/

#include "global.h"
#include "log.h"

long getCurrentTime(){      // 返回调用时间
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

/*------------不再使用-------------*/
// char* getFlagstr(uint8_t n){    // 返回数据报的标志位字符串
//     if (n==NO_FLAG) return "NO_FLAG";
//     else if (n==SYN_FLAG_MASK) return "SYN";
//     else if (n==FIN_FLAG_MASK) return "FIN";
//     else if (n==ACK_FLAG_MASK) return "ACK";
//     else if (n==(FIN_FLAG_MASK|ACK_FLAG_MASK)) return "FIN|ACK";
//     else if (n==(SYN_FLAG_MASK|ACK_FLAG_MASK)) return "SYN|ACK";
//     else{
//         printf("event日志: getFlagstr标志位获取错误\n");
//         exit(-1);
//     }
// }

FILE* getEventlog(){    // 返回将要写入的日志文件
    char hostname[8];
    gethostname(hostname, 8);
    if (strcmp(hostname,"server")==0) return server_event_log;
    else if (strcmp(hostname,"client")==0) return client_event_log;
    else{
        printf("event日志函数: getEventlog获取hostneme失败\n");
        exit(-1);
    }
}

// 模块初始化
void init_log(){
    server_event_log=fopen("./server.event.trace","w");
    client_event_log=fopen("./client.event.trace","w");
}

// 关闭模块
void close_log(){
    fclose(server_event_log);
    fclose(client_event_log);
}
