#include "server.h"
extern Server sp1;

/************************************************执行函数***************************************************************/
int  Server::perform(void){

    socklen_t addrlen = sizeof(struct sockaddr_in);//记录结构体长度的

    //1.创建套接字 (购买一个通信设备 手机)  [待连接套接字]
    if (fd == -1 )
    {
        perror("socker ");
        return -1 ;
    }
    perror("socker ");

    struct sockaddr_in ServerAddr;
    int a = INADDR_ANY;// 地址不做强制绑定
    memcpy(&ServerAddr.sin_addr,&a,sizeof(ServerAddr.sin_addr));
    ServerAddr.sin_port = htons(65003);
    ServerAddr.sin_family = AF_INET;

    //2.把地址信息与套接字进行绑定 (把手机卡 插入手机进行绑定)
    if(bind(fd, (struct sockaddr *)&ServerAddr, addrlen))
    {
        perror("bind");
        return -1; 
    }
    perror("bind");
    
    //3.把待连接套接字设置为 监听套接字  (开机并设置来电铃声) [待连接套接字] --> [监听套接字]
    if(listen(fd , 2 ))
    {
        perror("listen ");
        return -1 ;
    }
    perror("listen ");

    //4.创建epoll
    int epfd = epoll_create(MAXSIZE);
    if (epfd == -1)
    {
        perror("fail to epoll_create");
        exit(1);
    }

    //5.往epoll添加要监听的fd
    struct epoll_event ev;//服务于epoll_ctl( )函数
    ev.data.fd = fd;
    ev.events = EPOLLIN;//监听fd读操作
    int ret = epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&ev);
    if (ret < 0)
    {
        perror("fail to epoll_ctl");
        exit(1);
    }

    //6.死循监听前的准备工作
    struct sockaddr_in caddr;
    int caddrlen = sizeof(caddr);
    char buff[1024];
    memset(buff,0,sizeof(buff));
    char ip[30] = {0};
    int i;
    int num;
    int len;
    init_pool(pool, 10);//2.线程池初始化
    
    //7.epoll持续监听
    while (1)
    {
        //1.epoll监听fd
        num = epoll_wait(epfd,events,MAXSIZE, -1);//-1表示阻塞
        if (num < 0)
        {
            perror("fail to epoll_wait");
            exit(1);
        }
        //2.处理有反应的fd
        for ( i = 0; i < num; i++)
        {
            //(1)判断是不是监听套接字[fd],如果是则连接客户端
            if (events[i].data.fd == fd)
            {
                int fdc = accept(fd,(struct sockaddr *)&caddr,(socklen_t *)(&caddrlen));//连接客户端,得到连接套接字fdc
                if (fdc == -1)
                {
                    perror("fail to accept");
                    exit(0);
                }
                ev.data.fd = fdc;
                ev.events = EPOLLIN;
                ret = epoll_ctl(epfd,EPOLL_CTL_ADD, fdc, &ev);//往epoll加入连接套接字fdc
                if (ret < 0)
                {
                    perror("fail to epoll_ctl");
                    exit(1);
                }
                //将客户端插入到链表中
                NodeClient Newclient(fdc,caddr);
                con->Add_Node(Newclient);
                Client_Num++;
                printf("[fd:%d]客户端%s:%d已连接,当前客户端数量:%d\n",fdc,inet_ntoa(caddr.sin_addr),ntohs(caddr.sin_port),Client_Num);
                
            }

            //(2)判断是不是连接套接字(表示有人发消息过来了!!!)
            else if (events[i].data.fd & EPOLLIN)
            {
                bzero(buff,sizeof(buff));
                len = recv(events[i].data.fd , buff , 1024 , 0 );//接收消息
                if ( len > 0 )
                {
                    //printf("[当前用户数量:%d],接收的内容为：%s\n"  , Client_Num,buff);
                    send_data NewData = {.fd = events[i].data.fd};
                    memcpy(NewData.msg,buff,1024);
                    add_task(this->pool,Forwarding,&NewData); //加入到线程池中
                }
                else if ( len == 0 )
                {
                    Client_Num--;
                    printf("[fd = %d]连接已断开,当前客户端数量为%d\n",events[i].data.fd,Client_Num);
                    ev.data.fd = events[i].data.fd;
                    ev.events = EPOLLIN;
                    epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd, &ev);//删除epoll中的连接套接字
                    con->Del_Node(events[i].data.fd);//删除链表成员
                    close(events[i].data.fd);//随手关描述符,因为连接套接字本质上就是文件描述符
        
                    break; 
                }
                else{
                    perror("fail to recv");
                    Client_Num--;
                }
            }         
        }      
    } 
}






/**************************************转发消息线程函数************************************************************************/
void *Forwarding (void *arg)
{
    send_data *Data = (send_data *)arg;
    int fd_s = Data->fd;//发件人
    int fd_r;           //收件人
    char mep[1024];      //要发的内容
    sscanf(Data->msg,"%d,%s",&fd_r,mep);

    //判断发件人是否存在
    int ret = sp1.con->Find_Node(fd_r);
    if (ret == -1)
    {
        //收件人不在
        bzero(mep,1024);
        memcpy(mep,"no_person,send_error......",1024);
        send(fd_s, mep , strlen(mep) , 0 );
    }
    else
    {
        //收件人在
        ssize_t retval = send(fd_r, mep , strlen(mep) , 0 );
        if (retval > 0 )
        {    
            bzero(mep,1024);
            memcpy(mep,"send_succes......",1024);
            send(fd_s, mep , strlen(mep) , 0 );
        }
        else
        {
            bzero(mep,1024);
            memcpy(mep,"send_error...",1024);
            send(fd_s, mep , strlen(mep) , 0 );
        }
    }  
    return NULL;
}