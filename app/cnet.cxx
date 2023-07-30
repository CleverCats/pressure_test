#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <iostream>
#include "cnet.h"
#include "threads_pool.h"
#include "global.h"
#include "connection_pool.h"
#include "conf.h"

static int pool_node = 0;
NetWork::NetWork()
{
    last_recv_time = std::chrono::high_resolution_clock::now();
}
int NetWork::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
};

void NetWork::init_connected_sock(int epoll_fd, int fd)
{
    // printf("add fd:%d\n",  fd);
    Connections *g_conn_pool = Connections::get_instance();
    auto fd_node = g_conn_pool->get_connect_node(fd);

    /**
     * @brief 是否单向测压
     * @param isrecv_msg: true 交互测试 false 单向测试
     */
    epoll_event event;

    if (g_net.isrecv_msg == true)
        event.events = EPOLLOUT | EPOLLET | EPOLLERR;
    else
        event.events = EPOLLOUT | EPOLLERR;

    /**
     * @brief data.fd和data.ptr 为共用体(union)的一部分,
     * 共享同一块内存空间,二者选其一使用即可
     */
    event.data.ptr = (void *)fd_node;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
};

bool NetWork::write_nbytes(ConnNode *fd_node, CThreadPool &g_thread_pool)
{
    // printf("write out%d bytes to socket%d\n", len, sockfd);
    g_thread_pool.ProcessDataAndSignal(fd_node);
    return true;
};

bool NetWork::read_once(int sockfd, char *buffer, int len)
{
    int bytes_read = 0;
    memset(buffer, '\0', len);
    bytes_read = recv(sockfd, buffer, len, 0);
    if (bytes_read == -1)
        return false;
    else if (bytes_read == 0)
        return false;
    printf("read in %d bytes from socket %d with content: %s\n", bytes_read, sockfd, buffer);
    return true;
};

void NetWork::start_conn(int epoll_fd, int num, const char *ip, int port)
{
    int ret = 0;
    int conn_succ = 0, conn_err = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    printf("connect scale: %d ip: %s port: %d\n", num, ip, port);
    for (int i = 0; i < num; ++i)
    {
        int sockfd = socket(PF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
            continue;
        if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == 0)
        {
            ++conn_succ;
            // printf("sockfd %d\n", sockfd);
            init_connected_sock(epoll_fd, sockfd);
        }
        else
            ++conn_err;
    }
    std::cout << "build connection: " << conn_succ;
    std::cout << " error connection: " << conn_err << std::endl;
};

void NetWork::close_conn(int epoll_fd, ConnNode *fd_info)
{
    Connections *instance = Connections::get_instance();
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd_info->sockfd, 0);
    instance->back_connect_node(fd_info);
};

void NetWork::init_config()
{
    CConfig *g_conf_device = CConfig::GetInstance();
    if (g_conf_device->Load("client.conf") == false)
        assert(false);

    max_conn = g_conf_device->GetIntDefault("CLIENT_MAX_CON", 3000);
    isrecv_msg = g_conf_device->GetIntDefault("CLIENT_IS_RECV_MSG", 1) == 1 ? true : false;
    thread_num = g_conf_device->GetIntDefault("CLIENT_THREAD_NUM", 10);
    isset_header = g_conf_device->GetIntDefault("CLIENT_IS_SETHEADER", 10) == 1 ? true : false;
    pressure_continue_time = g_conf_device->GetIntDefault("CLIENT_EXIT_TIME", 5);
}

void NetWork::show_connect_info()
{
    static int t = 0;
    Connections *g_conn_pool = Connections::get_instance();
    auto cur_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - g_net.last_recv_time).count();
    if (duration >= show_interval)
    {
        ++t;
        if (t != 1)
        {
            g_net.last_recv_time = cur_time;
            std::cout << "\ronline: " << g_conn_pool->online_pool.size() << " "
                      << "pool capacity: " << g_conn_pool->conns_pool.capacity() << " "
                      << "msg_count: " << msg_count << " ";
            std::cout << "qps: " << g_net.msg_count / (duration / 1000.0);
            g_net.msg_count = 0;
            // std::cout<<" thread is running: "<<g_thread_pool.m_isRunningThreadNum<<" ";
            std::cout.flush();
            if (pressure_continue_time < show_interval * t / 1000)
                handle_trem(1);
            /*pressure_continue_time end client*/
        }
    }
}

void NetWork::handle_trem(int sig)
{
    Connections *g_conn_pool = Connections::get_instance();
    g_thread_pool.StopAllthreads();
    std::cout <<std::endl;
    std::cout << "online scale: " << g_conn_pool->online_pool.size();
    /**
     * @brief 断开在线
     */
    for (auto online_node : g_conn_pool->online_pool)
    {
        g_net.close_conn(g_net.epoll_fd, online_node.second);
    }
    g_conn_pool->online_pool.clear();
    g_conn_pool->free_all_nodes();
    g_release_over = true;
}