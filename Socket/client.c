#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

//添加网络socket 相关的头文件 
//这里 arpa/inet.h 内部包含了 socket.h 
#include <arpa/inet.h>


int main()
{ 
    //1.创建用于通信的套接字
    int fd = socket(AF_INET,SOCK_STREAM,0);
    if(fd == -1)
    {
        perror("socket");
        return -1;
    }

    //2. 连接到服务器的 IP : Port
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    // 这里端口号要转化为大段,方便进行网络通信
    //所以这里要补一个 h to n ,又因为 ipv4 的 ip 地址 32位 , 所以用 htons
    saddr.sin_port = htons(9999); // 5000 以下大部分被使用了

    //这里客户端需要绑定到一个实际的ip的地址(就是服务器的ip地址)才能进行通信
    // 本机ip地址 "192.168.232.129" 是小端存储,需要转为大端 , 然后保存到 saddr.sin_addr.s_addr 中
    inet_pton(AF_INET,"192.168.0.103",&saddr.sin_addr.s_addr);

    int ret = connect(fd,(struct sockaddr*)&saddr, sizeof(saddr));
    
    if (ret == -1)
    {
        perror("connect");
        return -1;
    }


    int number = 0;
    //3.套接字的通信 
    while (1)
    {
        //创建一个 buffer ,将要发送的数据放到 buff  中 
        char buff[1024];
        sprintf(buff,"你好, hello world ,  %d....\n" , number);
        send(fd,buff,strlen(buff)+1,0); //+1 是为了包含字符串最后的 \0
        
        //接受数据
        //清空buff中的数据
        memset(buff , 0 , sizeof(buff));
        int len = recv(fd,buff,sizeof(buff),0);
        if(len >0)
        {
            printf("Server say : %s \n",buff);

        }
        else if (len ==0)
        {
            printf("服务器已断开连接...\n");
            break;
        }
        else
        {
            perror("recv");
        }
        number ++;
        sleep(1);// 客户端每隔一秒发送一个数据
    }

    //关闭文件描述符
    close (fd);

    return 0;
}

