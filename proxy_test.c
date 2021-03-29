//
// proxy.c - CS:APP Web proxy
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

#define BUFFERSIZE 100
#define MAXURL 2048
#define BACKLOG 10   // how many pending connections queue will hold

int HaveDoubleEnter(unsigned char* pcInOutBuffer);
int DynamicCopyBuffer(unsigned char** pcDestBuffer, unsigned char* pcSrcBuffer, const int nDestBuffer);
int ProcessFromHeader(unsigned char* pcInHeader, unsigned char* pcOutHeader, unsigned char* pcOutHostname, int* pnOutport);
void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);

int main(int argc, char* argv[])
{

    //Port Parser 
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        return 1;
    }


    //Networking Beej's Guide
    int sockfd;  // listen on sock_fd, new connection on newClient
    int newClient = 0;
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

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
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

            //1. Recv from telnet
            unsigned char* pcRecvHeader = calloc(1, BUFFERSIZE+1);
            unsigned char* pcCopiedHeader = calloc(1,BUFFERSIZE+1);
            unsigned char* pcHost = calloc(1,MAXURL+1);
            unsigned char* pcBuffer = calloc(1,BUFFERSIZE+1);
            unsigned char* pcSendBuffer = calloc(1,BUFFERSIZE+1);
            int nDestBuffer = 1;
            int nPort = 0;
            int nRemainHeaderSize = BUFFERSIZE;
            int nRecvHeaderSize = 0;
            do{
                nRecvHeaderSize=recv(newClient,pcRecvHeader,nRemainHeaderSize,0);
                if(nRecvHeaderSize == -1){ 
                    perror("recv");
                    break;
                }
                nRemainHeaderSize-=nRecvHeaderSize;
                nDestBuffer = DynamicCopyBuffer(&pcCopiedHeader,pcRecvHeader, nDestBuffer);
                if(HaveDoubleEnter(pcCopiedHeader)) 
                {
                    break;
                }
                else
                {
                    memset(pcRecvHeader,0,BUFFERSIZE+1);
                }
            }
            while(nRecvHeaderSize>0);
            memset(pcRecvHeader,0,BUFFERSIZE+1);
            unsigned char* pcSendHeader = calloc(1,strlen(pcCopiedHeader)+1);

            if(!ProcessFromHeader(pcCopiedHeader,pcSendHeader,pcHost,&nPort)) 
            {
                goto MemFree;
            }

            //2. Send To HTTP Server
            struct addrinfo* addrHTTPServer;
            // IP Get from DNS server
            if(getaddrinfo(pcHost,NULL,NULL,&addrHTTPServer)!=0) 
            {
                fprintf(stderr,"503 Service Unavailable\n");
                goto MemFree;
            }
            // Set Port
            struct sockaddr_in* addrHTTPServerIn = (struct sockaddr_in*)addrHTTPServer->ai_addr;
            addrHTTPServerIn->sin_port = htons(nPort);

            //Connection with HTTP Server
            int nServerSock = 0;

            if((nServerSock = socket(addrHTTPServer->ai_family, addrHTTPServer->ai_socktype, 0))==-1)
            {  
                perror("socket");
                goto MemFree;
            }
            if(connect(nServerSock, addrHTTPServer->ai_addr,addrHTTPServer->ai_addrlen)==-1)
            {
                perror("connect");
                goto MemFree;
            }
            if(send(nServerSock,pcSendHeader,strlen(pcSendHeader)+1,0)==-1)
            {
                perror("send");
                goto MemFree;
            }
            if(pcSendHeader != NULL) free(pcSendHeader);
            //3. Recv HTTP data from Web server 
            int nRemainSize = BUFFERSIZE;
            int nRecvSize = 0;
            int isEnd = 0;
            int nSend = 0;
            while(1){
                nRemainSize = BUFFERSIZE;
                do{
                    nRecvSize=recv(nServerSock,pcBuffer,nRemainSize,0);
                    if(nRecvSize == 0){ 
                        isEnd = 1;
                        break;
                    } // TODO
                    memcpy(pcSendBuffer+(BUFFERSIZE-nRemainSize),pcBuffer,nRecvSize);
                    memset(pcBuffer,0,BUFFERSIZE+1);
                    nRemainSize-=nRecvSize;
                }
                while(nRemainSize>0);
                if(isEnd && (strlen(pcSendBuffer)==0))
                {
                    break;
                }
                //4. Send HTTP data to client
                nSend = send(newClient,pcSendBuffer,strlen(pcSendBuffer),0);
                if(nSend==-1)
                {
                    perror("send");
                    goto MemFree;
                }
                if(isEnd)
                {
                    break;
                }
                memset(pcSendBuffer,0,BUFFERSIZE+1);
            }

        MemFree:
            if(pcRecvHeader != NULL) free(pcRecvHeader);
            if(pcCopiedHeader != NULL) free(pcCopiedHeader);
            if(pcHost != NULL) free(pcHost);
            if(pcBuffer != NULL) free(pcBuffer);
            if(pcSendBuffer != NULL) free(pcSendBuffer);
            if(nServerSock !=0) close(nServerSock);
            if(newClient !=0) close(newClient);
            exit(0);
        }
        if(newClient !=0) close(newClient);  // parent doesn't need this
    }

    return 0;
}
int HaveDoubleEnter(unsigned char* pcInOutBuffer){
    /*
        Input/Output : pcInOutBuffer -> DynamicCopyBuffer(_,pcSrcBuffer,_)
        return : Have "\r\n\r\n" 1 , If not 0 
    */
    void* pvNeedle = NULL;
    pvNeedle = strstr(pcInOutBuffer,"\r\n\r\n");
    if(pvNeedle != NULL) return 1;
    else return 0;
}
int DynamicCopyBuffer(unsigned char** pcDestBuffer, unsigned char* pcSrcBuffer, const int nDestBuffer){

	/*
        Input/Output : pcDestBuffer 
                       Already allocated (BUFFERSIZE+1), already written
                       (Init Setting : pcDestBuffer = calloc(1,BUFFERSIZE + 1);)
        Input        : pcSrcBuffer  
                       MemorySize == BUFFERSIZE + 1
        Input        : nDestBuffer
                       pcDestBuffer memory size == nDestBuffer * BUFFERSIZE +1
        return       : nDestBuffer
    */
   int nBufferLength = strlen(*pcDestBuffer);
   int nSrcLength = strlen(pcSrcBuffer);

   if ((nBufferLength+nSrcLength) > nDestBuffer*BUFFERSIZE){
       unsigned char* pcTemp = calloc(1,((nDestBuffer+1)*BUFFERSIZE) +1);
       pcTemp = strcat(pcTemp,*pcDestBuffer);
       pcTemp = strcat(pcTemp,pcSrcBuffer);
       free(*pcDestBuffer);
       *pcDestBuffer = pcTemp;
       return nDestBuffer+1;
   }
   else{
       *pcDestBuffer = strcat(*pcDestBuffer,pcSrcBuffer);
       return nDestBuffer;
   }
}
int ProcessFromHeader(unsigned char* pcInHeader, unsigned char* pcOutHeader, unsigned char* pcOutHostname, int* pnOutport){

    /*    
        Input  : pcInHeader == GET http://www.google.co.kr/ HTTP/1.0
                               Host: www.google.co.kr
                               .... \r\n\r\n\0
        Output : pcOutHeader == GET / HTTP/1.0
                                Host : www.google.co.kr\r\n\r\n\0
        Output : pcOutHostname => www.google.co.kr already allocated char[MAXURL+1]
               : pnOutport == 80
        return : Error==0, Success==1
    */
    *pnOutport = 80;
    if(strncmp("GET",pcInHeader,3) != 0){ //should consider (only Start of Buffer)!
        fprintf(stderr,"<GET> Command Not Exist\n");
        return 0;
    }

    unsigned char* pcHostNeedle = NULL;
    pcHostNeedle = strstr(pcInHeader,"Host");
    if(pcHostNeedle == NULL){
        fprintf(stderr,"Host Command Not Exist\n");
        return 0;
    }

    
    unsigned char* pcTempFromCommand = strstr(pcInHeader, "http://");
    if(pcTempFromCommand == NULL){
        fprintf(stderr,"GET url Not Exist\n");
        return 0;
    }
    pcTempFromCommand+=7;//strlen("http://") == 7
    unsigned char* pcTempFromCommandEnd = strpbrk(pcTempFromCommand, ":/ ");
    int nUrlLength = pcTempFromCommandEnd-pcTempFromCommand; //without '/'

    unsigned char* pcTempFromHost = strchr(pcHostNeedle, ' ');
    if(pcTempFromHost == NULL){
        fprintf(stderr,"HOST url Not Exist\n");
        return 0;
    }
    pcTempFromHost+=1;
    //TODO : Erase From Host http:// Not Did
    unsigned char* pcHostRequest = NULL;
    pcHostRequest = strstr(pcTempFromHost,"http://");
    if(pcHostRequest != NULL)
    {
        pcTempFromHost+=7;
    }


    unsigned char* pcTempFromHostEnd = strstr(pcTempFromHost, "\r\n");
    int nUrlFromHostLength = pcTempFromHostEnd - pcTempFromHost;

    if(nUrlLength != nUrlFromHostLength)
    {
        fprintf(stderr,"400 Bad Request\n");
        return 0;
    }
    else if (strncmp(pcTempFromHost,pcTempFromCommand,nUrlLength) != 0)
    {
        fprintf(stderr,"400 Bad Request\n");
        return 0;
    }
    else
    {
        strncpy(pcOutHostname,pcTempFromCommand,nUrlLength);
    }
    if(*pcTempFromCommandEnd == ':')
    {
        *pnOutport = atoi(pcTempFromCommandEnd+1);
        pcTempFromCommandEnd = strpbrk(pcTempFromCommandEnd,  "/ ");
    }

    strncat(pcOutHeader,pcInHeader,4);// "GET " 

    if(*pcTempFromCommandEnd == ' ')
    {
        strncat(pcOutHeader,"/",1);
    }
    if (pcHostRequest == NULL)
    {
        strcat(pcOutHeader,pcTempFromCommandEnd);// "/ HTTP/1.0 .... \0"
    }
    else 
    {
        int nCmdCpyLength = pcHostNeedle-pcTempFromCommandEnd;
        strncat(pcOutHeader, pcTempFromCommandEnd, nCmdCpyLength);
        strcat(pcOutHeader,"Host: ");
        strcat(pcOutHeader,pcTempFromHost);
    }
    
    return 1;
}
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
