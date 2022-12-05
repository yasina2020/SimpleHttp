#include "server.h"


// 需要传入两个参数 服务器端口、服务器绑定的资源目录
int main(int argc, const char *argv){
    // if(argc<3){
    //     printf("USE: ./a.out port path\n");
    //     return -1;
    // }
    const char* p = "20000";
    unsigned short port = atoi(p);
    // 将服务器进程切换到用户指定的目录下面
    const char* c = "/home/fang/simpleHttp/src";
    chdir(c);
    // 初始化用于监听的套接字
    int listen_fd = initListenFd(port);
    // 启动服务器程序处理业务
    epollRun(listen_fd);

    return 0;
}
