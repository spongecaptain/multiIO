#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>

// 初始化服务端的监听端口
// 返回值 -1 意味着初始化失败，否则代表 Socket 的文件描述符
int initserver(int port)
{  // 创建 Server Socket
   int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
   if (serverSocket < 0)
   {
      printf("socket() failed.\n");
      return -1;
   }

   // 配置
   int opt = 1;
   unsigned int len = sizeof(opt);
   setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, len);
   setsockopt(serverSocket, SOL_SOCKET, SO_KEEPALIVE, &opt, len);
   // 监听配置
   struct sockaddr_in servaddr;
   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
   servaddr.sin_port = htons(port);
   // 端口绑定
   if (bind(serverSocket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
   {
      printf("bind() failed.\n");
      close(serverSocket);
      return -1;
   }
   // 开始监听
   int backlog = 5;
   if (listen(serverSocket, backlog) != 0)
   {
      printf("listen() failed.\n");
      close(serverSocket);
      return -1;
   }

   return serverSocket;
}