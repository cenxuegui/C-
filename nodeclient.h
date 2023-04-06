#ifndef _NODECLIENT_H_     //预编译指令，作用是：防止递归包含以下内容
#define _NODECLIENT_H_

//一函数头文件
#include <iostream>
#include <stdio.h>

using namespace std;
//二,宏定义

//三,函数申明


//四,类定义

//客户端节点类
class NodeClient
{
    public:
    NodeClient(int n_fd,struct sockaddr_in caddr_new){
        this->fd = n_fd;
        memcpy(&(this->caddr),&(caddr_new),sizeof(caddr));
    }

    int fd;         //连接套接字
    struct sockaddr_in caddr;//信息结构体

    private:

};

#endif
