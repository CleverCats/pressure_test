#pragma once
#include "cnet.h"
#include "connection_pool.h"
#include "threads_pool.h"
extern NetWork g_net;
extern bool g_shutdown;
extern int g_epoll_fd;
extern bool g_isset_header;
extern std::string g_message;
extern bool g_isrecv_msg;