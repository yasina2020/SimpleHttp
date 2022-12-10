#include "threadPool.h"



void taskfunc(void *arg){
    int i = *(int *)arg;
    pthread_t tid = pthread_self();
    printf("Number is %d  and thread ID is %ld\n",i,tid);
    sleep(1);
}

int main(){
    ThreadPool* pool = initThreadPool(2,10,100);
    printf("pool size is %ld\n",sizeof(pool));
    for(int i =0;i<100;i++){
        printf("1111\n");  
        int * num = (int *)malloc(sizeof(int));
        num = &i;
        printf("num = %d\n",*num);
        Task *task = (Task *)malloc(sizeof(Task));
        task->arg = num;
        task->function = taskfunc;
        addTask2Pool(task,pool);
    }
    sleep(30);
    destroyPool(pool);
    return 0;
}