#include <pthread.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <strings.h>
#include <sys/wait.h>

#include "wrap.h"

#define MAXLINE 8192
#define SERV_PORT 8000

struct s_info{
    struct sockaddr_in cliaddr;
    int connfd;
};

void *do_work(void *arg)
{
    int n, i;
    struct s_info *ts = (struct s_info*)arg;
    char buf[MAXLINE];
    char str[INET_ADDRSTRLEN];

    while(1){
        n = Read(ts->connfd, buf, MAXLINE);	// 读客户端
	if(n == 0){
	    printf("the client %d closed...\n", ts->connfd);
	    break;	// 跳出循环，关闭cfd
	}
	printf("received from %s at PORT %d\n",
		inet_ntop(AF_INET, &(*ts).cliaddr.sin_addr, str, sizeof(str)),
		ntohs((*ts).cliaddr.sin_port));

	for(i = 0; i < n; i++)
		buf[i] = toupper(buf[i]);

	Write(STDOUT_FILENO, buf, n);
	Write(ts->connfd, buf, n);
    }
    Close(ts->connfd);

    return (void *)0;
}

int main(void)
{
    struct sockaddr_in servaddr, cliaddr;
    socklen_t cliaddr_len;
    int listenfd, connfd;
    pthread_t tid;

    struct s_info ts[256];	// 创建结构体数组
    int i = 0;

    listenfd = Socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));    // 地址结构清零

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);    // 指定端口号
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);    // 指定本地任意IP

    Bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));    //绑定

    Listen(listenfd, 128);

    printf("Acceptint client connect ...\n");

    while(1){
	cliaddr_len = sizeof(cliaddr);
        connfd = Accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);
	ts[i].cliaddr = cliaddr;
	ts[i].connfd = connfd;

	pthread_create(&tid, NULL, do_work, (void*)&ts[i]);
	pthread_detach(tid);	// 子线程分离，防止僵尸线程产生
	i++;
    }

    return 0;
}

