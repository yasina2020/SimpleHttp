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

// 线程池结构体
typedef struct ThreadPool ThreadPool;

// 管理者线程执行的函数
void *manager(void*arg);
// 工作线程执行的函数：取任务
void *worker(void*arg);


// 创建线程池并初始化
// min最小线程数 max最大线程数 cap任务队列容量
ThreadPool* initThreadPool(int min,int max,int cap);

// 销毁线程池
// 向线程池（中的任务队列）加任务
// 获取当前忙碌的工作线程数
// 获取当前活着的工作线程数
