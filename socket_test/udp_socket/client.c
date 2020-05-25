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
    int sockfd, ret;
    char buf[BUFSIZ];

    struct sockaddr_in serv_addr;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr.s_addr);
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd == -1){
    	sys_err("socket error");
    }

    while(1){
	fgets(buf, sizeof(buf), stdin);
        sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	ret = recvfrom(sockfd, buf, sizeof(buf), 0, NULL, 0);
	
	write(STDOUT_FILENO, buf, ret);
    }

    close(sockfd);

    return 0;
}
