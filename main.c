#include "server.h"

int main(){

    // 初始化用于监听的套接字
    int listen_fd = initListenFd(20000);
    // 启动服务器程序处理业务
    epollRun(listen_fd);

    return 0;
}
