#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
        
//包含线程池
#include "threadpool.h"

//使用多线程 实现客户端的并发访问 
#include <pthread.h>

//添加网络socket 相关的头文件 
//这里 arpa/inet.h 内部包含了 socket.h 
#include <arpa/inet.h>

// 定义一个信息结构体,用来传递 cfd 与 caddr 给 working 函数 
struct SockInfo
{
    //每个子线程与不同的客户端进行连接,其对应的 addr 与 fd 不同
    struct sockaddr_in addr;
    int fd;
    /* data */
};

typedef struct PoolInfo
{
    ThreadPool *p; //线程池实例的地址
    int fd;        //文件描述符
}PoolInfo;


//下面定义的两个任务函数,需要和线程池代码中的 一致
//声明子线程的任务函数 , 处理和客户端的通信
void  working(void * arg);

//任务函数 ,用来处理和客户端的连接
void  acceptConnect(void * arg);




int main()
{
    //1.创建监听的套接字
    int fd = socket(AF_INET,SOCK_STREAM,0);
    if(fd == -1)
    {
        perror("socket");
        return -1;
    }

    //2.套接字与 本地ip:端口 进行绑定
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    // 这里端口号要转化为大端,方便进行网络通信
    //所以这里要补一个 h to n ,又因为 ipv4 的 ip 地址 32位 , 所以用 htons
    saddr.sin_port = htons(9999); // 5000 以下大部分被使用了
    saddr.sin_addr.s_addr = INADDR_ANY;   
    //INADDR_ANY 默认为 0.0.0.0 ,不受大小端影响 

    int ret = bind(fd,(struct sockaddr*)&saddr, sizeof(saddr));
    
    if (ret == -1)
    {
        perror("bind");
        return -1;
    }

    //3.设置监听
    ret  = listen(fd,128);
    if (ret == -1)
    {
        perror("listen");
        return -1;
    }

    //创建线程池
    ThreadPool *pool = threadPoolCreate (3,8,100);

    //创建PoolInfo 
    PoolInfo* info = (PoolInfo*)malloc(sizeof(PoolInfo));
    info->p = pool;
    info->fd =fd;


    //向线程池中放任务 
    threadPoolAdd(pool , acceptConnect , info);

    //主线程退出，剩下的交给子线程
    pthread_exit(NULL);
    return 0;
}

void acceptConnect(void * arg)
{
    //将传递进来的参数转为PoolInfo类型
    PoolInfo * poolinfo = (PoolInfo *)arg;
    //从arg 中 获得 SockInfo                                           
    struct SockInfo* pinfo = (struct SockInfo*)arg;

    //4.阻塞并等待客户端的连接 

    int addrlen = sizeof(struct sockaddr_in );  
    while (1)
    {
        struct SockInfo* pinfo; // 结构体的指针         
        //这里不需要数组,直接在堆里面分配内存即可,由线程池完成回收
        pinfo = (struct SockInfo*)malloc(sizeof(struct SockInfo)); 
        //不停地创建子线程
        //cfd 用于通信的文件描述符  ,  fd 用于监听的文件描述符 
        pinfo->fd = accept(poolinfo->fd, (struct sockaddr*)&pinfo->addr,&addrlen);
         
        if (pinfo->fd == -1)
        {
            perror("accept");
            break;
        }

        //这里子线程相关操作由线程池完成,不需要在这里实现子线程的创建了
        //只需要把建立连接后需要处理的各种任务放入线程池就行了
        threadPoolAdd(poolinfo->p,working,pinfo);  
    }

    close(poolinfo->fd); // 关闭监听用的文件描述符 , 这里通信的文件描述符cfd在子线程中被使用,不需要主线程关闭
    
}

void  working(void * arg)
{

    //从arg 中 获得 SockInfo 
    struct SockInfo* pinfo = (struct SockInfo*)arg;

    
    //5.用4中获得的cfd 与客户端进行通信

    //连接建立成功 ,
    //打印保存到caddr内部的信息(打印客户端ip和端口信息)
    //这里从客户端拿到的信息都是大端信息,到本机上还要转换成小端
    char ip[32];
    printf("客户端IP: %s , 端口 : %d\n" , 
            inet_ntop(AF_INET,&pinfo->addr.sin_addr.s_addr , ip, sizeof(ip))
          , ntohs(pinfo->addr.sin_port));


   
    while (1)
    {
        //将接收到的数据放到一个 buffer 中 
        char buff[1024];
        int len = recv(pinfo->fd,&buff,sizeof(buff),0); // 这里len是实际收到数据的长度
        if(len > 0)
        {
            //说明这里接收到客户端传递过来的数据了
            printf("Clint say: %s\n",buff);
            send(pinfo->fd,buff,len,0);
        }
        else if(len ==0)
        {
            //说明客户端已经断开连接了
            printf("客户端已断开连接.....\n");
            break;
        }

        else 
        {
            //最后一种情况, recv 返回值 -1 说明数据接收失败了
            perror("recv");
            break;
        }
    }

    //退出while 循环,说明对应的通信已经结束了,可以之前定义的一系列文件描述符关闭 
    close (pinfo->fd);
    pinfo->fd = -1; // 将数组中保存的 fd 重新改为-1,这样main 中 就会知道这一块区域是空的 
}

