#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h> //htons
#include <zlib.h>
#include <string.h> //memcpy
#include "threads_pool.h"
#include "auto_mutex.h"
#include "global.h"
#include "struct.h"
#include "crc_32.h"

pthread_mutex_t CThreadPool::m_pthreadMutex = PTHREAD_MUTEX_INITIALIZER; // 初始化线程同步锁
pthread_cond_t CThreadPool::m_pthreadCond = PTHREAD_COND_INITIALIZER;    // 初始化条件变量
CThreadPool::CThreadPool()
{
    m_curMsgNum = 0; // 当前消息队列项数
    m_isRunningThreadNum = 0;
    m_lastThreadEmTime = time(NULL);
}

CThreadPool::~CThreadPool() {}

int CThreadPool::SendMessage(const std::string &body, ConnNode *message_info)
{
    int sockfd = message_info->sockfd;
    int len = 0, have_send = 0;
    std::shared_ptr<char[]> message;
    COMM_PKG_HEADER pack_header;
    if (g_net.isset_header == true)
    {
        /*获取报文缓存区*/
        message = message_info->malloc_memory(body.size() + sizeof(COMM_PKG_HEADER) + 1);
        /*填充包头*/
        pack_header.crc32 = htonl(1024);
        pack_header.messageCode = htons(7);
        pack_header.packageLength = htons(body.size() + sizeof(COMM_PKG_HEADER) + 1);
        len = ntohs(pack_header.packageLength);

        /**********   version 1  ***********
        cout<<"pkg_len sizeof(COMM_PKG_HEADER) + message.size(): "<<sizeof(COMM_PKG_HEADER) + message.size()<<endl;
        cout<<"sizeof(COMM_PKG_HEADER): " <<sizeof(COMM_PKG_HEADER)<<" message.size(): "<<message.size()<<endl;

        char *c_header = (char *)&pack_header;
        // 组合包头和包体
        std::string send_buf(c_header, sizeof(COMM_PKG_HEADER));
        send_buf += message;
        const char *c_send_buf = send_buf.c_str();

        // 解析
        LP_COMM_PKG_HEADER header_get = LP_COMM_PKG_HEADER(c_send_buf);
        cout<<"pack_header.crc32: "<<ntohl(header_get->crc32)<<endl;;
        cout<<"pack_header.messageCode: "<<ntohs(header_get->messageCode)<<endl;;
        char *body = (char *)c_send_buf + sizeof(COMM_PKG_HEADER);
        cout<<"body: "<<body<<endl;
        ********************/

        /********** version 2 ***********/
        // vector<char> send_buf(sizeof(COMM_PKG_HEADER) + message.size());
        // memcpy(send_buf.data(), &pack_header, sizeof(COMM_PKG_HEADER));
        // memcpy(send_buf.data() + sizeof(COMM_PKG_HEADER), message.c_str(), message.size());
        // char *c_send_buf = send_buf.data();

        /********** version 3 ***********/
        memcpy(message.get(), &pack_header, sizeof(COMM_PKG_HEADER));
        memcpy(message.get() + sizeof(COMM_PKG_HEADER), body.c_str(), body.size() + 1);

        /*在接收端，解析报文
        LP_COMM_PKG_HEADER header_get = (LP_COMM_PKG_HEADER)send_buf.data();
        cout<<"pack_header.crc32: "<<ntohl(header_get->crc32)<<endl;;
        cout<<"pack_header.messageCode: "<<ntohs(header_get->messageCode)<<endl;;*/
    }
    else
    {
        message = message_info->malloc_memory(body.size());
        memcpy(message.get(), body.c_str(), body.size() + 1);
        len = body.size();
    }

    char *c_send_buf = message.get();
    while (1)
    {
        int bytes_write = send(sockfd, c_send_buf, len, 0);
        have_send += bytes_write;
        if (bytes_write == -1)
            return -1;
        else if (bytes_write == 0)
            return -1;
        len -= bytes_write;
        c_send_buf = c_send_buf + bytes_write;
        if (len <= 0)
            return have_send;
    }
}
void *CThreadPool::ThreadFunc(void *ThreadData)
{
    ThreadItem *lpthread = (ThreadItem *)ThreadData;
    CThreadPool *lpThreadPool = lpthread->_pThis;

    int err;

    pthread_t _tid = pthread_self(); // 获取线程id
    std::cout<<"\r("<< ++lpThreadPool->cur_index <<"/"<<lpThreadPool->m_ThreadNum
             <<") "<<"running thread id:"<< _tid<<"  ";
    std::cout.flush();
    while (true)
    {
        err = pthread_mutex_lock(&m_pthreadMutex); // 线程同步互斥
        if (err != 0)
            printf("ThreadFunc(void *ThreadData),pthread_mutex_lock调用失败,错误码:%d\n", err); // 不应该出现,走到这里的都锁住了才对

        while ((lpThreadPool->m_msgSendQueue.size() == 0) && g_shutdown == false)
        {
            if (lpthread->ifrunning == false)
                lpthread->ifrunning = true;                     // 线程标记运行中
            pthread_cond_wait(&m_pthreadCond, &m_pthreadMutex); // 相当于C++11中条件对象的成员函数wait
        }

        // 线程池通知退出
        if (g_shutdown == true)
        {
            pthread_mutex_unlock(&m_pthreadMutex);
            break;
        }

        /*互斥获取发送队列数据*/

        auto send_node_info = lpThreadPool->m_msgSendQueue.front();
        lpThreadPool->m_msgSendQueue.pop_front();
        --lpThreadPool->m_curMsgNum; // 消息数 -1
        // printf("get ok fd: %d\n", send_node_info->sockfd);
        err = pthread_mutex_unlock(&m_pthreadMutex); // 解锁互斥量
        if (err != 0)
            printf("ThreadFunc(void *ThreadData),pthread_mutex_unlock调用失败,错误码:%d !\n", err);

        /*[处理用户需求]*/

        const std::string body = g_message;

        // 当前正处理业务线程数+1
        ++lpThreadPool->m_isRunningThreadNum;
        // std::cout<<" thread is running: "<<lpThreadPool->m_isRunningThreadNum<<" "<<std::endl;
        // 开始处理用户需求...
        int have_send = SendMessage(body, send_node_info);
        if (have_send == -1)
        {
            printf("send error return :%d\n", have_send);
            if (send_node_info != nullptr)
                g_net.close_conn(g_net.epoll_fd, send_node_info);
        }
        else
        {
            // printf("报文数: %d socket: %d successful send massage len : %d bytes thread id: %lu \n",
            //     ++msg_count, send_node_info->sockfd, have_send, pthread_self());
            
            ++g_net.msg_count;
            if (g_net.msg_count > 100000)
            {
                /**
                 * @brief 睡眠观察交互情况
                 */
                sleep(5000);
            }

            /**
             * @brief
             * 重新注册的原因是因为使用了边缘触发模式（EPOLLET），
             * LT模式下，只要套接字可写，EPOLLOUT 事件就会一直触发1。所以，如果你要实现这样的功能，你不需要重新注册 EPOLLOUT 事件，
             * 只需要在 epoll_ctl 中使用 EPOLL_CTL_ADD 操作，并指定 EPOLLOUT 事件即可。但是，这样可能会导致你的程序频繁地处理
             * EPOLLOUT 事件，而不是真正需要写数据的时候才处理，
             * 这样可能会降低效率。所以，一般建议使用 ET 模式，并按需注册 EPOLLOUT 事件
             */

            if (g_net.isrecv_msg == true)
            {
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLET | EPOLLERR;
                int socket_fd = send_node_info->sockfd;
                event.data.ptr = (void *)send_node_info;
                epoll_ctl(g_net.epoll_fd, EPOLL_CTL_MOD, socket_fd, &event);
            }
        }

        /**
         * @brief 数据处理完毕, 可以进行后续处理(如释放分配内存)
         */
        --lpThreadPool->m_isRunningThreadNum;
    }
    return nullptr;
}

bool CThreadPool::CreatThreadPool(int threadNum)
{
    ThreadItem *_lpthread; // 线程结构指针
    int err;
    m_ThreadNum = threadNum; // 记录创建线程数

    for (int i = 0; i < m_ThreadNum; ++i)
    {
        m_logicthreadsQueue.push_back(_lpthread = new ThreadItem(this));
        err = pthread_create(&_lpthread->_Handle, NULL, ThreadFunc, (void *)_lpthread);

        if (err != 0)
        {
            // 创建线程失败
            printf("CreatThreadPool(int threadNum),创建线程%d 调用失败,错误码:%d\n", i, err);
            // 直接退出此进程
            return false;
        }
        else
        {
            // printf("线程创建成功:%d",i);
        }

    } // end for

    std::vector<ThreadItem *>::iterator iter;
lblfor:
    for (iter = m_logicthreadsQueue.begin(); iter != m_logicthreadsQueue.end(); ++iter)
    {
        if ((*iter)->ifrunning == false)
        {
            // 等待没有完全启动的线程 3s
            sleep(3);
            goto lblfor;
        }
    }
    std::cout<<std::endl;
    std::cout << "sacle of logic threads be created : " << m_logicthreadsQueue.size() << std::endl;
    return true;
}

void CThreadPool::ProcessDataAndSignal(ConnNode *fd_node)
{
    int err;
    err = pthread_mutex_lock(&m_pthreadMutex);
    if (err != 0 && g_shutdown != true)
    {
        printf("ProcessDataAndSignal(char* _cMsg)中,pthread_mutex_lock,错误码%d !!!\n", err);
    }
    m_msgSendQueue.push_back(fd_node); // 入消息队列
    // printf("in ok fd: %d\n", m_msgSendQueue.back()->sockfd);
    ++m_curMsgNum; // 消息数 +1
    err = pthread_mutex_unlock(&m_pthreadMutex);
    if (err != 0 && g_shutdown != true)
    {
        printf("ProcessDataAndSignal(char* _cMsg)中,pthread_mutex_unlock,错误码%d !!!\n", err);
    }
    CallFreeThread(); // 唤醒一个线程处理
    return;
}

void CThreadPool::CallFreeThread()
{
    int err = pthread_cond_signal(&m_pthreadCond);
    if (err != 0)
    {
        printf("CallFreeThread()中,pthread_cond_signal调用失败,错误码%d !!!\n", err);
        return;
    }

    // if (m_ThreadNum == m_isRunningThreadNum)
    // {
    //     // 如果线程全部繁忙,若至少10s间出现两次,则警告
    //     time_t _CurTime = time(NULL);
    //     if (_CurTime - m_lastThreadEmTime > 10)
    //     {
    //         m_lastThreadEmTime = _CurTime;
    //         printf("繁忙线程数:%d 线程全部占用中,无法及时处理用户需求...\n", (int)m_isRunningThreadNum);
    //     }
    // }
    return;
}

void CThreadPool::StopAllthreads()
{
    // 已经调用过了
    if (g_shutdown == true)
    {
        return;
    }
    g_shutdown = true;

    // 广播唤醒所有m_pthreadCond导致沉睡的线程[==notify_all]
    int err = pthread_cond_broadcast(&m_pthreadCond);

    if (err != 0) // 无法唤醒,打印紧急日志
    {
        printf("StopAllthreads(),pthread_cond_broadcast(&m_pthreadCond) 广播失败,错误码:%d\n", err); // 不应该出现,走到这里的都锁住了才对
        return;
    }

    std::vector<ThreadItem *>::iterator iter;
    for (iter = m_logicthreadsQueue.begin(); iter != m_logicthreadsQueue.end(); ++iter)
    {
        // 等待一个线程调用结束[这里for等待线程池中所有线程终止]
        pthread_join((*iter)->_Handle, NULL); // 相当于join函数
    }

    pthread_mutex_destroy(&m_pthreadMutex);
    pthread_cond_destroy(&m_pthreadCond);

    // 释放new出来的线程结构内存
    for (iter = m_logicthreadsQueue.begin(); iter != m_logicthreadsQueue.end(); ++iter)
    {
        if (*iter)
            delete *iter;
    }
    m_logicthreadsQueue.clear();

    // g_socket.ngx_clear_connectionPool();        //回收连接池内存
    std::cout<<std::endl;
    std::cout<<"stop all threads over"<<std::endl;
    return;
}