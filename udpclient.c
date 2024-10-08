#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <ctype.h>

#define MAXLINE 512
#define SERV_PORT 6666
#define LOCAL_PORT 8888

int main(int argc, char *argv[])
{
	struct sockaddr_in servaddr;
	int sockfd, n;
	char buf[MAXLINE];

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	// inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
	inet_pton(AF_INET, "172.16.38.104", &servaddr.sin_addr);
	servaddr.sin_port = htons(SERV_PORT);

    


struct sockaddr_in cliaddr;
	socklen_t cliaddr_len;

	bzero(&cliaddr, sizeof(cliaddr));
	cliaddr.sin_family = AF_INET;
	// servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	cliaddr.sin_addr.s_addr = inet_addr("172.16.38.105");
	cliaddr.sin_port = htons(LOCAL_PORT);

	bind(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr));


    // buf[0]='a';
    // buf[1]='b';
	while (fgets(buf, MAXLINE, stdin) != NULL) {
	// while (1) {
		n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
		if (n == -1)
			perror("sendto error");
		n = recvfrom(sockfd, buf, MAXLINE, 0, NULL, 0);
		if (n == -1)
			perror("recvfrom error");
		write(STDOUT_FILENO, buf, n);
	}
	close(sockfd);
	return 0;
}