#pragma once

#include <arpa/inet.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <sys/stat.h>
#include <assert.h>
#include <dirent.h>
#include <sys/sendfile.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <iconv.h>
#include <ctype.h>

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

// 接收HTTP请求
int recvHttpRequest(int fd,int epoll_fd);

// 解析http协议
/* get请求
首行
头
空
空
*/ 
// 解析http协议:这里解析get请求得请求首行
int parseRequestLine(const char * line,int cfd);

// 将http_body文件内容发送给客户端
int sendFile(const char *fileName,int cfd);

// 发送响应头(状态行+响应头（暂时先组装两个：内容类型、长度）+响应空行)
int sendHeadMsg(int cfd,int status,const char *descr,const char *type,int length);

// 根据文件名获取文件类型，返回http所接受的类型名称
const char* getFileType(const char* name);

// 发送目录
int sendDir(const char *dirName,int cfd);

// 将浏览器发来的url中的中文16进制乱码转位服务器可以理解的utf-8
// 将src-->dst
void hexToUtf8(char *src_str,char *dst_str);
