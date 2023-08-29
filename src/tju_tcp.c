#include "tju_tcp.h"

/*
创建 TCP socket 
初始化对应的结构体
设置初始状态为 CLOSED
*/
tju_tcp_t* tju_socket(){
    tju_tcp_t* sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    sock->state = CLOSED;
    
    pthread_mutex_init(&(sock->send_lock), NULL);
    sock->sending_buf = NULL;
    sock->sending_len = 0;

    pthread_mutex_init(&(sock->recv_lock), NULL);
    sock->received_buf = NULL;
    sock->received_len = 0;
    
    if(pthread_cond_init(&sock->wait_cond, NULL) != 0){
        perror("ERROR condition variable not set\n");
        exit(-1);
    }

    sock->window.wnd_send = NULL;
    sock->window.wnd_recv = NULL;

    return sock;
}

/*
绑定监听的地址 包括ip和端口
*/
int tju_bind(tju_tcp_t* sock, tju_sock_addr bind_addr){
    sock->bind_addr = bind_addr;
    return 0;
}

/*
被动打开 监听bind的地址和端口
设置socket的状态为LISTEN
注册该socket到内核的监听socket哈希表
*/
int tju_listen(tju_tcp_t* sock){
    // 初始化半连接和全连接队列
    init_queue();

    sock->state = LISTEN;
    int hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
    listen_socks[hashval] = sock;
    return 0;
}

/*
接受连接 
返回与客户端通信用的socket
这里返回的socket一定是已经完成3次握手建立了连接的socket
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
tju_tcp_t* tju_accept(tju_tcp_t* listen_sock){
    
    // 判断全连接队列中是否有 socket
    printf("开始监听全连接队列\n");
    tju_tcp_t* accept_socket=get_from_accept();     // 队列为空 阻塞
    printf("从全连接队列中取出一个sock\n");

    tju_tcp_t* new_conn = accept_socket;

    // 将新的conn放到内核建立连接的socket哈希表中
    int hashval = cal_hash(new_conn->established_local_addr.ip, new_conn->established_local_addr.port, \
                new_conn->established_remote_addr.ip, new_conn->established_remote_addr.port);
    established_socks[hashval] = new_conn;

    // 如果new_conn的创建过程放到了tju_handle_packet中 那么accept怎么拿到这个new_conn呢
    // 在linux中 每个listen socket都维护一个已经完成连接的socket队列
    // 每次调用accept 实际上就是取出这个队列中的一个元素
    // 队列为空,则阻塞 
    printf("服务器三次握手完成\n");
    return new_conn;
}


/*
连接到服务端
该函数以一个socket为参数
调用函数前, 该socket还未建立连接
函数正常返回后, 该socket一定是已经完成了3次握手, 建立了连接
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
int tju_connect(tju_tcp_t* sock, tju_sock_addr target_addr){

    // 将socket绑定本地地址
    clock_t time_point;     // 计时用
    tju_sock_addr local_addr;
    local_addr.ip = inet_network(CLIENT_IP);
    local_addr.port = 5678; // 连接方进行connect连接的时候 内核中是随机分配一个可用的端口
    sock->established_local_addr = local_addr;

    // 向客户端发送 SYN 报文，并将状态改为 SYN_SENT
    uint32_t seq=CLIENT_ISN;
    char* packet_SYN=create_packet_buf(local_addr.port,target_addr.port,seq,0,\
            DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,SYN_FLAG_MASK,1,0,NULL,0);
    sendToLayer3(packet_SYN,DEFAULT_HEADER_LEN);
    time_point=clock();  // 开始计时
    sock->state=SYN_SENT;
    printf("客户端发送SYN---第一次握手\n");

    // 将即将建立连接的socket放入内核 已建立连接哈希表中
    int hashval = cal_hash(local_addr.ip, local_addr.port, target_addr.ip, target_addr.port);
    established_socks[hashval] = sock;

    // 阻塞等待（简单计时器）
    while (sock->state!=ESTABLISHED){
        if ((clock()-time_point)/CLOCKS_PER_SEC>=1){    //触发计时器--超时重传
            sendToLayer3(packet_SYN,DEFAULT_HEADER_LEN);
            time_point=clock(); //重新开始计时
        }
    }
    
    // 三次握手完成
    sock->established_remote_addr = target_addr;    // 绑定远端地址
    
    printf("客户端三次握手完成\n");
    return 0;
}

int tju_send(tju_tcp_t* sock, const void *buffer, int len){
    // 这里当然不能直接简单地调用sendToLayer3
    char* data = malloc(len);
    memcpy(data, buffer, len);

    char* msg;
    uint32_t seq = 464;
    uint16_t plen = DEFAULT_HEADER_LEN + len;

    msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
              DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, data, len);

    sendToLayer3(msg, plen);
    
    return 0;
}
int tju_recv(tju_tcp_t* sock, void *buffer, int len){
    while(sock->received_len<=0){
        // 阻塞
    }

    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    int read_len = 0;
    if (sock->received_len >= len){ // 从中读取len长度的数据
        read_len = len;
    }else{
        read_len = sock->received_len; // 读取sock->received_len长度的数据(全读出来)
    }

    memcpy(buffer, sock->received_buf, read_len);

    if(read_len < sock->received_len) { // 还剩下一些
        char* new_buf = malloc(sock->received_len - read_len);
        memcpy(new_buf, sock->received_buf + read_len, sock->received_len - read_len);
        free(sock->received_buf);
        sock->received_len -= read_len;
        sock->received_buf = new_buf;
    }else{
        free(sock->received_buf);
        sock->received_buf = NULL;
        sock->received_len = 0;
    }
    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

    return 0;
}

int tju_handle_packet(tju_tcp_t* sock, char* pkt){
    
    // 判断收到报文的 socket 的状态是否为 SYN_SENT
    if (sock->state==SYN_SENT){
        if (get_ack(pkt)==CLIENT_ISN+1){
            char* packet_SYN_ACK2=create_packet_buf(get_dst(pkt),get_src(pkt),get_ack(pkt),get_seq(pkt)+1,\
                        DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,ACK_FLAG_MASK,1,0,NULL,0);
            sendToLayer3(packet_SYN_ACK2,DEFAULT_HEADER_LEN);
            sock->state=ESTABLISHED;
            printf("客户端发送SYN_ACK----第三次握手\n");
        }
    }
    else if (sock->state==LISTEN){
        if (get_flags(pkt)==SYN_FLAG_MASK){
            // 将 socket 存入半连接队列中
            tju_tcp_t* new_conn = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
            memcpy(new_conn, sock, sizeof(tju_tcp_t));
            new_conn->state=SYN_RECV;
            en_syn_queue(new_conn);
            printf("sock进半连接队列\n");

            // 向客户端发送 SYN_ACK 报文
            char* packet_SYN_ACK1=create_packet_buf(get_dst(pkt),get_src(pkt),SERVER_ISN,get_seq(pkt)+1,\
                        DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,SYN_FLAG_MASK|ACK_FLAG_MASK,1,0,NULL,0);
            sendToLayer3(packet_SYN_ACK1,DEFAULT_HEADER_LEN);
            printf("服务器发送SYN_ACK----第二次握手\n");
        }
        else if (get_flags(pkt)==ACK_FLAG_MASK&&get_ack(pkt)==SERVER_ISN+1){
            // 取出半连接中的socket加入全连接队列中
            tju_tcp_t* tmp_conn=get_from_syn();
            printf("从半连接队列中取出sock\n");

            tmp_conn->established_local_addr=tmp_conn->bind_addr;
            tmp_conn->established_remote_addr.ip=inet_network(CLIENT_IP);
            tmp_conn->established_remote_addr.port=get_src(pkt);
            tmp_conn->state=ESTABLISHED;

            en_accept_queue(tmp_conn);
            printf("sock加入全连接队列\n");
        }
    }
    else if (sock->state==ESTABLISHED){
        if (get_flags(pkt)==(FIN_FLAG_MASK|ACK_FLAG_MASK)){
            // 发送FIN_ACK报文
            char* packet_FIN_ACK=create_packet_buf(get_dst(pkt),get_src(pkt),get_ack(pkt),get_seq(pkt)+1,\
                    DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,ACK_FLAG_MASK,1,0,NULL,0);
            sendToLayer3(packet_FIN_ACK,DEFAULT_HEADER_LEN);
            printf("发送FIN_ACK报文\n");

            // 更新状态
            sock->state=CLOSE_WAIT;
            sleep(1);          // 等待，防止混入 同时关闭 的情况

            // 调用close
            printf("调用tju_close函数\n");
            tju_close(sock);
        }
    }
    else if (sock->state==FIN_WAIT_1){
        if (get_flags(pkt)==ACK_FLAG_MASK&&get_ack(pkt)==FIN_SEQ+1){    // 双方先后关闭
            sock->state=FIN_WAIT_2;
        }
        else if (get_flags(pkt)==(FIN_FLAG_MASK|ACK_FLAG_MASK)){    // 同时关闭
            // 发送FIN_ACK报文
            char* packet_FIN_ACK3=create_packet_buf(get_dst(pkt),get_src(pkt),get_ack(pkt),get_seq(pkt)+1,\
                    DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,ACK_FLAG_MASK,1,0,NULL,0);
            sendToLayer3(packet_FIN_ACK3,DEFAULT_HEADER_LEN);

            // 状态更新
            sock->state=CLOSING;
        }
    }
    else if (sock->state==FIN_WAIT_2){
        if (get_flags(pkt)==(FIN_FLAG_MASK|ACK_FLAG_MASK)){
            // 发送FIN_ACK报文
            char* packet_FIN_ACK2=create_packet_buf(get_dst(pkt),get_src(pkt),get_ack(pkt),get_seq(pkt)+1,\
                    DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,ACK_FLAG_MASK,1,0,NULL,0);
            sendToLayer3(packet_FIN_ACK2,DEFAULT_HEADER_LEN);

            // 更新socket状态并等待2MSL
            sock->state=TIME_WAIT;
            sleep(10);
            sock->state=CLOSED;
        }
    }
    else if (sock->state==LAST_ACK){
        if (get_flags(pkt)==ACK_FLAG_MASK&&get_ack(pkt)==FIN_SEQ+1){
            sock->state=CLOSED;
        }
    }
    else if (sock->state==CLOSING){
        if (get_flags(pkt)==ACK_FLAG_MASK&&get_ack(pkt)==FIN_SEQ+1){
            sock->state=TIME_WAIT;
            sleep(10);          // 等待2MSL
            sock->state=CLOSED;
        }
    }
    
    uint32_t data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;
    if (data_len==0) return 0;

    // 把收到的数据放到接受缓冲区
    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    if(sock->received_buf == NULL){
        sock->received_buf = malloc(data_len);
    }else {
        sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);
    }
    memcpy(sock->received_buf + sock->received_len, pkt + DEFAULT_HEADER_LEN, data_len);
    sock->received_len += data_len;

    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁


    return 0;
}

int tju_close (tju_tcp_t* sock){
    // 发送FIN报文
    char* packet_FIN=create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,FIN_SEQ,\
                    0,DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,FIN_FLAG_MASK|ACK_FLAG_MASK,1,0,NULL,0);
    sendToLayer3(packet_FIN,DEFAULT_HEADER_LEN);
    printf("tju_close函数发送FIN报文\n");

    // 状态更新
    if (sock->state==ESTABLISHED){  // 主动调用
        sock->state=FIN_WAIT_1;
    }
    else if (sock->state==CLOSE_WAIT){  // 被动调用
        sock->state=LAST_ACK;
    }

    // 阻塞等待
    printf("等待中......\n");
    while (sock->state!=CLOSED) ;

    printf("连接关闭\n");
    // 释放资源
    free(sock);
    return 0;
}