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
#include "cnet.h"
#include <string>
#include "threads_pool.h"
#include "global.h"
#include "connection_pool.h"

static int pool_node = 0;
int NetWork::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
};

void NetWork::add_epoll(int epoll_fd, int fd)
{
    //printf("add fd:%d\n",  fd);
    Connections *g_conn_pool = Connections::get_instance();
    auto fd_node = g_conn_pool->get_connect_node(fd);

    /**
     * @brief data.fd和data.ptr 为共用体(union)的一部分,
     * 共享同一块内存空间,二者选其一使用即可
     */
    epoll_event event;
    event.events = EPOLLOUT | EPOLLET | EPOLLERR;
    event.data.ptr = (void *)fd_node;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
};

bool NetWork::write_nbytes(ConnNode *fd_node, CThreadPool &g_thread_pool)
{
    int bytes_write = 0;
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
    inet_pton(AF_INET, ip,&address.sin_addr);
    address.sin_port = htons(port);
    printf("scale: %d ip: %s port: %d\n", num, ip, port);
    for (int i = 0; i<num; ++i)
    {
        int sockfd = socket(PF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
            continue;
        if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == 0)
        {
            ++conn_succ;
            //printf("sockfd %d\n", sockfd);
            add_epoll(epoll_fd, sockfd);
        }
        else
            ++conn_err;
    }
    printf("build connection: %d\n", conn_succ);
    printf("connection error: %d\n", conn_err);
};

void NetWork::close_conn(int epoll_fd, ConnNode *fd_info)
{
    Connections *instance = Connections::get_instance();
    epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd_info->sockfd,0);
    instance->back_connect_node(fd_info);
};