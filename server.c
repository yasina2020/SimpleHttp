#include "server.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <fcntl.h>

int initListenFd(unsigned short port){
    
    // 创建套接字
    int listen_fd = socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd == -1){
        perror("socket");
        return -1;
    }

    // 端口复用:对端结束后，需要2msl时长才能释放端口，设置服用后可以不用等待这么久。
    int opt = 1;
    int ret = setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    if(ret == -1){
        perror("setSocketOpt");
        return -1;
    }

    // 绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(listen_fd,(struct sockaddr *)&addr,sizeof(addr));
    if(ret == -1){
        perror("bind");
        return -1;
    }

    // 监听
    ret = listen(listen_fd,128);
    if(ret == -1){
        perror("listen");
        return -1;
    }

    // 返回fd
    return listen_fd;
}


int epollRun(int lfd){

    // 创建rpoll实例
    int epoll_fd = epoll_create(1);
    if(epoll_fd == -1){
        perror("epoll_create");
        return -1;
    }

    // 向epoll红黑树添加lfd
    struct epoll_event ev;
    ev.data.fd = lfd;
    ev.events = EPOLLIN;
    int ret = epoll_ctl(epoll_fd,EPOLL_CTL_ADD,lfd,&ev);
    if(ret == -1){
        perror("epoll_ctl");
        return -1;
    }
    // 检测
    struct epoll_event evs[1024];
    int size = sizeof(evs)/sizeof(struct epoll_event);
    while(1){
        int num = epoll_wait(epoll_fd,evs,size,-1);
        for(int i=0;i<num;i++){
            int fd = evs[i].data.fd;
            if(fd == lfd){
                // 建立新连接 accept
                acceptClient(lfd,epoll_fd);
            }else{
                // 通信，主要是读对端的数据

            }
        }
    }
    return 0;
}


int acceptClient(int lfd,int epoll_fd){
    // 建立连接
    struct sockaddr_in caddr;
    int caddr_len = sizeof(caddr);
    int cfd = accept(lfd,(struct sockaddr*)&caddr,&caddr_len);
    if(cfd == -1){
        perror("accept");
        return -1;
    }

    // 设置cfd非阻塞：epoll模型用'边沿非阻塞'模式 
    int flag = fcntl(cfd,F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd,F_SETFL,flag);

    // 向epoll红黑树添加cfd
    struct epoll_event ev;
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(epoll_fd,EPOLL_CTL_ADD,cfd,&ev);
    if(ret == -1){
        perror("epoll_ctl");
        return -1;
    }

    return 0;
}