#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
// 注意事项：epoll 不同于 select 以及 poll 在 Linux 与 Mac 上都有，epoll 并不能在 Mac 等系统上编译通过
// MAXEVENTS 配置取决于具体应用的需求，但通常不会很大，100 左右
#define MAXEVENTS 128
#define PORT 5110
// 把 socket 设置为非阻塞的方式
int setnonblocking(int sockfd);

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
      int epollfd;                       // epoll_create 函数返回的文件描述符
      char buffer[1024];                 // 读写操作共用的 char[] 数组
      memset(buffer, 0, sizeof(buffer)); // 将 buffer 初始化

      // 创建一个 epoll 文件描述符，区别：select 以及 poll 都不会返回一个文件描述符，输入参数为一个正数即可
      epollfd = epoll_create(1);

      // 添加监听描述符事件
      // 创建事件
      struct epoll_event ev;
      // 建立事件与文件描述符之间的联系
      ev.data.fd = listensock;
      // 确定感兴趣的事件类型
      ev.events = EPOLLIN;
      // 将文件描述符以及事件（事件有文件描述符的引用，反之则不然）
      epoll_ctl(epollfd, EPOLL_CTL_ADD, listensock, &ev);

      while (1)
      {
            struct epoll_event events[MAXEVENTS]; // 存放有事件发生的结构数组
            // 等待监视的 socket 有事件发生
            // epoll_wait 函数返回时会对 epoll_event 结构体数组产生副作用
            int infds = epoll_wait(epollfd, events, MAXEVENTS, -1);
            // <0  表示出现致命错误
            if (infds < 0)
            {
                  printf("epoll_wait() failed.\n");
                  perror("epoll_wait()");
                  break;
            }
            // =0 表示超时
            if (infds == 0)
            {
                  printf("epoll_wait() timeout.\n");
                  continue;
            }
            // 遍历有事件发生的结构数组
            int j;
            for (j = 0; j < infds; j++)
            {
                  // epoll 机制相对于 select/poll 机制的一大特点，能够通过事件直接找到文件描述符
                  if ((events[j].data.fd == listensock) && (events[j].events & EPOLLIN))
                  {
                        // 如果发生事件的是 listensock，表示有新的客户端连上来
                        struct sockaddr_in client;
                        socklen_t len = sizeof(client);
                        int clientsock = accept(listensock, (struct sockaddr *)&client, &len);
                        if (clientsock < 0)
                        {
                              printf("accept() failed.\n");
                              continue;
                        }

                        // 把新的客户端添加到 epoll 中
                        memset(&ev, 0, sizeof(struct epoll_event));
                        ev.data.fd = clientsock;
                        ev.events = EPOLLIN;
                        epoll_ctl(epollfd, EPOLL_CTL_ADD, clientsock, &ev);
                        printf("client(socket=%d) connected ok.\n", clientsock);
                        continue;
                  }
                  else if (events[j].events & EPOLLIN)
                  {
                        // 客户端有数据过来或客户端的 socket 连接被断开
                        char buffer[1024];
                        memset(buffer, 0, sizeof(buffer));

                        // 读取客户端的数据，一次最多读取 1024 字节
                        ssize_t isize = read(events[j].data.fd, buffer, sizeof(buffer));

                        // 发生了错误或 socket 被对方关闭
                        if (isize <= 0)
                        {
                              printf("client(eventfd=%d) disconnected.\n", events[j].data.fd);
                              // 把已断开的客户端从 epoll 中删除
                              memset(&ev, 0, sizeof(struct epoll_event));
                              ev.events = EPOLLIN;
                              ev.data.fd = events[j].data.fd;
                              epoll_ctl(epollfd, EPOLL_CTL_DEL, events[j].data.fd, &ev);
                              close(events[j].data.fd);
                              continue;
                        }
                        printf("recv(eventfd=%d,size=%d):%s\n", events[j].data.fd, isize, buffer);
                        // 把收到的报文发回给客户端
                        // 通过事件就能够找到对应的文件描述符，这是 epoll 与 select、poll 的区别之一
                        write(events[j].data.fd, buffer, strlen(buffer));
                  }
            }
      }

      close(epollfd);

      return 0;
}