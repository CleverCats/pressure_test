#pragma once
#include <string>
#include <chrono> // 时钟
#include "threads_pool.h"

struct ConnNode;
class NetWork
{
public:
    NetWork();
    void init_config();
    int setnonblocking(int fd);
    void init_connected_sock(int epoll_fd, int fd);
    bool write_nbytes(ConnNode *fd_node, CThreadPool &g_thread_pool);
    /*数据读取*/
    bool read_once(int sockfd, char *buffer, int len);
    void start_conn(int epoll_fd, int num, const char *ip, int port);
    /*负载监视*/
    void show_connect_info();
    /*资源回收*/
    static void handle_trem(int sig);
    void close_conn(int epoll_fd, ConnNode *fd_node);
public:
    /**
     * @brief 网路配置
     * @param isrecv_msg 
     * @param max_conn 
     * @param thread_num 线程池线程数
     * @param epoll_fd
     */
    // 是否接受服务器数据(交互测压/单项测压)
    bool isrecv_msg = true;
    // 是否遵循包头包体协议
    bool isset_header = true;
    // epoll最大连接数(设置必须大于连接数)
    int max_conn = 2000;
    // 线程池线程数
    int thread_num = 10;
    // epoll_creat fd
    int epoll_fd = 0;
    // 业务处理数
    std::atomic<int> msg_count = 0;
    // 上次发包时间
    std::chrono::time_point<std::chrono::high_resolution_clock> last_recv_time;
    // 压测时间(s)
    int pressure_continue_time = 5;
    // 显示频率(ms)
    int show_interval = 2000;
};
