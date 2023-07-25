#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <signal.h>
#include "threads_pool.h"
#include "connection_pool.h"
#include "cnet.h"
NetWork g_net;
int g_epoll_fd;
int thread_num = 10;
int max_conn = 3000;
bool g_shutdown = false;
bool g_release_over = false;
bool g_isset_header = true;
bool g_isrecv_msg = true;

std::string g_message = "say hellow world";
CThreadPool g_thread_pool;
Connections *g_conn_pool = Connections::get_instance();
static void handle_trem(int sig)
{
    g_thread_pool.StopAllthreads();
    printf("online_pool.size():%lu\n", g_conn_pool->online_pool.size());
    /**
     * @brief 断开在线
     */
    for (auto online_node : g_conn_pool->online_pool)
    {
        //printf("online_node.first: %d\n", online_node.first);
        g_net.close_conn(g_epoll_fd, online_node.second);

    }
    g_conn_pool->online_pool.clear();
        
    g_conn_pool->free_all_nodes();
    g_release_over = true;
}
int main(int argc, char *argv[])
{
    // std::cout << "is ok" << std::endl;
    signal(SIGTERM, handle_trem);

    g_conn_pool->init_pool(20);
    sleep(5);
    g_epoll_fd = epoll_create(max_conn);

    printf("creat epoll_fd: %d\n", g_epoll_fd);
    g_thread_pool.CreatThreadPool(thread_num);
    sleep(3);
    /*scale ip port*/
    assert(argc == 4);

    g_net.start_conn(g_epoll_fd, atoi(argv[3]), argv[1], atoi(argv[2]));
    // Connections *g_conn_pool = Connections::get_instance();
    // g_conn_pool->free_all_nodes();
    epoll_event events[1000];
    char buffer[2048];
    while (!g_release_over)
    {
        int fds = epoll_wait(g_epoll_fd, events, 10000, -1);
        printf("========== get event: %d ===============\n", fds);
        for (int i = 0; i < fds; i++)
        {
            // printf("ok fd: %d\n", fd_node->sockfd);
            if (events[i].events & EPOLLIN)
            {

                auto fd_node = (ConnNode *)events[i].data.ptr;
                if (!g_net.read_once(fd_node->sockfd, buffer, 2048))
                {
                    g_net.close_conn(g_epoll_fd, fd_node);
                }
                else
                {
                    struct epoll_event event;
                    event.events = EPOLLOUT | EPOLLET | EPOLLERR;
                    int socket_fd = fd_node->sockfd;
                    event.data.ptr = (void *)fd_node;
                    epoll_ctl(g_epoll_fd, EPOLL_CTL_MOD, socket_fd, &event);
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                auto fd_node = (ConnNode *)events[i].data.ptr;
                g_net.write_nbytes(fd_node, g_thread_pool);
            }
            else if (events[i].events & EPOLLERR)
                printf("error happend");
        }
    }
}