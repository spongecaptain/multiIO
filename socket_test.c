#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SERVER_PORT 5110

// gcc -o server socket_test.c
// ./server
// telnet 127.0.0.1 5110
int main(int argc, char *argv[])
{
      // 1. 创建服务端 socket
      int listenfd;
      listenfd = socket(AF_INET, SOCK_STREAM, 0);
      if (listenfd == -1)
      {
            perror("socket");
            return -1;
      }

      // 2. server socket 绑定到本地指定端口
      struct sockaddr_in servaddr; // 服务端地址信息的数据结构
      memset(&servaddr, 0, sizeof(servaddr));
      servaddr.sin_family = AF_INET;                // 协议族，在 socket 编程中只能是 AF_INET
      servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // 任意 ip 地址
      servaddr.sin_port = htons(SERVER_PORT);       // 指定通信端口
      if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
      {
            perror("bind");
            close(listenfd);
            return -1;
      }

      // 3. 把 socket 设置为监听模式
      if (listen(listenfd, 5) != 0)
      {
            perror("listen");
            close(listenfd);
            return -1;
      }

      // 4. Server Socket 开始监听指定端口
      // 这里也可以选择新起一个线程来处客户端 Socket 的数据
      while (1)
      {
            int clientfd;                             // 客户端的socket。
            int socklen = sizeof(struct sockaddr_in); // struct sockaddr_in的大小
            struct sockaddr_in clientaddr;            // 客户端的地址信息
            clientfd = accept(listenfd, (struct sockaddr *)&clientaddr, (socklen_t *)&socklen);
            printf("客户端（%s）已连接\n", inet_ntoa(clientaddr.sin_addr));

            // 5. 与客户端通信，接收客户端发过来的报文后，echo 回复
            char buffer[1024];
            while (1)
            {
                  int iret;
                  memset(buffer, 0, sizeof(buffer));
                  if ((iret = recv(clientfd, buffer, sizeof(buffer), 0)) <= 0) // 接收客户端的请求报文
                  {
                        printf("iret=%d\n", iret);
                        break;
                  }
                  printf("接收：%s", buffer);
                  if ((iret = send(clientfd, buffer, strlen(buffer), 0)) <= 0) // 向客户端发送响应结果
                  {
                        perror("send");
                        break;
                  }
                  printf("发送：%s", buffer);
            }

            // 6. close client socket
            close(clientfd);
      }
      // 7. close server socket
      close(listenfd);
}