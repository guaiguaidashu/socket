#include "wrap.h"

#define MAXLINE 8192
#define SERV_PORT 8000
#define OPEN_MAX 5000

int main(int argc, char *argv[])
{
    int i, listenfd, connfd, sockfd;
    int n, num = 0;
    ssize_t nready, efd, res;
    char buf[MAXLINE], str[INET_ADDRSTRLEN];
    socklen_t clilen;

    struct sockaddr_in servaddr, cliaddr;
    struct epoll_event tep, ep[OPEN_MAX];	// tep:epoll_ctr参数  ep[]:epoll_wait参数

    listenfd = Socket(AF_INET, SOCK_STREAM, 0);
    int opt =1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    Bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    Listen(listenfd, 20);

    efd = epoll_create(OPEN_MAX);
    if(efd == -1)
	    perr_exit("epoll_create error!");

    tep.events = EPOLLIN;	// 指定 lfd 的监听事件为 读
    tep.data.fd = listenfd;
    res = epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &tep);	//将 lfd 及对应的结构体设置到树上，efd 可找到该树
    if(res == -1)
	    perr_exit("epoll_ctl error!");
    
    for(;;){
        // epoll为server阻塞监听事件，ep为struct epoll_event类型数组，OPEN_MAX为数组容量，-1表示永久阻塞
	nready = epoll_wait(efd, ep, OPEN_MAX, -1);
	if(nready == -1)
		perr_exit("epoll_wait error!");
	for(i = 0; i < nready; i++){
	    if(!(ep[i].events & EPOLLIN))	// 如果不是读事件，继续循环
		    continue;
	    if(ep[i].data.fd == listenfd){	// 判断满足事件的 fd 是不是 lfd
	        clilen = sizeof(cliaddr);
		connfd = Accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);	// 接受连接

		printf("received from %s at PORT %d\n",
			inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)),
			ntohs(cliaddr.sin_port));
		printf("cfd %d ---client %d\n", connfd, ++num);

		tep.events = EPOLLIN;
		tep.data.fd = connfd;
		res = epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &tep);	// 加入红黑树
		if(res == -1)
			perr_exit("epoll_ctl eroor!");
	    } else{
	        sockfd = ep[i].data.fd;
		n = Read(sockfd, buf, MAXLINE);

		if(n == 0){	// 读到 0 说明客户端关闭连接
		    res = epoll_ctl(efd, EPOLL_CTL_DEL, sockfd, NULL);	// 将该文件描述符从红黑树删除
		    if(res == -1)
			    perr_exit("epoll_ctl error!");
		    Close(sockfd);
		    printf("client[%d] closed connection\n", sockfd);
		} else if(n < 0){	// 出错
		    perror("read n < 0 error:");
		    res = epoll_ctl(efd, EPOLL_CTL_DEL, sockfd,NULL);	// 删除节点
		    Close(sockfd);
		} else{
		    for(i = 0; i < n; i++)
			    buf[i] = toupper(buf[i]);
		    Write(STDOUT_FILENO, buf, n);
		    Write(sockfd, buf, n);
		}
	    }
	}
    }

    Close(listenfd);

    return 0;
}

