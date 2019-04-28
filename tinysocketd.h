#ifndef _TINY_SOCKETD_
#define _TINY_SOCKETD_
#include <map>
#include <set>
#include <list>
#include <atomic>
#include <sys/epoll.h>
#include <memory.h>
#include <string>
#include <stdio.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/file.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <mutex>
namespace tinySocketd{
    class msg_base{
        public:
            static void setnonblocking(int sock);
            static int startup(u_short *port);
            std::atomic<bool> running;
            int epfd;
            inline void stop(){
                running=false;
            }
            void run(u_short p,int maxnum=256);
            virtual void onMessage(int,char*,int)=0;
            virtual void onConnect(int)=0;
            virtual void onQuit(int)=0;
            virtual void onWriAble(int)=0;
            virtual void destruct()=0;
            virtual void loop()=0;
    };
    class msg:public msg_base{
        private:
            struct conn{
                msg * parent;
                char     buf[256];
                uint16_t ptr;
                int      fd;
                void reset();
                void append(char c);
                void quit();
                void init(int ifd,msg * ptr);
            };
            std::map<int,conn> conns;
        public:
            virtual void onConnect(int ifd);
            virtual void onQuit(int connfd);
            virtual void onMessage(int connfd,const char * fst,int len);
            
            virtual void onFDConnect(int connfd)=0;
            virtual void onFDMessage(int connfd,const char * fst,int len)=0;
            virtual void onFDQuit(int connfd)=0;
    };
    class auth:public msg{
        private:
            std::set<int> authed;
        public:
            virtual void onFDConnect(int connfd);
            virtual void onFDMessage(int connfd,const char * fst,int len);
            virtual void onFDQuit(int connfd);
            
            virtual bool onAuth(int connfd,const char * fst,int len)=0;
            virtual void onAuthedConnect(int connfd)=0;
            virtual void onAuthedMessage(int connfd,const char * fst,int len)=0;
            virtual void onAuthedQuit(int connfd)=0;
    };
}
#endif