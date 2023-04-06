#ifndef _MYLIST_H_     //预编译指令，作用是：防止递归包含以下内容
#define _MYLIST_H_

//一函数头文件
#include <iostream>
#include <stdio.h>
#include <list>
#include "nodeclient.h"

using namespace std;
//二,宏定义

//三,函数申明


//四,类定义


//自定义链表类
class MyList
{
    public:
    MyList(){
        my_list = new list<NodeClient>;
    }
    ~MyList(){
        my_list->clear();   //清空链表
        delete my_list;     //释放
    }

    //增加成员
    void Add_Node(NodeClient New){
        my_list->push_back(New);// 在表尾插入元素
    }
    //删除成员
    int Del_Node(int new_fd){
        //遍历迭代器
        for (it = my_list->begin(); it != my_list->end(); ++it)
        {
            if (it->fd == new_fd)
            {
                //找到成员了
                my_list->erase(it);//移除元素
                return 0;
            }  
        }
        return -1;
        
    }
    //查找成员
    int Find_Node(int new_fd){
        //遍历迭代器
        for (it = my_list->begin(); it != my_list->end(); ++it)
        {
            if (it->fd == new_fd)
            {
                //找到fd了
                return it->fd;
            }  
        }
        return -1;
    }
    private:
    list<NodeClient> *my_list;           //链表的入口地址
    list<NodeClient>::iterator it;       //定义迭代器
};










#endif
