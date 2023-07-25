#pragma once
#include <unistd.h>
#include <memory>
#include <vector>
#include <map>
#include <auto_mutex.h>
#include <stdio.h>
#include <stdlib.h>
#include "global.h"
/**
 * @brief  连接池节点
 */
struct ConnNode
{
    ConnNode(int fd = -1) : sockfd(fd){};
    ~ConnNode()
    {
        // printf("析构 sock:%d\n", sockfd);
        if (sockfd != -1)
            close(sockfd);
        sockfd = -1;
    }
    void init_node()
    {
        if (sockfd != -1)
        {
            printf("close socket %d\n", sockfd);
            close(sockfd);
            sockfd = -1;
        }
        msg.reset();
    }
    std::shared_ptr<char[]> malloc_memory(size_t malloc_size)
    {
        std::shared_ptr<char[]> msg(new char[malloc_size]);
        return msg;
    }
    int sockfd;
    std::shared_ptr<char[]> msg;
};

/**
 * @brief 连接池
 */

class Connections
{
    // private:
    Connections(){};

public:
    ~Connections(){};
    static Connections *get_instance()
    {
        // 锁
        if (pool_instance == NULL)
        {
            pool_instance = new Connections(); // 第一次调用不应该放在线程中，应该放在主进程中，以免和其他线程调用冲突从而导致同时执行两次new CMemory()
            static InstanceDestruct cl;
        }
        // 放锁
        return pool_instance;
    }

    class InstanceDestruct
    {
    public:
        ~InstanceDestruct()
        {
            if (Connections::pool_instance)
            {
                delete Connections::pool_instance; // 这个释放是整个系统退出的时候，系统来调用释放内存的哦
                Connections::pool_instance = nullptr;
            }
        }
    };

    void init_pool(size_t pool_size)
    {
        for (size_t i = 0; i < pool_size; i++)
        {
            conns_pool.push_back(new ConnNode());
            // printf("g_conn_pool.size %d g_conn_pool back fd:%d \n", g_conn_pool.size(), g_conn_pool.back()->sockfd);
        }
    }

    ConnNode *get_connect_node(int fd)
    {
        CLOCK NodeGetLock(&pool_mutex);
        // printf("fd:%d\n", fd);
        if (conns_pool.size() == 0)
            return nullptr;

        if (conns_pool.size() < conns_pool.capacity() / 2)
            extend_pool(conns_pool.size() / 3 + 1);

        auto res = conns_pool.back();
        res->sockfd = fd;

        conns_pool.pop_back();
        online_pool.insert(std::pair<int, ConnNode *>(res->sockfd, res));
        return res;
    }

    void back_connect_node(ConnNode *node)
    {
        CLOCK NodeGetLock(&back_mutex);
        printf("back node socket :%d\n", node->sockfd);
        if (node != nullptr)
        {
            node->init_node();
            conns_pool.push_back(node);
            // printf("success back node\n");
        }
    }

    void free_all_nodes()
    {
        printf("conns_pool.size():%lu\n", conns_pool.size());
        /**
         * @brief 回收连接池
         */
        for (auto node = conns_pool.begin(); node != conns_pool.end(); ++node)
        {
            if (*node != nullptr)
            {
                delete *node;
            }
        }
        printf("free_all_nodes over\n");
    }
    size_t get_pool_size()
    {
        return conns_pool.size();
    }

private:
    /**
     * @brief 扩容
     *
     * @param extend_size 容量/2 + 1
     */
    void extend_pool(size_t extend_size)
    {
        printf("extend\n");
        for (size_t i = 0; i < extend_size; i++)
        {
            conns_pool.push_back(new ConnNode());
        }
    }

private:
    pthread_mutex_t pool_mutex; // 刷新锁
    pthread_mutex_t back_mutex; // 回归锁
    std::vector<ConnNode *> conns_pool;
    static Connections *pool_instance;

public:
    std::multimap<int, ConnNode *> online_pool;
};