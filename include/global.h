#pragma once
#include "cnet.h"
#include "connection_pool.h"
#include "threads_pool.h"
extern NetWork g_net;
// extern CThreadPool g_thread_pool;
extern bool g_shutdown;
extern int g_epoll_fd;