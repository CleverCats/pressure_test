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
bool g_shutdown = false;
bool g_release_over = false;

std::string g_message = "say hellow world";
CThreadPool g_thread_pool;
Connections *g_conn_pool = Connections::get_instance();

int main(int argc, char *argv[])
{
    // signal(SIGTERM, handle_trem);

    g_conn_pool->init_pool(atoi(argv[3]));
    g_net.epoll_fd = epoll_create(g_net.max_conn);
    g_net.init_config();
    g_thread_pool.CreatThreadPool(g_net.thread_num);
    /*scale ip port*/
    assert(argc == 4);
    g_net.start_conn(g_net.epoll_fd, atoi(argv[3]), argv[1], atoi(argv[2]));

    epoll_event events[1000];
    char buffer[2048];
    while (!g_release_over)
    {
        int fds = epoll_wait(g_net.epoll_fd, events, 1000, -1);
        // printf("========== get event: %d ===============\n", fds);
        g_net.show_connect_info();
        for (int i = 0; i < fds; i++)
        {
            if (events[i].events & EPOLLIN)
            {
                auto fd_node = (ConnNode *)events[i].data.ptr;
                if (!g_net.read_once(fd_node->sockfd, buffer, 2048))
                {
                    g_net.close_conn(g_net.epoll_fd, fd_node);
                }
                else
                {
                    struct epoll_event event;
                    event.events = EPOLLOUT | EPOLLET | EPOLLERR;
                    int socket_fd = fd_node->sockfd;
                    event.data.ptr = (void *)fd_node;
                    epoll_ctl(g_net.epoll_fd, EPOLL_CTL_MOD, socket_fd, &event);
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
    std::cout<<"pressure over"<<std::endl;
}