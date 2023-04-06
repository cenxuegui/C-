#ifndef _SERVER_H     //预编译指令，作用是：防止递归包含以下内容
#define _SERVER_H

//一函数头文件
#include <iostream>
#include <stdio.h>
#include <list>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include "thread_pool.h"
#include "nodeclient.h"
#include "mylist.h"
#define MAXSIZE 2000//设置服务器的容量

using namespace std;
//二,宏定义

//三,函数申明

void * Send_p(void *mags);  //发消息的线程函数
void *Forwarding (void *arg);     //转发消息线程函数

//定义数据类型
//发消息时,传参用的结构体
typedef struct send_data
{
    int fd;
    char msg[1024];
}send_data;


class Server
{
    public:
    //1.构造函数
    Server(){
        this->Client_Num = 0;
        this->fd = socket(AF_INET , SOCK_STREAM , 0 );
        this->events = new struct epoll_event[2000];
        this->pool = new thread_pool;
        this->con = new MyList;
    };
    //2.析构函数
	~Server(){
        close(fd);
        destroy_pool(pool);//销毁线程池
        delete []events;
        delete pool;
        delete con;
    };
	//3.执行函数
    int perform(void);

    //4.友元函数
    friend void *Forwarding (void *arg);

    private:
    int Client_Num;             //当前客户端数量
    int fd;                     //监听套接字
    struct epoll_event * events;//存放就绪fd的数组
    thread_pool *pool;          //线程池管理结构体入口地址
    MyList *con;                //自定义链表入口地址
    


};




#endif
