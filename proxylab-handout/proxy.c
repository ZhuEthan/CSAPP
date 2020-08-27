#include <stdio.h>
#include <string.h>
#include <regex.h>
#include "csapp.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void doit(int fd);
void forward_requesthdrs(rio_t *rp, char* uri);
void clienterror(int fd, char*cause, char* errnum, char* shortmsg, char* longmsg);
void parse_uri(char* uri, char* hostname, char* port, char* path);
void *thread(void* vargp);

int main(int argc, char** argv)
{
    printf("%s", user_agent_hdr);
	printf("input (%s, %s)\n", argv[0], argv[1]);

	init_cache();
	int listenfd, *fd_from_client;
	char hostname[MAXLINE], port[MAXLINE];
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	pthread_t tid;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	
	listenfd = Open_listenfd(argv[1]);
	clientlen = sizeof(clientaddr);
	while (1) {
		fd_from_client = Malloc(sizeof(int));
		*fd_from_client = Accept(listenfd, (SA*)&clientaddr, &clientlen);
		Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
		printf("Accepted connection from (%s, %s)\n", hostname, port);
		Pthread_create(&tid, NULL, thread, fd_from_client);
	}

    return 0;
}

void *thread(void *vargp) {
	int fd_from_client = *((int*)vargp);
	Pthread_detach(pthread_self());
	Free(vargp);
	doit(fd_from_client);
	Close(fd_from_client);
	return NULL;
}

void doit(int fd_from_client) {
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	rio_t rio_from_client;

	Rio_readinitb(&rio_from_client, fd_from_client);
	if (!Rio_readlineb(&rio_from_client, buf, MAXLINE))
		return;
	printf("%s", buf);
	sscanf(buf, "%s %s %s", method, uri, version);


	if (strcasecmp(method, "GET")) {
		clienterror(fd_from_client, method, "501", "Not Implemented", "Tiny does not implement this method");
		return;
	}

	acquire_read_lock();
	sbuf_t* t_buf = get_object_by_uri(uri, strlen(uri));
	if ((t_buf) != NULL) {
		printf("==========cache caught==========\n");
		Rio_writen(fd_from_client, t_buf->buf, t_buf->buffer_size);
		release_read_lock();
	} else {
		release_read_lock();
		printf("==========forward started==========\n");
		forward_requesthdrs(&rio_from_client, uri);
	}

	check_cache();
}

void forward_requesthdrs(rio_t* rio_from_client, char* uri) {
	char hostname[MAXLINE], path[MAXLINE], port[MAXLINE];
	parse_uri(uri, hostname, port, path);

	ssize_t n = 0;
	char buf[MAXLINE];
	char* cache_value;
	int fd_to_server = Open_clientfd(hostname, port);
	rio_t rio_to_server;
	
	sprintf(buf, "GET /%s HTTP/1.0\r\n", path);
	printf("%s", buf);
	Rio_writen(fd_to_server, buf, strlen(buf));

	do {
		n = Rio_readlineb(rio_from_client, buf, MAXLINE);
		if (strncmp(buf, "Host:", 5) && strncmp(buf, "User-Agent:", 10) && strncmp(buf, "Connection:", 10) && strncmp(buf, "Proxy-Connection:", 17) && strncmp(buf, "\r\n", 2)) {
			printf("%s", buf);
			Rio_writen(fd_to_server, buf, n);
		}
	} while (strcmp(buf, "\r\n"));

	Rio_readinitb(&rio_to_server, fd_to_server);

	sprintf(buf, "Host: %s\r\n", hostname);
	printf("%s", buf);
	Rio_writen(fd_to_server, buf, strlen(buf));
	sprintf(buf, "%s", user_agent_hdr);
	printf("%s", buf);
	Rio_writen(fd_to_server, buf, strlen(buf));
	sprintf(buf, "Connection: close\r\n");
	printf("%s", buf);
	Rio_writen(fd_to_server, buf, strlen(buf));
	sprintf(buf, "Proxy-Connection: close\r\n\r\n");
	printf("%s", buf);
	Rio_writen(fd_to_server, buf, strlen(buf));
	
	cache_value = Malloc(MAX_OBJECT_SIZE);
	char* offset = cache_value;
	printf("========== receive reponse from server and forward back to client========\n");
	do {
		n = Rio_readlineb(&rio_to_server, buf, MAXLINE);
		memcpy(offset, buf, n);
		offset += n;
	} while (n != 0);


	Rio_writen(rio_from_client->rio_fd, cache_value, offset-cache_value);

	acquire_write_lock();
	insert_cache(uri, strlen(uri), cache_value, offset-cache_value);
	release_write_lock();

	Free(cache_value);
	Close(fd_to_server);

	return;
}

void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg) {
	char buf[MAXLINE];
	
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n\r\n");
	Rio_writen(fd, buf, strlen(buf));

	sprintf(buf, "<html><title>Proxy Error</title>");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "<hr><em>The proxy server</em>\r\n");
	Rio_writen(fd, buf, strlen(buf));

}

void parse_uri(char* uri, char* hostname, char* port, char* path) {
	regex_t reg;
	int reg_res;
	int status;
	
	reg_res = regcomp(&reg, "^http://[^:]+:[0-9]+.*", REG_EXTENDED);
	if (!reg_res) {
		printf("regEx compiled successfully\n");
	} else {
		printf("compilation error\n");
	}

	status = regexec(&reg, uri, 0, NULL, 0);
	regfree(&reg);

	if (status == 0) {
		printf("port is inputted for uri %s\n", uri);
		sscanf(uri, "http://%99[^:/]:%99[^/]/%99[^\n]", hostname, port, path);
	} else {
		printf("no port is inputted for uri %s\n", uri);
		sscanf(uri, "http://%99[^:/]/%99[^\n]", hostname, path);
		sscanf("80", "%s", port);
	}
	printf("hostname = %s\n", hostname);
	printf("port = %s\n", port);
	printf("path = %s\n", path);
}

