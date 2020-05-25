#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#define SERV_PORT 9527

void sys_err(const char *str){
    perror(str);
    exit(1);
}

int main(int argc, char *argv[]){
    int cfd;
    char buf[BUFSIZ];

    struct sockaddr_in serv_addr;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr.s_addr);
    
    cfd = socket(AF_INET, SOCK_STREAM, 0);
    if(cfd == -1){
    	sys_err("socket error");
    }

    int ret = connect(cfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if(ret != 0)
	sys_err("connect error");

    while(1){
	fgets(buf, sizeof(buf), stdin);
        write(cfd, buf, strlen(buf));
	ret = read(cfd, buf, sizeof(buf));
	write(STDOUT_FILENO, buf, ret);
    }

    close(cfd);

    return 0;
}
