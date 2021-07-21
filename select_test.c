#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>

#define SERVER_PORT 5110

// 初始化服务端的监听端口。
int initserver(int port);

int main(int argc, char *argv[])
{
   // 初始化服务端用于监听的socket。
   int listensock = initserver(SERVER_PORT);
   printf("listensock=%d\n", listensock);

   if (listensock < 0)
   {
      printf("initserver() failed.\n");
      return -1;
   }

   fd_set readfdset; // 读事件的集合，包括监听socket和客户端连接上来的socket。
   int maxfd;        // readfdset中socket的最大值。

   // 初始化结构体，把listensock添加到集合中。
   FD_ZERO(&readfdset);

   FD_SET(listensock, &readfdset);
   maxfd = listensock;

   while (1)
   {
      // 调用 select 函数时，会改变 socket 集合的内容，所以要把 socket 集合保存下来，传一个临时的给 select
      fd_set tmpfdset = readfdset;

      int infds = select(maxfd + 1, &tmpfdset, NULL, NULL, NULL);
      // printf("select infds=%d\n",infds);

      // 返回失败。
      if (infds < 0)
      {
         printf("select() failed.\n");
         perror("select()");
         break;
      }

      // 超时，在本程序中，select 函数最后一个参数为空，不存在超时的情况，但以下代码还是留着
      if (infds == 0)
      {
         printf("select() timeout.\n");
         continue;
      }

      // 检查有事情发生的 socket，包括监听和客户端连接的 socket
      // 这里是客户端的 socket 事件，每次都要遍历整个集合，因为可能有多个 socket 有事件
      int eventfd;
      // 下面的 for 循环涉及文件描述符从内核态拷贝到用户态
      for (eventfd = 0; eventfd <= maxfd; eventfd++)
      {
         if (FD_ISSET(eventfd, &tmpfdset) <= 0)
            continue;

         if (eventfd == listensock)
         {
            // 如果发生事件的是listensock，表示有新的客户端连上来
            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            int clientsock = accept(listensock, (struct sockaddr *)&client, &len);
            if (clientsock < 0)
            {
               printf("accept() failed.\n");
               continue;
            }

            printf("client(socket=%d) connected ok.\n", clientsock);

            // 把新的客户端 socket 加入集合，注意这里向 readfdset 加入，而不是向临时的 tmpfdset 加入
            FD_SET(clientsock, &readfdset);

            if (maxfd < clientsock)
               maxfd = clientsock;

            continue;
         }
         else
         {
            // 客户端有数据过来或客户端的 socket 连接被断开
            char buffer[1024];
            memset(buffer, 0, sizeof(buffer));

            // 读取客户端的数据：<0 表示读取错误，=0 表示连接已经断开，>0 表示正常读取到的字节数
            ssize_t isize = read(eventfd, buffer, sizeof(buffer));

            // 发生了错误或 socket 被对方关闭
            if (isize <= 0)
            {
               printf("client(eventfd=%d) disconnected.\n", eventfd);

               close(eventfd); // 关闭客户端的socket。

               FD_CLR(eventfd, &readfdset); // 从集合中移去客户端的 socket

               // 重新计算 maxfd 的值，注意，只有当 eventfd==maxfd 时才需要计算
               if (eventfd == maxfd)
               {
                  int ii;
                  for (ii = maxfd; ii > 0; ii--)
                  {
                     if (FD_ISSET(ii, &readfdset))
                     {
                        maxfd = ii;
                        break;
                     }
                  }

                  printf("maxfd=%d\n", maxfd);
               }

               continue;
            }

            printf("recv(eventfd=%d,size=%d):%s\n", eventfd, isize, buffer);

            // 把收到的报文发回给客户端。
            write(eventfd, buffer, strlen(buffer));
         }
      }
   }

   return 0;
}

// 初始化服务端的监听端口。
int initserver(int port)
{
   int sock = socket(AF_INET, SOCK_STREAM, 0);
   if (sock < 0)
   {
      printf("socket() failed.\n");
      return -1;
   }

   // Linux如下
   int opt = 1;
   unsigned int len = sizeof(opt);
   setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, len);
   setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &opt, len);

   struct sockaddr_in servaddr;
   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
   servaddr.sin_port = htons(port);

   if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
   {
      printf("bind() failed.\n");
      close(sock);
      return -1;
   }

   if (listen(sock, 5) != 0)
   {
      printf("listen() failed.\n");
      close(sock);
      return -1;
   }

   return sock;
}