#include "threadPool.h"


// 任务队列（数组或者链表）,先进先出
// 任务队列上都是任务结构体


ThreadPool* initThreadPool(int min,int max,int cap){

        // 1.在堆上实例化线程池对象
    ThreadPool *pool =(ThreadPool*) malloc(sizeof(ThreadPool));
    do{//这里使用dowhile是为了在初始化失败时更好的释放开辟的内存
        
        if(pool == NULL){
            printf("pool malloc error!\n");
            break;
        }
        // 2.在堆上为工作线程分配max大小的空间
        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t)*max);
        if(pool->threadIDs == NULL){
            printf("pool->threadIDs malloc error!\n");
            break;
        }
        memset(pool->threadIDs,0,sizeof(pthread_t)*max);
        printf("memset(pool->threadIDs,0,sizeof(pthread_t)*max);\n");
        // 3.其他初始化
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;//一开始忙线程为0
        pool->liveNum = min;//一开始就常见min个线程，所以活着的线程数时min
        pool->exitNUm = 0;//需要退出的线程数一开始为0，会动态调整
        // 4.锁和条件变量的初始化
        if(pthread_mutex_init(&pool->mutexPool,NULL)!=0||pthread_mutex_init(&pool->mutexBusy,NULL)!=0||
            pthread_cond_init(&pool->notFull,NULL)!=0||pthread_cond_init(&pool->notEmpty,NULL)!=0){
            
            printf("锁和条件变量的初始化失败\n");
            break;
        }
        
        

        // 5.任务队列的初始化
        pool->taskQ = (Task*)malloc(sizeof(Task) * cap);
        if(pool->taskQ == NULL){
            printf("pool->taskQ malloc error!\n");
            break;
        }
        pool->queueCapacity = cap;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;//因为一开始没有任务，所以尾在数组首部
        printf("任务队列的初始化\n");
        // 6.初始化线程池销毁标记
        pool->shutDown = 0;
        // 7.创建线程
        pthread_create(&pool->managerID,NULL,manager,(void *)pool);
        for(int i = 0; i < min; ++ i){
            // 注：这里的工作线程的工作就是去任务队列中取出一个任务，然后执行这个任务！！！！！
            pthread_create(&pool->threadIDs[i],NULL,worker,(void *)pool);
        }
        printf("ThreadPool* pool 创建成功\n");
        return pool;
    }
    while(0);

    if(pool && pool->threadIDs) free(pool->threadIDs);
    if(pool && pool->taskQ) free(pool->taskQ);
    if(pool) free(pool);
    printf("ThreadPool* pool 创建失败\n");
    return NULL;
    
}


void *manager(void*arg){
    ThreadPool *pool = (ThreadPool *)arg;
    while(!pool->shutDown){
        sleep(3);
        //  取线程池的状态
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexPool);


        // 动态添加线程:任务数>存活的线程数 && 存活的线程数<最大线程数
        if(queueSize>liveNum && liveNum<pool->maxNum){
            int counter = 0;//用来记录这次调整加了几个线程，增加的线程数不能超过规定的NUMBER

            pthread_mutex_lock(&pool->mutexPool);
            for(int i=0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; ++i){
                // 在threadIDs中找到一个没有被使用的，把线程ID放进去
                if(pool->threadIDs[i] == 0){
                    pthread_create(&pool->threadIDs[i],NULL,worker,pool);
                    counter++;
                    pool->liveNum ++;
                    liveNum = pool->liveNum;
                    busyNum = pool->busyNum;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }
        // 动态销毁线程:忙线程*2 <存活的线程 && 存活的线程 > 最小线程个数
        if(busyNum*2 < liveNum && liveNum > pool->minNum){
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNUm = NUMBER;//一次销毁两个
            pthread_mutex_unlock(&pool->mutexPool);
            for(int i=0;i<NUMBER;i++){
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }
}


void *worker(void *arg){
    ThreadPool *pool = (ThreadPool *)arg;
    while(1){//他的任务就是一直取任务
        // 1. 取任务之前加锁
        pthread_mutex_lock(&pool->mutexPool);

        // 2.1. 查看有没有任务可取
        while(pool->queueSize == 0 && !pool->shutDown){
            // 2.1.1. 没有任务可取，取线程池没有被标记为销毁，就利用条件变量阻塞等待
            pthread_cond_wait(&pool->notEmpty,&pool->mutexPool);
            // 2.1.2. 解除阻塞后，判断是否有需要退出的线程，如果有的话，就退出。
                    //除了taskQ非空会解除阻塞的外，若管理者线程发现有需要销毁线程时，也会发送解除阻塞的信号 
                    // 区别就是，taskQ解除的阻塞，exitNUm不会变，是需要工作
                    // 管理者接触的阻塞，exitNUm会+1，是需要销毁
            if(pool->exitNUm > 0){
                pool->exitNUm--;
                if(pool->liveNum > pool->minNum){
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);
                }
            }
        }
        // 2.2 查看线程池是否被标记为销毁
        if(pool->shutDown){
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);

        }

        // 3.1. 取任务
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;
        // 3.2. 更新任务队列(循环队列)的一些值 解锁addTask2Pool阻塞
        pool->queueFront = (pool->queueFront + 1) % (pool->queueCapacity);
        pool->queueSize --;
        pthread_cond_signal(&pool->notFull);
        // 4. 解锁线程池
        pthread_mutex_unlock(&pool->mutexPool);

        // 5.1. 更改busyNum
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum ++;
        pthread_mutex_unlock(&pool->mutexBusy);
        // 5.2. 执行任务函数
        task.function(task.arg);
        // 5.3. 函数执行完之后再次更改busyNum
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum --;
        pthread_mutex_unlock(&pool->mutexBusy);
    }
}


void threadExit(ThreadPool* pool){
    pthread_t tid = pthread_self();
    for(int i=0;i<pool->maxNum;i++){
        if(tid == pool->threadIDs[i]){
            pool->threadIDs[i] = 0;
            pthread_exit(NULL);
        }
    }
}


int destroyPool(ThreadPool* pool){

    if(pool == NULL){
        return -1;
    }
    // 回收管理线程
    pool->shutDown = 1;
    pthread_join(pool->managerID,NULL);
    //唤醒work线程,让其自己退出 
    for(int i=0;i<pool->liveNum;i++){
        pthread_cond_signal(&pool->notEmpty);
    }
    
    // 释放锁
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_mutex_destroy(&pool->mutexPool);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);

    // 释放空间
    if(pool->threadIDs!=NULL) free(pool->threadIDs);
    if(pool->taskQ!=NULL) free(pool->taskQ);
    if(pool!=NULL) free(pool);
    pool = NULL;

    return 0;
}


ThreadPool* addTask2Pool(Task * task,ThreadPool* pool){
    // 锁poll
    pthread_mutex_lock(&pool->mutexPool);
    // 阻塞taskQ
    while(pool->queueSize == pool->queueCapacity && !pool->shutDown){
        pthread_cond_wait(&pool->notFull,&pool->mutexPool);
    }
    if(pool->shutDown){
        pthread_mutex_unlock(&pool->mutexPool);
        return NULL;
    }
    // taskQ非满就加
    pool->taskQ[pool->queueRear] = *task;
    // 更新TaskQ各项指标
    pool->queueRear = (pool->queueRear+1) % pool->queueCapacity;
    pool->queueSize ++;
    // 唤醒worker阻塞  解锁pool
    pthread_cond_signal(&pool->notEmpty);
    pthread_mutex_unlock(&pool->mutexPool);

    return pool;
}

int getBusyNum(ThreadPool* pool){
    int busyNum;
    pthread_mutex_lock(&pool->mutexBusy);
    busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

int getLiveNum(ThreadPool* pool){
    int liveNum;
    pthread_mutex_lock(&pool->mutexPool);
    liveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return liveNum;
}