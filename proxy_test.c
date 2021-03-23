/*


*/

//
// proxy.c - CS:APP Web proxy
//
// Student Information:
//     김지현, 2013-11392
//
// 0.  argv[1]에서 프록시를 작동시킬 포트 번호를 얻어낸다.
// 1.  소켓 하나를 초기화시켜 연결이 들어올때까지 기다린다.
// 2.  연결이 맺어질경우 "\r\n\r\n"을 받을때까지 읽어서 request header를 받는다.
//
// 3.  얻어낸 Request header에서, "Connection: keep-alive" 라는 문자열을 찾아
//     "Connection: close"로 치환한다. 이때 길이가 변하는데, memmove() 함수로
//     처리한다.
//
// 4.  Request header에 적혀있는 서버 URI를 hostname과 포트번호, path로
//     분리한다음, hostname에 대응되는 IP주소가 몇인지 DNS Lookup을 수행한다.
//
// 5.  DNS Lookup으로 알아낸 IP 주소를 통해, 웹서버와 커넥션을 맺는다. 버퍼에
//     저장된 Request header를 서버로 보낸다.
//
// 6.  웹서버가 연결을 종료할때까지 웹서버가 전송하는 모든 데이터를
//     클라이언트에게 보낸다.
//
// 1~6을 반복한다.
/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10   // how many pending connections queue will hold

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char* argv[])
{

    //Port Parser 
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        return 1;
    }


    //Networking Beej's Guide
    int sockfd, newClient;  // listen on sock_fd, new connection on newClient
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage structClientAddr; // connector's address information
    socklen_t lenClientLength;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        lenClientLength = sizeof structClientAddr;
        newClient = accept(sockfd, (struct sockaddr *)&structClientAddr, &lenClientLength);
        if (newClient == -1) { perror("accept"); continue;}

        inet_ntop(structClientAddr.ss_family, get_in_addr((struct sockaddr *)&structClientAddr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            //TODO
            //1. Recv HTTP request(Header) from client(Error process)
            //2. Send HTTP request to Web server
            //3. Recv HTTP data from Web server 
            //4. Send HTTP data to client




            if (send(newClient, "Hello, world!", 13, 0) == -1)
                perror("send");














            close(newClient);
            exit(0);
        }
        close(newClient);  // parent doesn't need this
    }

    return 0;
}