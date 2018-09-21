#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

/* --- defaults & definitions --- */
#define HTTP_PORT	8080
#define MAX_CONNECTIONS	128

ssize_t recv_line(int fd, char *buf, size_t len);
int serve(int client,struct sockaddr_in* client_addr);

/* --- globals --- */
int http_port = HTTP_PORT;
int max_connections = MAX_CONNECTIONS;

/* --- error and signal handling --- */
#define error(msg, ...)		fprintf(stderr, "\033[1;41merror:\033[0m" msg, ##__VA_ARGS__)
#define warning(msg, ...)	fprintf(stderr, "\033[1;43mwarning:\033[0m" msg, ##__VA_ARGS__)
#define info(msg, ...)		fprintf(stdout, "\033[1;44minfo:\033[0m" msg, ##__VA_ARGS__)

#define SET_FD 1
#define CLOSE_FD 2
static void handle_fd(int fd, int instr){
	static int f;
	switch(instr){
		case SET_FD:
			f=fd? fd : -1;
		break;
		case CLOSE_FD:
			close(f);
		break;
	}
}
static void abort_program(int signum){
	handle_fd(0, CLOSE_FD);

	switch(signum){
		case SIGABRT: case SIGHUP:
			error("abnormal termination: exit\n");
		break;
		case SIGILL: case SIGSEGV:
			error("invalid instruction: please contact the maintainer\n");
		break;
		case SIGINT:
			info("exit\n");
		break;
		case SIGTERM: case SIGKILL:
			info("termination request: exit\n");
			signum = 0;
		break;
	}

	_exit((signum > 0) ? 1 : 0);
}

/* --- SERVER MAIN --- */
int main(int argc, char *argv[]){
	int fd, client, pid;
	struct sockaddr_in addr, client_addr;
	socklen_t siz;


	printf("\033[1;31mhttpd\033[0m\n--------\n");

	signal(SIGABRT, abort_program); //signal handling
	signal(SIGILL, abort_program);
	signal(SIGINT, abort_program);
	signal(SIGSEGV, abort_program);
	signal(SIGTERM, abort_program);
	signal(SIGCHLD, SIG_IGN);


	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if(fd == -1){
		error("can not create new socket\n");
		abort_program(-1);
	}
	int reuse=1;
	if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))<0) {
		error("setsockopt error\n");
		close(fd);
		abort_program(-1);
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(http_port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))==-1){
		error("bind() failed.\n >Is another server running on this port?\n");
		abort_program(-1);
	}

	if(listen(fd, max_connections)==-1){
		error("listen() failed\n");
		abort_program(-1);
	}

	info("Server running!\n"
	" >host: http://127.0.0.1:%d\n"
	" >\033[1mCtrl-C\033[0m to abort.\n", http_port);
	handle_fd(fd, SET_FD);

	while(1){
		siz = sizeof(struct sockaddr_in);
		client = accept(fd, (struct sockaddr *)&client_addr, &siz);

		if(client==-1){
			error("accept() failed\n");
			continue;
		}

		pid = fork();
		if(pid==-1){
			error("fork() failed\n");
			continue;
		}
		if(pid==0){
			close(fd);
			serve(client,&client_addr);

			shutdown(client, SHUT_RDWR);
			close(client);

			return 0;
		}
		close(client);
	}
	close(fd);

	return 0;
}

ssize_t recv_line(int fd, char *buf, size_t len){
	size_t i=0;
	ssize_t err=1;
	while((i < len-1) && err==1){
		err = recv(fd, &(buf[i]), 1, 0);
		if(buf[i]=='\n'){ break; }
		else{ i++; }
	}
	if(i && (buf[i-1]=='\r')){ i--; }
	buf[i] = '\0';

	return i;
}

int serve(int client,struct sockaddr_in* client_addr){
	char buf[8192]="\0";

	if(recv_line(client, buf, (sizeof(buf)-1) )<=3){
		warning("can not receive request\n");
		return 1;
	}

	while(recv_line(client, buf, (sizeof(buf)-1) ) > 0);

	const char* szIP = inet_ntoa(client_addr->sin_addr);
	long content_length=strlen(szIP);
	long len=sprintf(buf, "HTTP/1.0 200 OK\r\nContent-type: text/plain\r\nContent-length: %ld\r\nServer: httpd\r\n\r\n%s", content_length,szIP);
	send(client, buf, len, 0);
	info("%s query\n",szIP);
	return 0;
}
