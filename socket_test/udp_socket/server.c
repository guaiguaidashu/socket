#include "wrap.h"

#define SERV_PORT 9527

int main(void){
    int sockfd;
    int ret;
    char buf[BUFSIZ], client_IP[1024];

    struct sockaddr_in serv_addr, clit_addr;
    socklen_t clit_addr_len;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

    bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    clit_addr_len = sizeof(clit_addr);

    while(1){
    	ret = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&clit_addr, &clit_addr_len);
	write(STDOUT_FILENO, buf, ret);
        printf("client ip:%s port:%d\n",
		inet_ntop(AF_INET, &clit_addr.sin_addr, client_IP, sizeof(client_IP)),
                ntohs(clit_addr.sin_port));

	for(int i = 0; i < ret; i++)
		buf[i] = toupper(buf[i]);

	sendto(sockfd, buf, ret, 0,(struct sockaddr *)&clit_addr, clit_addr_len);

    }

    close(sockfd);

    return 0;

}












 








