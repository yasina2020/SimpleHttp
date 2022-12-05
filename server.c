#include "server.h"


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
            int cfd = evs[i].data.fd;
            if(cfd == lfd){
                // 建立新连接 accept
                acceptClient(lfd,epoll_fd);
                printf("连接已建立%d\n",cfd);
            }else{
                // 通信，主要是读对端的数据
                printf("接收到请求%d\n",cfd);
                recvHttpRequest(cfd,epoll_fd);
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


int recvHttpRequest(int cfd,int epoll_fd){
    // 因为是ET模式，所以要一次性把数据都读完
    int len = 0,total = 0;
    char tmp[1024] = {0};//临时接收数据，然后放入buf中
    char buf[4096] = {0};
    while((len = recv(cfd,tmp,sizeof(tmp),0)) > 0){
        if(total + len <sizeof(buf)){
            memcpy(buf + total,tmp,len);
        }
        total +=len;
    }
    //判断数据是否被接收完毕
    /*
        因为fd是非阻塞的，所以recv在出错和读取完数据时，都会返回-1，
        要靠errno来区分这两种情况：errno=EAGAIN  缓冲区中的数据被读取完了
                                 errno=        读取出错
    */ 
    if(len == -1 && errno == EAGAIN){
        // 数据读入完毕，开始解析http
        char * pt = strstr(buf,"\r\n");
        int reqLen = pt - buf;
        buf[reqLen] = '\0';
        parseRequestLine(buf,cfd);
    }else if(len == 0){
        // 客户端断开连接
        epoll_ctl(epoll_fd,EPOLL_CTL_DEL,cfd,NULL);
        close(cfd);
    }else{
        perror("recv");
    }
    return 0;
}

// 处理http请求首行（sscanf）
int parseRequestLine(const char * line,int cfd){
/*
sscanf:按一定格式解析字符串
sprintf:按一定格式写入字符串
strcasecmp:字符串比大小忽略大小写：<0 s1小；=0 相等；>0 s1大
*/ 
    
    // 解析首行得前两个 get /video/BV1XB4y http/1.1
    char method[12]={0};
    char path_hex[1024]={0};
    sscanf(line,"%[^ ] %[^ ] ",method,path_hex);
    // 处理method
    if(strcasecmp(method,"get") != 0){
        return -1;
    }
    // 处理path
    char path[1024]={0};
    hexToUtf8(path_hex,path);
    printf("mrthod %s,path %s\n",method,path);


        // 1.将file设置为path，如果是根目录path会/,要设置为./
    char * file = NULL;
    if(strcmp(path,"/")==0){
        file = "./";
    }else{
        file = path + 1;
    }
        // 2.判断file是路径还是文件
    struct stat st;
    int ret = stat(file,&st);
    if(ret == -1){
        // 文件不存在  回复404
        sendHeadMsg(cfd,404,"Not Found",getFileType(".html"),-1);
        sendFile("404.html",cfd);
        return 0;
    }
    if(S_ISDIR(st.st_mode)){
        // 是目录则将目录发送给客户端
        sendHeadMsg(cfd,200,"OK",getFileType(".html"),-1);
        sendDir(file,cfd);
    }else{
        // 是文件则将文件内容发送给客户端
        sendHeadMsg(cfd,200,"OK",getFileType(file),st.st_size);
        sendFile(file,cfd);
    }
    return 0;
}

int sendFile(const char *fileName,int cfd){
    // 将file打开，读出来放到cfd中，读一部分发一部分
    int fd = open(fileName,O_RDONLY);
    assert(fd > 0);
// 两种发文件得方法
#if 0
    while(1){
        char buf[1024];
        int len = read(fd,buf,sizeof buf);
        if(len >0){
            send(cfd, buf,len,0);
            // 注：发的速度太快的话，客户端可能解析不过来，所以要控制一下发送的节奏
            /*
            个人思考，在这里监听cfd写事件，如果写事件发生，就表明可写，即客户端已经把数据读走了，但可能没有处理完
            */ 
            usleep(10);
        }else if(len == 0){
            break;
        }else{
            perror("read");
            // return -1;
        }
    }

#else
    struct stat st;
    stat(fileName,&st);
    int file_size = st.st_size;

/*
ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
out_fd:接收方
in_fd：发送方
offset：偏移量，输入输出参数，会自动修改
    发送前根据offset读文件的偏移
    发送后将offset设置为发送到的偏移
count：文件大小
*/ 
    off_t offset = 0;
    while(sendfile(cfd,fd,&offset,file_size-offset)!=0);
    

#endif

    close(cfd);
    return 0;
}

int sendHeadMsg(int cfd,int status,const char *descr,const char *type,int length){
    // 状态行
    char buf[4095]={0};
    sprintf(buf,"http/1.1 %d %s\r\n",status,descr);
    // 响应头
    sprintf(buf + strlen(buf),"content-type: %s\r\n",type);
    sprintf(buf + strlen(buf),"content-length: %d\r\n",length);
    // http响应空行
    sprintf(buf + strlen(buf),"\r\n");

    send(cfd,buf,strlen(buf),0);
    return 0;
}

const char* getFileType(const char* name){
    // 从右往左找，找后缀名
    const char* dot = strrchr(name,'.');
    if(dot == NULL){
        return "text/plain; charset=utf-8";
    }
    if(strcmp(dot,".html")==0 || strcmp(dot,".htm")==0){
        return "text/html; charset=utf-8";
    }
    if(strcmp(dot,".html")==0 || strcmp(dot,".htm")==0){
        return "text/html; charset=utf-8";
    }
    if(strcmp(dot,".jpg")==0 || strcmp(dot,".jpeg")==0){
        return "image/jpeg";
    }
    if(strcmp(dot,".png")==0){
        return "image/png";
    }
    if(strcmp(dot,".mkv")==0){
        return "video/x-matroska";
    }
    

    return "text/plain; charset=utf-8";

}


// 发送目录
// #define __USE_XOPEN2K8
int sendDir(const char *dirName,int cfd){
    char buf[4096] = {0};
    sprintf(buf,
    "<html><head><title>%s</title></head><body><table>",
    dirName);


    struct dirent** nameList;
    int num = scandir(dirName,&nameList,NULL,alphasort);
    for(int i=0;i<num;i++){
        char * name = nameList[i]->d_name;
        struct stat st;
        char subPath[1024] = {0};
        sprintf(subPath,"%s/%s",dirName,name);
        stat(subPath,&st);
        if(S_ISDIR(st.st_mode)){
            sprintf(buf+strlen(buf),
            "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
            name,name,st.st_size);
        }else{
            sprintf(buf+strlen(buf),
            "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
            name,name,st.st_size);
        }
        send(cfd,buf,strlen(buf),0);
        memset(buf,0,sizeof(buf));
        free(nameList[i]);
    }
    sprintf(buf,"</table></body></html>");
    send(cfd,buf,strlen(buf),0);
    free(nameList);

    return 0;
}

int hextoDec(char c){
    if(c >= '0' && c<= '9'){
        return c-'0';
    }
    if(c >= 'a' && c <= 'f'){
        return c-'a'+10;
    }
    if(c >= 'A' && c <= 'F'){
        return c-'A'+10;
    }
    return 0;
}

void hexToUtf8(char *src,char *dst){
    for(;*src != '\0';++src,++dst){
        if(src[0] == '%' && isxdigit(src[1]) && isxdigit(src[2])){
            *dst = hextoDec(src[1]) * 16 + hextoDec(src[2]);
            src += 2;
        }else{
            *dst = *src;
        }
    }
}


