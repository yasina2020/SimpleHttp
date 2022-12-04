#pragma once



// 初始化用于监听的套接字
// port:端口号5000-65535
// return：listen_fd，失败返回-1
int initListenFd(unsigned short port);

// 启动epoll模型处理业务
// lfd:监听socket_fd
// return：
int epollRun(int lfd);

// 从lfd中接收cfd，再将cfd加入到epoll检测中
int acceptClient(int lfd,int epoll_fd);


