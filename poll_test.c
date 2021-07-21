#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>

// 最大值为 ulimit -n，但应当取决于应用需求
#define MAXNFDS 1024
#define PORT 5110
// 初始化服务端的监听端口
int initserver(int port);

int main(int argc, char *argv[])
{
      // 初始化服务端用于监听的 socket
      int listensock = initserver(PORT);
      printf("listensock=%d\n", listensock);

      if (listensock < 0)
      {
            printf("initserver() failed.\n");
            return -1;
      }

      int maxfd;                  // fds 数组中需要监视的 socket 的大小
      struct pollfd fds[MAXNFDS]; // fds pollfd 数组，每一个元素引用了 Socket 文件描述符

      for (int ii = 0; ii < MAXNFDS; ii++)
            fds[ii].fd = -1; // 初始化数组，把全部的 fd 设置为 -1，-1 表示没有文件描述符引用，poll 会忽略这些 pollfd 元素（可以通过 man poll 命令来理解这里的初始化语义）

      // 把 listensock 添加到数组中（数组下标直接采用文件描述符编号即可）
      fds[listensock].fd = listensock; // listensock 在运行模式下通常为 3
      fds[listensock].events = POLLIN; // 有数据可读事件，包括新客户端的连接、客户端 socket 有数据可读和客户端 socket 断开三种情况
      maxfd = listensock;              // 一个高水位，用于表示 fds 有效的元素，这个值会通知 poll 函数

      while (1)
      { // 与 select 的一个区别是不需要创建临时文件描述符 fds
            // poll 每次调用时需要在用户态与内核态之间拷贝 fds，即 pollfd 结构体数组，这是效率比较差的地方
            int infds = poll(fds, maxfd + 1, 5000);
            // 返回失败
            if (infds < 0)
            {
                  printf("poll() failed.\n");
                  perror("poll():");
                  break;
            }
            // 超时
            if (infds == 0)
            {
                  printf("poll() timeout.\n");
                  continue;
            }
            // poll 同 select 一样需要进行 O(N) 时间复杂度的遍历，效率类似
            for (int eventfd = 0; eventfd <= maxfd; eventfd++)
            { // 没有对应的 fd，说明这个 pollfd 并没有关联一个文件描述符
                  if (fds[eventfd].fd < 0)
                        continue;
                  // 没有发生可读事件，跳过
                  if ((fds[eventfd].revents & POLLIN) == 0)
                        continue;
                  // 发生了可读事件，先把当前事件标志位清空
                  fds[eventfd].revents = 0;
                  // 处理 Server Socket 的事件
                  if (eventfd == listensock)
                  {
                        // 如果发生事件的是 listensock，表示有新的客户端连上来，进行 accept，然后将新 Socket 注册到 fds 上
                        struct sockaddr_in client;
                        socklen_t len = sizeof(client);
                        // 1. accept
                        int clientsock = accept(listensock, (struct sockaddr *)&client, &len);
                        if (clientsock < 0)
                        {
                              printf("accept() failed.\n");
                              continue;
                        }

                        printf("client(socket=%d) connected ok.\n", clientsock);
                        // 2. 确保没有超过 poll 的最大 pollfd 数量限制
                        if (clientsock > MAXNFDS)
                        {
                              printf("clientsock(%d)>MAXNFDS(%d)\n", clientsock, MAXNFDS);
                              close(clientsock);
                              continue;
                        }
                        // 3. 将新 Socket 注册到 fds 中
                        fds[clientsock].fd = clientsock;
                        fds[clientsock].events = POLLIN;
                        fds[clientsock].revents = 0;
                        if (maxfd < clientsock)
                              maxfd = clientsock;

                        printf("maxfd=%d\n", maxfd);
                        continue;
                  }
                  else
                  {
                        // 客户端有数据过来或客户端的 socket 连接被断开
                        char buffer[1024];
                        memset(buffer, 0, sizeof(buffer));

                        // 读取客户端的数据
                        ssize_t isize = read(eventfd, buffer, sizeof(buffer));

                        // 发生了错误或 socket 被对方关闭
                        if (isize <= 0)
                        {
                              printf("client(eventfd=%d) disconnected.\n", eventfd);

                              close(eventfd); // 关闭客户端的 socket
                              // 通过 fd 置为 -1 来取消此 pollfd 对此 Socket 文件描述符的引用是很有必要的
                              fds[eventfd].fd = -1;

                              // 重新计算 maxfd 的值，注意，只有当 eventfd==maxfd 时才需要计算
                              if (eventfd == maxfd)
                              {
                                    for (int ii = maxfd; ii > 0; ii--)
                                    {
                                          if (fds[ii].fd != -1)
                                          {
                                                maxfd = ii;
                                                break;
                                          }
                                    }

                                    printf("maxfd=%d\n", maxfd);
                              }

                              continue;
                        }

                        printf("recv(eventfd=%d,size=%ld):%s\n", eventfd, isize, buffer);

                        // 把收到的报文发回给客户端。
                        write(eventfd, buffer, strlen(buffer));
                  }
            }
      }

      return 0;
}

// 初始化服务端的监听端口
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