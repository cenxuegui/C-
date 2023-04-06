#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <pthread.h>
#include <iostream>

using namespace std;

#define MAX_WAITING_TASKS	1000
#define MAX_ACTIVE_THREADS	20

struct task
{
	void *(*do_task)(void *arg);  // 函数指针 （执行实际上任务需要执行的函数）
	void *arg;			// 执行任务时所需要携带的参数

	struct task *next;	 // 后继指针， （单向不循环的链表）
};

typedef struct thread_pool
{
	pthread_mutex_t lock;  	// 互斥锁 ，用于互斥访问任务列表
	pthread_cond_t  cond;	// 条件变量 ， 用于方便提供场所为没有任务的线程进行挂起， 并便于唤醒他们执行任务

	bool shutdown;		// 标记当前线程池是否要被销毁

	struct task *task_list; 	// 任务链表的结构体

	pthread_t *tids;		// 输入的入口地址 （存放的是线程们的TID）

	unsigned max_waiting_tasks;	 // 最大的等待任务数量
	unsigned waiting_tasks;		// 正在等待被处理的任务数量
	unsigned active_threads;		// 当前线程池中的线程输入
}thread_pool;


bool init_pool(thread_pool *pool, unsigned int threads_number);
bool add_task(thread_pool *pool, void *(*do_task)(void *arg), void *task);
int  add_thread(thread_pool *pool, unsigned int additional_threads_number);
int  remove_thread(thread_pool *pool, unsigned int removing_threads_number);
bool destroy_pool(thread_pool *pool);

void *routine(void *arg);


#endif
