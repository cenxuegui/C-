#include "thread_pool.h"
#include "server.h"
#include <memory>

Server sp1;

int main(int argc, char const *argv[])
{
    cout<<"C++第六次测试,已集成epoll监听功能和线程池代发消息功能"<<endl;
    
    sp1.perform();

    return 0;
}