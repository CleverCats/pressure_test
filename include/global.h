#pragma once
#include "cnet.h"
#include "connection_pool.h"
#include "threads_pool.h"

// 结构定义,用来储存有效配置信息
typedef struct _CConfItem
{
	char ItemName[50];	   // 配置信息名
	char ItemContent[500]; // 配置信息数据
} CConfItem, *LPCConfItem;

extern NetWork g_net;
extern bool g_shutdown;
extern std::string g_message;