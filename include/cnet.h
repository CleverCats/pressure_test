#pragma once
#include <string>
#include "threads_pool.h"

struct ConnNode;
class NetWork{
    public:
        void init_config();
        int setnonblocking(int fd);
        void init_connected_sock(int epoll_fd, int fd);
        bool write_nbytes(ConnNode *fd_node, CThreadPool &g_thread_pool);

        /*从服务器读取数据*/
        bool read_once(int sockfd,char*buffer,int len);
        void start_conn(int epoll_fd,int num,const char*ip,int port);
        void close_conn(int epoll_fd, ConnNode *fd_node);
    public:
        bool isrecv_msg = true;
        bool isset_header = true;
        int max_conn = 2000;
        int thread_num = 10;
        int epoll_fd = 0;
};
