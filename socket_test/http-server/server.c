#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include "wrap.h"

#define MAXSIZE 2048

void encode_str(char* to, int tosize, const char* from);
void decode_str(char *to, char *from);
int hexit(char c);
int get_line(int cfd, char *buf, int size);
const char *get_file_type(const char *name);
void http_request(const char *request, int cfd);
void send_dir(int cfd, const char* dirname);
void send_respond_head(int cfd, int no, char *disp, const char *type, int len);
int init_listen_fd(int port, int epfd);
void disconnect(int cfd, int epfd);
void do_accept(int lfd, int epfd);
void send_file(int cfd, const char *file);
void send_error(int cfd, int status, char *title, char *text);
void *epoll_run(int port);

void send_error(int cfd, int status, char *title, char *text)
{
    char buf[4096] = {0};

    sprintf(buf,"%s %d %s\r\n", "HTTP/1.1", status, title);;
    sprintf(buf+strlen(buf), "Content-Type:%s\r\n", "text/html");
    sprintf(buf+strlen(buf), "Content-Length:%d\r\n", -1);
    sprintf(buf+strlen(buf), "Connection: close\r\n");
    send(cfd, buf, strlen(buf), 0);
    send(cfd, "\r\n", 2, 0);

    memset(buf, 0, sizeof(buf));

    sprintf(buf, "<html><head><title>%d %s</title></head>\n", status, title);
    sprintf(buf+strlen(buf), "<body bgcolor=\"#cc99cc\"><h4 align=\"center\">%d %s</h4>\n", status, title);
    sprintf(buf+strlen(buf), "%s\n", text);
    sprintf(buf+strlen(buf), "<hr>\n</body>\n</html>\n");
    send(cfd, buf, strlen(buf), 0);

    return ;
}

int get_line(int cfd, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while((i < size-1) && (c != '\n')){
        n = recv(cfd, &c, 1, 0);
	if(n > 0){
	    if(c == '\r'){
	        n = recv(cfd, &c, 1, MSG_PEEK);
		if((n > 0) && (c == '\n')){
		    recv(cfd, &c, 1, 0);
		} else{
		    c = '\n';
		}
	    }
	    buf[i] = c;
	    i++;
	} else{
	    c = '\n';
	}
    }
    buf[i] = '\0';
    if(-1 == n)
	    i = n;
    return i;
}

// 通过文件名获取文件类型
const char *get_file_type(const char *name)
{
    char* dot;
    // 自右向左查找'.'字符，如果不存在返回NULL
    dot = strrchr(name, '.');
    if(dot == NULL)
	    return "text/plain; charset=utf-8";
    if(strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
	    return "text/html; charset=utf-8";
    if(strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
	    return "image/jpeg";
    if(strcmp(dot, "gif") == 0)
	    return "image/gif";
    if(strcmp(dot, ".png") == 0)
	    return "image/png";
    if(strcmp(dot, ".css") == 0)
	    return "text/css";
    if(strcmp(dot, ".au") == 0)
	    return "audio/basic";
    if(strcmp(dot, ".wav") == 0)
	    return "audio/wav";
    if(strcmp(dot, ".avi") == 0)
	    return "video/x-msvideo";
    if(strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
	    return "video/quicktime";
    if(strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
	    return "video/mpeg";
    if(strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
	    return "model/vrml";
    if(strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
	    return "audio/midi";
    if(strcmp(dot, ".mp3") == 0)
	    return "audio/mpeg";
    if(strcmp(dot, ".ogg") == 0)
	    return "application/ogg";
    if(strcmp(dot, ".pac") == 0)
	    return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}


// 客户端的fd，错误号，错误秒数，回发文件类型，文件长度
void send_respond_head(int cfd, int no, char *disp, const char *type, int len)
{
	
    char buf[1024] = {0};
    // 状态行
    sprintf(buf, "HTTP/1.1 %d %s\r\n", no, disp);
    send(cfd, buf, strlen(buf), 0);
    // 消息报头
    sprintf(buf, "Content-Type:%s\r\n", type);
    sprintf(buf+strlen(buf), "Content-Length:%d\r\n", len);
    send(cfd, buf, strlen(buf), 0);
    // 空行
    send(cfd, "\r\n", 2, 0);
    
}

// 发送服务器本地文件 给浏览器
void send_file(int cfd, const char *file)
{
    int n = 0, ret;
    char buf[1024];
    // 打开的服务器本地文件 --- cfd能访问客户端的socket
    int fd = open(file, O_RDONLY);
    if(fd == -1){
	// 404 错误页面
        perror("open error");
	exit(1);
    }
    while((n = read(fd, buf, sizeof(buf))) > 0){
        ret = send(cfd, buf, n, 0);
	if(ret == -1){
	    printf("error = %d\n", errno);
	    if(errno == EAGAIN){
	        printf("----EAGAIN\n");
		continue;
	    } else if(errno == EINTR){
	        printf("----EINTR\n");
		continue;
	    } else{
	        perror("send error");
		exit(1);
	    }
	}
	if(ret < 4096);
	    printf("----send ret: %d\n", ret);
    }
    close(fd);
}

// 处理http请求，判断文件是否存在，回发
void http_request(const char *request, int cfd)
{
    // 拆分http请求行
    char method[12], path[1024],protocol[12];
    sscanf(request, "%[^ ] %[^ ] %[^ ]", method, path, protocol);
    printf("method=%s, path=%s, protocol=%s\n", method, path, protocol);
    // 转码 将不能 识别的中文乱码 -- 转中文
    // 解码 %23 %34 %5f
    decode_str(path, path);
    char* file = path + 1;  // 去掉path中的/ 获取访问文件名

    //如果没有指定访问的资源，默认显示资源目录中的内容
    if(strcmp(path, "/") == 0){
        // file的值，资源目录的当前位置
	file = "./";
    }
    
    // 获取文件属性
    struct stat st;
    // 判断文件是否存在
    int ret = stat(file, &st);
    if(ret != 0){
        // 回发浏览器 404 错误页面
	send_error(cfd, 404, "Not found", "NO such a file or directory.");
	return ;
    }
    // 判断是目录还是文件
    if(S_ISDIR(st.st_mode)){
        // 发送头信息
	send_respond_head(cfd, 200, "OK", get_file_type(".html"), -1);
	// 发送目录信息
	send_dir(cfd, file);
    } else if(S_ISREG(st.st_mode)){
        // 回发 http 协议应答
	send_respond_head(cfd, 200, "OK", get_file_type(file), st.st_size);
	// send_respond_head(cfd, 200, "OK", "Content-Type: text/plain; charset=iso-8859-1", st.st_size);
	   
	// 发送文件内容
	send_file(cfd, file);
    }
}


int hexit(char c)
{
    if( c >= '0' && c <= '9')
	    return c - '0';
    if( c >= 'a' && c <= 'f')
	    return c - 'a' + 10;
    if( c >= 'A' && c <= 'F')
	    return c - 'A' + 10;
    return 0;
}

void encode_str(char* to, int tosize, const char* from)
{
    int tolen;
    for(tolen=0; *from!='\0' && tolen+4 < tosize; ++from){
        if(isalnum(*from) || strchr("/_.-~", *from) != (char*)0){
	    *to = *from;
	    ++to;
	    ++tolen;
	} else{
	    sprintf(to, "%%%02x", (int) *from & 0xff);
	    to += 3;
	    tolen += 3;
	}
    }
    *to = '\0';
}

void decode_str(char *to, char *from)
{
    for(; *from != '\0'; ++to, ++from){
        if(from[0] == '%' && isxdigit(from[1] && isxdigit(from[2]))){
	    *to = hexit(from[1]*16 + hexit(from[2]));
	    from += 2;
	} else {
	    *to = *from; 
	}
    }
    *to = '\0';
}

// 发送目录内容
void send_dir(int cfd, const char* dirname)
{
    int i, ret;
    // 拼一个HTML页面<table></table>
    char buf[4094] = {0};
    sprintf(buf, "<html><head><title>目录吗: %s</title></head>", dirname);
    sprintf(buf+strlen(buf), "<body><h1>当前目录: %s</h1><table>", dirname);
    
    char enstr[1024] = {0};
    char path[1024] = {0};
    // 目录项二级指针
    struct dirent** ptr;
    int num = scandir(dirname, &ptr, NULL, alphasort);

    for(i = 0; i < num; i++){
        char* name = ptr[i]->d_name;
	// 拼接文件的完整路径
	sprintf(path, "%s/%s", dirname, name);
	printf("path = %s===================\n",path);
	struct stat st;
	stat(path, &st);
	// 编码生成 %%%
	encode_str(enstr, sizeof(enstr), name);
	// 如果是文件
	if(S_ISREG(st.st_mode)){
	    sprintf(buf+strlen(buf),
		    "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
		    enstr, name, (long)st.st_size);
	} else if(S_ISDIR(st.st_mode)){
	    sprintf(buf+strlen(buf),
		    "<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
		    enstr, name, (long)st.st_size);
	}
	ret = send(cfd, buf, strlen(buf), 0);
	if(ret == -1){
	    if(errno == EAGAIN){
	        perror("send error:");
		continue;
	    } else if(errno == EINTR){
	        perror("send error:");
		continue;
	    } else{
	        perror("send error:");
		exit(1);
	    }
	}
	memset(buf, 0, sizeof(buf));
    }
    sprintf(buf+strlen(buf), "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);
    printf("dir message send OK !!!\n");
}

int init_listen_fd(int port, int epfd)
{
    // 创建监听的套接字 lfd
    struct sockaddr_in srv_addr;
    int lfd = Socket(AF_INET, SOCK_STREAM, 0);

    int opt =1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero(&srv_addr, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int ret = Bind(lfd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
    ret = Listen(lfd, 128);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;

    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if(ret == -1){
        perror("epoll_ctl add lfd error");
	exit(1);
    }
    return lfd;
}

void do_accept(int lfd, int epfd)
{
    struct sockaddr_in clt_addr;
    socklen_t clt_addr_len = sizeof(clt_addr);
    int cfd = accept(lfd, (struct sockaddr*)&clt_addr, &clt_addr_len);
    if(cfd == -1){
        perror("accept error");
	exit(1);
    }
    // 打印客户端IP+port
    char client_ip[64] = {0};
    printf("New Client IP: %s, Port: %d, cfd = %d\n",
	    inet_ntop(AF_INET, &clt_addr.sin_addr.s_addr, client_ip,
	    sizeof(client_ip)), ntohs(clt_addr.sin_port), cfd);
    // 设置 cfd 非阻塞
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);
    // 将新节点cfd挂载到epoll监听树上
    struct epoll_event ev;
    ev.data.fd = cfd;
    // 边沿非阻塞模式
    ev.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
    if(ret == -1){
        perror("epoll_ctl add cfd error");
	exit(1);
    }
}

void disconnect(int cfd, int epfd)
{
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    if(ret != 0){
        perror("epoll_ctl error!");
	exit(1);
    }
    close(cfd);
}

void do_read(int cfd, int epfd)
{
    // 读取一行http协议，拆分，获取get文件名协议号
    char line[1024] = {0};
    int len = get_line(cfd, line, sizeof(line));
    if(len == 0){
        printf("服务器检查到客户端关闭...\n");
	disconnect(cfd, epfd);
    } else{
	printf("============ 请求头 ==========\n");
	printf("请求行数据: %s", line);
	// 还要数据没读完，继续读走
	while(1){
	    char buf[1024] = {0};
	    len = get_line(cfd, buf, sizeof(buf));
	    if(buf[0] == '\n'){
	        break;
	    } else if(len == -1){
	        break;
	    }
	}
	printf("=========== The End ==========\n");
	if(strncasecmp("get", line, 3) == 0){
	    http_request(line, cfd);
	    disconnect(cfd, epfd);
	}
    }
}

void *epoll_run(int port)
{
    int i = 0;
    struct epoll_event all_events[MAXSIZE];
    // 创建一个epoll监听树根
    int epfd = epoll_create(MAXSIZE);
    if(epfd == -1){
        perror("epoll_create error");
	exit(1);
    }
    // 创建lfd，并添加至监听数
    int lfd = init_listen_fd(port, epfd);
    while(1){
        // 监听节点对应事件
	int ret = epoll_wait(epfd, all_events, MAXSIZE, -1);
	if(ret == -1){
	    perror("epoll_wait error");
	    exit(1);
	}
	for(i = 0; i < ret; i++){
	    // 只处理读事件，其他事件默认不处理
	    struct epoll_event *pev = &all_events[i];
	    // 不是读事件
	    if(!(pev->events & EPOLLIN)){
	        continue;
	    }
	    if(pev->data.fd == lfd){
	        do_accept(lfd, epfd);
	    } else{
	        do_read(pev->data.fd, epfd);
	    }
	}
    }
}


int main(int argc, char *argv[]){
    // 命令行参数获取 端口和server提供的目录
    if(argc < 3){
        printf("./server port path\n");
    }
    // 获取用户输入的端口
    int port = atoi(argv[1]);
    // 改变进程工作目录
    int ret = chdir(argv[2]);
    if(ret != 0){
        perror("chdir error");
	exit(1);
    }
    // 启动epoll监听
    epoll_run(port);
    
    return 0;
}
