#include <threadPool.h>

// 任务结构体
typedef struct Task{
    void (*function)(void *arg);//这个任务要执行得函数
    void *arg;//函数参数
}Task;

// 任务队列（数组或者链表）,先进先出
// 任务队列上都是任务结构体


// 线程池结构体
struct ThreadPool{
    //1、先定义一个任务队列数组，
    // 在定义队列容量，当前大小，
    // 队尾：放任务，对头：取任务
    Task* taskQ;
    int queueCapacity;
    int queueSize;
    int queueRear;
    int queueFront;

    //2、线程池
    pthread_t managerID;//管理者线程得ID
    pthread_t *threadIDs; //存放工作线程ID的数组
    //3、因为要实现一个动态增减的线程池，故需要定义最小最大和当前线程数
    int minNum;
    int maxNum;
    int liveNum;//当前存在的线程数
    int busyNum; //当前工作中的线程数
    int exitNUm; //经过动态调整要杀死的线程数

    // 4、对线程池操作时加锁
    pthread_mutex_t mutexPool;
    // 对线程池中的busyNum操作时加锁
    pthread_mutex_t mutexBusy;
    // 任务队列是共享资源，故需条件变量
    pthread_cond_t notFull;//注：notFull命名的理由，非满就解除阻塞 
    pthread_cond_t notEmpty;//注：notEmpty命名的理由，非空就解除阻塞 

    //5、线程池状态
    int shutDown;
};


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
        // 3.其他初始化
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;//一开始忙线程为0
        pool->liveNum = min;//一开始就常见min个线程，所以活着的线程数时min
        pool->exitNUm = 0;//需要退出的线程数一开始为0，会动态调整
        // 4.锁和条件变量的初始化
        pthread_mutex_init(&pool->mutexPool,NULL);
        pthread_mutex_init(&pool->mutexBusy,NULL);
        pthread_cond_init(&pool->notFull,NULL);
        pthread_cond_init(&pool->notEmpty,NULL);
        // 5.任务队列的初始化
        pool->taskQ = (Task*)malloc(sizeof(Task)*cap);
        if(pool->taskQ == NULL){
            printf("pool->taskQ malloc error!\n");
            break;
        }
        pool->queueCapacity = cap;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;//因为一开始没有任务，所以尾在数组首部
        // 6.初始化线程池销毁标记
        pool->shutDown = 0;
        // 7.创建线程
        pthread_create(&pool->managerID,NULL,manager,pool);
        for(int i = 0; i < min; ++ i){
            // 注：这里的工作线程的工作就是去任务队列中取出一个任务，然后执行这个任务！！！！！
            pthread_create(&pool->threadIDs[i],NULL,worker,pool);
        }
    }
    while(0);

    if(pool && pool->threadIDs) free(pool->threadIDs);
    if(pool && pool->taskQ) free(pool->taskQ);
    if(pool) free(pool);
    
    return pool;
}


void *manager(void*arg){
    ThreadPool *pool = (ThreadPool *)arg;
    while(!pool->shutDown){
        sleep(3);

        
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
        }
        // 2.2 查看线程池是否被标记为销毁
        if(pool->shutDown){
            pthread_mutex_unlock(&pool->mutexPool);
            pthread_exit(NULL);
        }

        // 3.1. 取任务
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;
        // 3.2. 更新任务队列(循环队列)的一些值
        pool->queueFront = (pool->queueFront + 1) % (pool->queueCapacity);
        pool->queueSize --;
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