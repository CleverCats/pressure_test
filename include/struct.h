#pragma once

#pragma pack(1)

/* 包头结构 */
typedef struct
{
    unsigned short packageLength; // 报文长度【包头+包体】
    unsigned short messageCode;   // 消息类型代码【区别命令类型】
    int crc32;                    // 校验是否为过期包
} COMM_PKG_HEADER, *LP_COMM_PKG_HEADER;

/*登录*/
typedef struct
{
    char Registercid[50];   // 登录账户
    char Registerccode[50]; // 登录密码
} Register, *LP_Register;

#pragma pack()