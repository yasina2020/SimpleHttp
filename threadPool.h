#pragma once


#include <stdio.h>
#include <errno.h>
#include <strings.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>


/*
线程池要干啥呢？
1、在一开始创建一些线程
2、把这些线程的tid放到一个数组里
3、这些线程去读任务队列中的任务
4、任务队列是一个共享资源，多个线程都要请求，所以要使用锁
5、获取到任务后就处理这些任务
6、将处理结果返回给cfd
7、或许第6步得处理不太好，

工作线程的工作就是去任务队列中取出一个任务，然后执行这个任务！！！！！
因此我们可以将要执行的函数给封装成task，挂在任务队列中，线程池中的线程的工作就是去取我们挂的task，然后执行task里面封装的函数
*/

////////////////一些变量的定义//////////////////////////////////////////////////////////////////////
#define NUMBER 2

// 任务结构体
typedef struct TASK{
    void (*function)(void *arg);//这个任务要执行得函数
    void *arg;//函数参数
}Task;

// 线程池结构体
typedef struct THREADPOOL{
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
}ThreadPool;


////////////////一些函数的声明//////////////////////////////////////////////////////////////////////

// 创建线程池并初始化min最小线程数 max最大线程数 cap任务队列容量
ThreadPool* initThreadPool(int min,int max,int cap);

// 管理者线程执行的函数
void *manager(void*arg);

// 工作线程执行的函数：取任务
void *worker(void*arg);

// 线程退出函数
void threadExit(ThreadPool* pool);

// 销毁线程池
int destroyPool(ThreadPool* pool);

// 向线程池（中的任务队列）加任务
ThreadPool* addTask2Pool(Task * task,ThreadPool* pool);

// 获取当前忙碌的工作线程数
int getBusyNum(ThreadPool* pool);

// 获取当前活着的工作线程数
int getLiveNum(ThreadPool* pool);
