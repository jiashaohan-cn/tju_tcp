/***************************************************
 * Filename: syn.c
 * Author: 贾少涵   Date: 2023.9.22
 * Description: 定义头文件"syn.h"中声明的相关函数
****************************************************/

#include "global.h"
#include "kernel.h"
#include "syn.h"


void init_queue(){      // 初始化
    for (int i=0;i<MAX_SOCK;i++){
        syn_queue[i].sock=accept_queue[i]=NULL;
        syn_queue[i].packet_SYN_ACK=NULL;
    }
    syn_num=accept_num=0;
    // 初始化锁
    pthread_mutex_init(&(syn_queue_lock), NULL);
    pthread_mutex_init(&(accept_queue_lock), NULL);
}

tju_tcp_t* get_from_accept(){   // 从全连接队列中取出socket
    while (accept_num==0) ;
    tju_tcp_t* ret;
    pthread_mutex_lock(&(accept_queue_lock));
    for (int i=0;i<MAX_SOCK;i++){
        if (accept_queue[i]!=NULL){
            ret=accept_queue[i];
            accept_queue[i]=NULL;
            accept_num--;
            pthread_mutex_unlock(&(accept_queue_lock));
            return ret;
        }
    }
    pthread_mutex_unlock(&(accept_queue_lock));
    return NULL;
}

void en_syn_queue(tju_tcp_t* sock, char* pkt){ // 将socket加入半连接队列
    int hashval;
    hashval=cal_hash(sock->established_local_addr.ip,sock->established_local_addr.port,\
                sock->established_remote_addr.ip,sock->established_remote_addr.port);
    pthread_mutex_lock(&(syn_queue_lock));
    if (syn_queue[hashval].sock==NULL){
        syn_queue[hashval].sock=sock;
        syn_queue[hashval].last_ack_time=clock();
        syn_queue[hashval].remands=SYN_DEFAULT_REMANDS;
        syn_queue[hashval].packet_SYN_ACK=pkt;
        syn_num++;
    }
    else{
        printf("该socket已在半连接队列中\n");
    }
    pthread_mutex_unlock(&(syn_queue_lock));
}

void en_accept_queue(tju_tcp_t* sock){  // 将socket加入全连接队列
    int hashval;
    hashval=cal_hash(sock->established_local_addr.ip,sock->established_local_addr.port,\
                sock->established_remote_addr.ip,sock->established_remote_addr.port);
    pthread_mutex_lock(&(accept_queue_lock));
    if (accept_queue[hashval]==NULL){
        accept_queue[hashval]=sock;
        accept_num++;
    }
    else{
        printf("该socket已在全连接队列中\n");
    }
    pthread_mutex_unlock(&(accept_queue_lock));
}

tju_tcp_t* get_from_syn(char* pkt){  // 从半连接队列中取出socket
    pthread_mutex_lock(&(syn_queue_lock));
    if (syn_num==0){    // 半连接队列为空
        pthread_mutex_unlock(&(syn_queue_lock));
        return NULL;
    }
    tju_tcp_t* ret;
    tju_sock_addr local_addr, remote_addr;
    local_addr.ip=inet_network(SERVER_IP);
    local_addr.port=get_dst(pkt);
    remote_addr.ip=inet_network(CLIENT_IP);
    remote_addr.port=get_src(pkt);

    int hashval=cal_hash(local_addr.ip,local_addr.port,remote_addr.ip,remote_addr.port);
    if (syn_queue[hashval].sock!=NULL){
        ret=syn_queue[hashval].sock;
        syn_queue[hashval].sock=NULL;
        free(syn_queue[hashval].packet_SYN_ACK);
        syn_queue[hashval].packet_SYN_ACK=NULL;
        syn_num--;
    }
    else{
        ret=NULL;
    }
    pthread_mutex_unlock(&(syn_queue_lock));
    return ret;
}

// 测试平台用不了
void* syn_retrans_thread(void* arg){       // 半连接队列维护线程
    for (int i=0;i<MAX_SOCK;i++){
        pthread_mutex_lock(&(syn_queue_lock));
        if (syn_queue[i].sock!=NULL){
            // 检查该sock是否过期
            if (syn_queue[i].remands>0){
                if ((clock()-syn_queue[i].last_ack_time)/CLOCKS_PER_SEC>=1){
                    // 超时
                    char *packet_SYN_ACK=(char *)malloc(DEFAULT_HEADER_LEN*sizeof(char));
                    memcpy(packet_SYN_ACK,syn_queue[i].packet_SYN_ACK,DEFAULT_HEADER_LEN);
                    sendToLayer3(packet_SYN_ACK,DEFAULT_HEADER_LEN);
                    free(packet_SYN_ACK);
                    syn_queue[i].last_ack_time=clock(); // 重新计时
                    syn_queue[i].remands--;
                    printf("半连接队列：超时重传\n");
                }
            }
            else{
                // 过期，丢掉该sock
                free(syn_queue[i].sock);
                syn_queue[i].sock=NULL;
                free(syn_queue[i].packet_SYN_ACK);
                syn_queue[i].packet_SYN_ACK=NULL;
                printf("半连接队列：过期，丢弃该sock\n");
            }
        }
        pthread_mutex_unlock(&(syn_queue_lock));
        if (i==MAX_SOCK-1){
            i=0;
            // sleep(1);
        }
    }
}
