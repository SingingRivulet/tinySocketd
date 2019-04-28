#include "tinysocketd.h"
namespace tinySocketd{

void msg_base::setnonblocking(int sock){
    int opts;
    opts = fcntl(sock, F_GETFL);
    if(opts < 0) {
        perror("fcntl(sock, GETFL)");
        return;
    }
    opts = opts | O_NONBLOCK;
    if(fcntl(sock, F_SETFL, opts) < 0) {
        perror("fcntl(sock, SETFL, opts)");
        return;
    }
}
int msg_base::startup(u_short *port){
    int rpcd = 0;
    struct sockaddr_in name;
    rpcd = socket(PF_INET, SOCK_STREAM, 0);
    if (rpcd == -1)
        return -1;
    int reuse = 1;
    if (setsockopt(rpcd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        return -1;
    }
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(rpcd, (struct sockaddr *)&name, sizeof(name)) < 0)
         return -1;
    if (*port == 0){
        int namelen = sizeof(name);
        if (getsockname(rpcd, (struct sockaddr *)&name, (socklen_t *)&namelen) == -1)
            return -1;
        *port = ntohs(name.sin_port);
    }
    if (listen(rpcd, 5) < 0)
        return -1;
    return (rpcd);
}
void msg_base::run(u_short p,int maxnum){
    running=true;
    int listenfd = -1;
    u_short port = p;
    int connfd = -1;

    struct sockaddr_in client;
    int client_len = sizeof(client);
      
    signal(SIGPIPE,[](int){});
    //绑定监听端口
    listenfd = startup(&port);
      
      
    struct epoll_event ev, events[20];
    epfd = epoll_create(maxnum);
    setnonblocking(listenfd);
    ev.data.fd = listenfd;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
      
      
    while (running){
        
        int nfds = epoll_wait(epfd, events, 20, 4000);
        loop();
        for(int i = 0; i < nfds; ++i) {
            if(events[i].data.fd == listenfd) {
            
                connfd = accept(listenfd, (struct sockaddr *)&client, (socklen_t *)&client_len);
                if(connfd < 0) {
                    continue;
                }
            
                onConnect(connfd);
            
                setnonblocking(connfd);
                ev.data.fd = connfd;
                ev.events = EPOLLIN | EPOLLHUP;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            
            }else if(events[i].events & EPOLLIN) {
                if((connfd = events[i].data.fd) < 0) continue;
                //处理请求
                char cbuf[256];
                int len;
                if((len=read(connfd, cbuf, 256)) <= 0) {
              
                    onQuit(connfd);
                    ev.data.fd = connfd;
                    ev.events = 0;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, &ev);
              
                }else{
              
                    //printf("msg\n");
                    onMessage(connfd,cbuf,len);
              
                    ev.data.fd = connfd;
                    ev.events = EPOLLIN | EPOLLHUP;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, connfd, &ev);
              
                }
            }else if(events[i].events & EPOLLHUP) {
                if((connfd = events[i].data.fd) < 0) continue;
            
                onQuit(connfd);
            
            }else if(events[i].events & EPOLLOUT) {
                if((connfd = events[i].data.fd) < 0) continue;
                onWriAble(connfd);
            }
        }
    }
    close(listenfd);
    close(epfd);
    destruct();
    return;
}

void msg::conn::reset(){
    bzero(buf,256);
    ptr=0;
}

void msg::conn::append(char c){
    char ic;
    if(ptr>=255){
        buf[255]='\0';
        parent->onFDMessage(fd,buf,255);
        reset();
    }
    
    if(c=='\0' || c=='\n')
        ic='\0';
    else
        ic=c;
    buf[ptr]=ic;
    ++ptr;
    if(ic=='\0'){
        parent->onMessage(fd,buf,ptr);
        reset();
    }
}

void msg::conn::quit(){
    parent->onFDQuit(fd);
}

void msg::conn::init(int ifd,msg * ptr){
    reset();
    fd=ifd;
    parent=ptr;
    ptr->onFDConnect(ifd);
}

void msg::onConnect(int ifd){
    conns[ifd].init(ifd,this);
}

void msg::onQuit(int connfd){
    auto it=conns.find(connfd);
    if(it==conns.end())return;
    conn & cp=it->second;
    cp.quit();
}

void msg::onMessage(int connfd,const char * fst,int len){
    auto it=conns.find(connfd);
    if(it==conns.end())return;
    conn & cp=it->second;
    
    for(int j=0;j<len;j++)
        cp.append(fst[j]);
    
    while(1){
        char buf[256];
        int slen=read(connfd,buf,256);
        if(slen<=0)return;
        
        for(int j=0;j<slen;j++)
            cp.append(buf[j]);
        
        if(slen<256)
            return;
    }
    
}

void auth::onFDConnect(int connfd){
    
}

void auth::onFDMessage(int connfd,const char * fst,int len){
    if(authed.find(connfd)==authed.end()){
        if(onAuth(connfd,fst,len)){
            authed.insert(connfd);
            onAuthedConnect(connfd);
        }
    }else{
        onAuthedMessage(connfd,fst,len);
    }
}

void auth::onFDQuit(int connfd){
    if(authed.find(connfd)!=authed.end()){
        onAuthedQuit(connfd);
        authed.erase(connfd);
    }
}

}//namespace tinySocketd
