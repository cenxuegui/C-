#include "thread_pool.h"

// 当该线程收到取消请求时， 自动执行解锁操作
void handler(void *arg)
{
	printf("[%u] is ended.\n",
		(unsigned)pthread_self());

	// 当该线程收到取消请求时， 自动执行解锁操作
	pthread_mutex_unlock((pthread_mutex_t *)arg);
}


//线程设置
void *routine(void *arg)
{
	#ifdef DEBUG
	printf("[%u] is started.\n",
		(unsigned)pthread_self());
	#endif

	thread_pool *pool = (thread_pool *)arg;  // 把参数中的线程池管理结构体转换到本地指针
	struct task *p; // 任务结点类型的结构体

	while(1)
	{
		/*
		** push a cleanup functon handler(), make sure that
		** the calling thread will release the mutex properly
		** even if it is cancelled during holding the mutex.
		**
		** NOTE:
		** pthread_cleanup_push() is a macro which includes a
		** loop in it, so if the specified field of codes that 
		** paired within pthread_cleanup_push() and pthread_
		** cleanup_pop() use 'break' may NOT break out of the
		** truely loop but break out of these two macros.
		** see line 61 below.
		*/
		//================================================//
		pthread_mutex_lock(&pool->lock); //  阻塞等待上互斥锁
		pthread_cleanup_push(handler, (void *)&pool->lock);  // 把取消处理函数handler压入栈中  ， 并把互斥锁传递过去
		//================================================//

		// 1, no task, and is NOT shutting down, then wait
		while(pool->waiting_tasks == 0 && !pool->shutdown)
		{
			pthread_cond_wait(&pool->cond, &pool->lock); // 如果没有任务而且线程池没有被销毁则进入条件变量中等待
		}

		// 2, no task, and is shutting down, then exit 
		if(pool->waiting_tasks == 0 && pool->shutdown == true)
		{
			pthread_mutex_unlock(&pool->lock);
			pthread_exit(NULL); // CANNOT use 'break';
		}

		// 3, have some task, then consume it
		p = pool->task_list->next; // p 执行任务列表中的第一个有效结点
		pool->task_list->next = p->next; // 从链表中剔除 被取走的结点
		pool->waiting_tasks--;// 等待被执行的任务减一

		//================================================//
		pthread_mutex_unlock(&pool->lock); // 取到任务后 解锁 （互斥锁）
		pthread_cleanup_pop(0); // 弹出栈中的取消处理函数并设置为【不执行】
		//================================================//

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL); // 设置不响应取消请求
		(p->do_task)(p->arg); // 执行任务操作
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL); // 开启取消响应请求

		free(p); // 释放已经执行结束的任务结点
	}

	pthread_exit(NULL);
}



/********************************************线程池初始化*********************************************************/

/**
 * @brief 线程池初始化
 * 
 * @param (线程池管理结构体指针,线程数量) 
 * @return 失败返回false,成功返回true
 */
bool init_pool(thread_pool *pool, unsigned int threads_number)
{
	pthread_mutex_init(&pool->lock, NULL); // 互斥锁初始化
	pthread_cond_init(&pool->cond, NULL); // 条件变量初始化

	pool->shutdown = false;  // 销毁的开关设置为 否
	pool->task_list = (task*)malloc(sizeof(struct task)); 	// 创建任务链表的头结点
	pool->tids = (pthread_t*)malloc(sizeof(pthread_t) * MAX_ACTIVE_THREADS); // 申请一个堆数组用于存放线程号的数组

	if(pool->task_list == NULL || pool->tids == NULL)
	{
		perror("allocate memory error");
		return false;
	}

	pool->task_list->next = NULL; // 链表头结点的初始化 

	pool->max_waiting_tasks = MAX_WAITING_TASKS;  // 设置限制信息， 最大的任务等待数量
	pool->waiting_tasks = 0;	// 设置正在等待的任务数量为 0 
	pool->active_threads = threads_number; // 目前线程的活跃数量

	int i;
	int ErrNum = 0 ;
	for(i=0; i<pool->active_threads; i++)
	{
		ErrNum = pthread_create(&((pool->tids)[i]), NULL,routine, (void *)pool) ;
		if( ErrNum  != 0)
		{
			fprintf( stderr ,"create threads error :%s\n" , strerror(ErrNum));
			return false;
		}

		#ifdef DEBUG
		printf("[%u]:[%s] ==> tids[%d]: [%u] is created.\n",
			(unsigned)pthread_self(), __FUNCTION__,
			i, (unsigned)pool->tids[i]);
		#endif
	}

	return true;
}


/********************************************往任务链表里加任务*****************************************************/

/**
 * @brief 往任务链表里加任务
 * 
 * @param (线程池管理结构体指针,任务函数,参数) 
 * @return 失败返回false,成功返回true
 */
bool add_task(thread_pool *pool,void *(*do_task)(void *arg), void *arg)
{
	struct task *new_task = (task*)malloc(sizeof(struct task));
	if(new_task == NULL)
	{
		perror("allocate memory error");
		return false;
	}
	new_task->do_task = do_task;
	new_task->arg = arg;
	new_task->next = NULL;

	//============ LOCK =============//
	pthread_mutex_lock(&pool->lock);  // 阻塞上互斥锁 , 准备把新的任务结点插入任务列表中
	//===============================//

	if(pool->waiting_tasks >= MAX_WAITING_TASKS)
	{
		pthread_mutex_unlock(&pool->lock);

		fprintf(stderr, "too many tasks.\n");
		free(new_task);

		return false;
	}
	
	struct task *tmp = pool->task_list;
	while(tmp->next != NULL)  // 遍历寻找链表末尾
		tmp = tmp->next;

	// 从while 出来后tmp 则指向了链表的末尾
	tmp->next = new_task;
	pool->waiting_tasks++;

	//=========== UNLOCK ============//
	pthread_mutex_unlock(&pool->lock);  // 操作好任务列表后解锁
	//===============================//

	#ifdef DEBUG
	printf("[%u][%s] ==> a new task has been added.\n",
		(unsigned)pthread_self(), __FUNCTION__);
	#endif

	pthread_cond_signal(&pool->cond); // 唤醒一个在等待的线程
	return true;
}




/********************************************添加新线程*****************************************************/

/**
 * @brief 添加新线程
 * 
 * @param (线程池管理结构体指针,添加几个线程) 
 * @return (新线程的数量)
 */
int add_thread(thread_pool *pool, unsigned additional_threads)
{
	if(additional_threads == 0)
		return 0;

	unsigned total_threads =
			pool->active_threads + additional_threads;

	int i, actual_increment = 0;
	for(i = pool->active_threads;
	    i < total_threads && i < MAX_ACTIVE_THREADS;
	    i++)
	{
		if(pthread_create(&((pool->tids)[i]),
					NULL, routine, (void *)pool) != 0)
		{
			perror("add threads error");

			// no threads has been created, return fail
			if(actual_increment == 0)
				return -1;

			break;
		}
		actual_increment++; 

		#ifdef DEBUG
		printf("[%u]:[%s] ==> tids[%d]: [%u] is created.\n",
			(unsigned)pthread_self(), __FUNCTION__,
			i, (unsigned)pool->tids[i]);
		#endif
	}

	pool->active_threads += actual_increment;
	return actual_increment;
}





/********************************************删除线程*****************************************************/

/**
 * @brief 删除线程
 * 
 * @param (线程池管理结构体指针,删除几个线程) 
 * @return (删除线程后的线程活跃数)
 */
int remove_thread(thread_pool *pool, unsigned int removing_threads)
{
	if(removing_threads == 0) // 如果删除线程的数量为 0 则直接返回当前活跃的数量
		return pool->active_threads;


	// 为了确保线程池中至少还保留一个线程可用做一个计算
	int remaining_threads = pool->active_threads - removing_threads;
	remaining_threads = remaining_threads > 0 ? remaining_threads : 1;

	int i;
	for(i=pool->active_threads-1; i>remaining_threads-1; i--)
	{
		errno = pthread_cancel(pool->tids[i]); // 循环发送取消请求让线程们退出 

		if(errno != 0)
			break;

		#ifdef DEBUG
		printf("[%u]:[%s] ==> cancelling tids[%d]: [%u]...\n",
			(unsigned)pthread_self(), __FUNCTION__,
			i, (unsigned)pool->tids[i]);
		#endif
	}

	if(i == pool->active_threads-1)
		return -1;
	else
	{
		pool->active_threads = i+1;
		return i+1;
	}
}



/********************************************销毁线程池*****************************************************/

/**
 * @brief 销毁线程池
 * 
 * @param (线程池管理结构体指针) 
 * @return 成功返回true
 */
bool destroy_pool(thread_pool *pool)
{
	// 1, activate all threads
	pool->shutdown = true;
	pthread_cond_broadcast(&pool->cond);

	// 2, wait for their exiting
	int i;
	for(i=0; i<pool->active_threads; i++)
	{
		errno = pthread_join(pool->tids[i], NULL);
		if(errno != 0)
		{
			printf("join tids[%d] error: %s\n",
					i, strerror(errno));
		}
		else
			printf("[%u] is joined\n", (unsigned)pool->tids[i]);
		
	}

	// 3, free memories
	free(pool->task_list);
	free(pool->tids);
	free(pool);

	return true;
}







/*******************************************测试************************************************************/
// #include "thread_pool.h"

// //任务
// void *mytask(void *arg)
// {
// 	int n = (int)arg;

// 	printf("[%u][%s] ==> job will be done in %d sec...\n",
// 		(unsigned)pthread_self(), __FUNCTION__, n);

// 	sleep(n);

// 	printf("[%u][%s] ==> job done!\n",
// 		(unsigned)pthread_self(), __FUNCTION__);

// 	return NULL;
// }





// /********************************************显示时间*********************************************************/

// /**
//  * @brief 显示时间
//  * 
//  * @param (空) 
//  * @return NULL
//  */
// void *count_time(void *arg)
// {
// 	int i = 0;
// 	while(1)
// 	{
// 		sleep(1);
// 		printf("sec: %d\n", ++i);
// 	}
// 	return NULL;
// }



// int main(void)
// {
// 	pthread_t a;
// 	pthread_create(&a, NULL, count_time, NULL);

// 	//1.对管理结构体进行初始化
// 	thread_pool *pool = malloc(sizeof(thread_pool));

// 	//2.线程池初始化
// 	init_pool(pool, 2);

// 	//3.往任务链表里加任务
// 	printf("throwing 3 tasks...\n");
// 	add_task(pool, mytask, (void *)(rand()%10));
// 	add_task(pool, mytask, (void *)(rand()%10));
// 	add_task(pool, mytask, (void *)(rand()%10));

// 	//4.输出当前活跃线程数
// 	printf("current thread number: %d\n",remove_thread(pool, 0));
// 	sleep(9);

// 	//5.添加新任务
// 	printf("throwing another 2 tasks...\n");
// 	add_task(pool, mytask, (void *)(rand()%10));
// 	add_task(pool, mytask, (void *)(rand()%10));

// 	//6.添加新线程
// 	add_thread(pool, 2);

// 	sleep(5);

// 	//7.删除线程
// 	printf("remove 3 threads from the pool, "
// 	       "current thread number: %d\n",
// 			remove_thread(pool, 3));

// 	//8.销毁线程池
// 	destroy_pool(pool);
// 	return 0;
// }
