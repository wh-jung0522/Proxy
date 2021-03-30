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

#define REDIRECTIONURL "warning.or.kr"
#define BUFFERSIZE 1024
#define MAXURL 2048
#define BACKLOG 10   // how many pending connections queue will hold

int HaveDoubleEnter(unsigned char* pcInOutBuffer);
int DynamicCopyBuffer(unsigned char** pcDestBuffer, unsigned char* pcSrcBuffer, const int nDestBuffer);
int ProcessFromHeader(unsigned char* pcInHeader, unsigned char* pcOutHeader, unsigned char* pcOutHostname, int* pnOutport, unsigned char**pcInBlackList, int nBlackList);
int IsRedirection(unsigned char* pcInputHost, unsigned char** Blacklist, int nBlackList);
unsigned char** FileToList(int* nListSize);
void FreeArrayBuffer(unsigned char** pcBufferArray, int nTotalBuffer);

void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);

int main(int argc, char* argv[])
{

    //Port Parser 
    if (argc != 2) {
        //fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        return 1;
    }

    int nBlackList = 0;
    unsigned char** pcBlackList = FileToList(&nBlackList);

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
        //fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
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
        //fprintf(stderr, "server: failed to bind\n");
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

    //printf("server: waiting for connections...\n");


    while(1) {  // main accept() loop
        lenClientLength = sizeof structClientAddr;
        newClient = accept(sockfd, (struct sockaddr *)&structClientAddr, &lenClientLength);
        if (newClient == -1) { perror("accept"); continue;}

        inet_ntop(structClientAddr.ss_family, get_in_addr((struct sockaddr *)&structClientAddr), s, sizeof s);
        //printf("server: got connection from %s\n", s);

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
            int ErrorNo = 0;
            char* ErrorText[] = {"400 Bad Request\n","503 Service Unavailable\n"};
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


            if((ErrorNo = ProcessFromHeader(pcCopiedHeader,pcSendHeader,pcHost,&nPort, pcBlackList, nBlackList)) != 0) 
            {
                //TODO : Error Handling
                switch(ErrorNo)
                {
                    case 400:
                    {
                        if(send(newClient,ErrorText[0],strlen(ErrorText[0]),0)==-1)
                        {
                            perror("send");
                            goto MemFree;
                        }
                        break;
                    }
                    case 503:
                    {    
                        if(send(newClient,ErrorText[1],strlen(ErrorText[1]),0)==-1)
                        {
                            perror("send");
                            goto MemFree;
                        }
                        break;
                    }
                }
                goto MemFree;
            }

            //2. Send To HTTP Server
            struct addrinfo* addrHTTPServer;
            // IP Get from DNS server
            if(getaddrinfo(pcHost,NULL,NULL,&addrHTTPServer)!=0) 
            {
                //fprintf(stderr,"503 Service Unavailable\n");
                if(send(newClient,ErrorText[1],strlen(ErrorText[1]),0)==-1)
                {
                    perror("send");
                    goto MemFree;
                }                
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
            if(send(nServerSock,pcSendHeader,strlen(pcSendHeader),0)==-1)
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
    if(pcBlackList!=NULL) FreeArrayBuffer(pcBlackList,nBlackList);
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
int ProcessFromHeader(unsigned char* pcInHeader, unsigned char* pcOutHeader, unsigned char* pcOutHostname, int* pnOutport, unsigned char**pcInBlackList, int nBlackList){

    /*    
        Input  : pcInHeader == GET http://www.google.co.kr/ HTTP/1.0
                               Host: www.google.co.kr
                               .... \r\n\r\n\0
        Output : pcOutHeader == GET / HTTP/1.0
                                Host : www.google.co.kr\r\n\r\n\0
        Output : pcOutHostname => www.google.co.kr already allocated char[MAXURL+1]
               : pnOutport == 80
        return : Error==400,503, Success==0
    */
    *pnOutport = 80;

    if(strncmp("GET ",pcInHeader,4) != 0)//TODO : space
    { 
        //fprintf(stderr,"<GET> Command Not Exist\n");
        return 400;
    }
    unsigned char* pcNeedle = pcInHeader+3;
    while(1)
    {
        if(*pcNeedle!=' ')
        {
            break;
        }
        pcNeedle++;
    }//Erase Space

    unsigned char* pcURLFromCommand = pcNeedle;
    if(strncmp(pcURLFromCommand, "http://",7) != 0){
        //fprintf(stderr,"GET url Not Exist\n");
        return 503;
    }
    pcURLFromCommand+=7;//strlen(" http://") == 7
    unsigned char* pcURLPathFromCommand = strpbrk(pcURLFromCommand, ":/ ");//GET http://www.kist.re.kr/kist_web/resource/image/common/footer_logo.png:80 HTTP/1.0 case
    unsigned char* pcURLEndFromCommand = strchr(pcURLFromCommand,' ');
    unsigned char* pcErrorChkNeedle = NULL;
    int nHostLengthFromCommand = pcURLPathFromCommand-pcURLFromCommand; //without '/'
    int nURLPathLength = 0;

    //If pcURLPathFromCommand == '/' -> Port check one more
    unsigned char* pcPortStart = NULL;
    if(*pcURLPathFromCommand == '/')
    {
        pcPortStart = strchr(pcURLPathFromCommand,':');
        if((pcPortStart != NULL)&&(pcPortStart < pcURLEndFromCommand))//GET http://www.kist.re.kr/a:80 HTTP/1.0 
        {
            *pnOutport = atoi(pcPortStart+1);
            nURLPathLength = pcPortStart-pcURLPathFromCommand;
        }
        else//GET http://www.kist.re.kr/a/b/c/d HTTP/1.0
        {
            nURLPathLength = pcURLEndFromCommand-pcURLPathFromCommand;
        }
    }
    else if(*pcURLPathFromCommand == ':')//GET http://www.kist.re.kr:80 HTTP/1.0 
    {   
        pcErrorChkNeedle = strpbrk(pcURLPathFromCommand+1, ":/ ");
        if(pcErrorChkNeedle != NULL)
        {
            if(pcErrorChkNeedle != pcURLEndFromCommand)
            {
                //GET http://www.kist.re.kr:80/ HTTP/1.0 
                //GET http://www.kist.re.kr::80 HTTP/1.0
                //GET http://www.kist.re.kr:80: HTTP/1.0   
                //fprintf(stderr,"URL Error\n");
                return 400;
            }
        }
        
        *pnOutport = atoi(pcURLPathFromCommand+1);
        nURLPathLength = 0;
    }
    else //GET http://www.kist.re.kr HTTP/1.0 
    {
        nURLPathLength = 0;
    }
    //if nURLPathLength == 0 -> add '/' else add strncpy(pcOut, pcURLPathFromCommand ,nURLPahtLength);
    pcNeedle = pcURLEndFromCommand;
    while(1)
    {
        if(*pcNeedle!=' ')
        {
            break;
        }
        pcNeedle++;
    }//Erase Space

    unsigned char* pcHTTPNeedle = pcNeedle;//TODO : space
    if(strncmp(pcHTTPNeedle,"HTTP/1.0",8)!=0){
        //fprintf(stderr,"HTTP Error\n");
        return 400;
    }
    pcNeedle += 8;
    while(1)
    {
        if(*pcNeedle!=' ')
        {
            break;
        }
        pcNeedle++;
    }//Erase Space
    unsigned char* pcHostCmdNeedle = pcNeedle;
    if(strncmp(pcHostCmdNeedle,"\r\nHost",6) != 0){
        ////fprintf(stderr,"Host Command Not Exist\n");
        return 400;
    }
    pcNeedle += 6;
    while(1)
    {
        if(*pcNeedle!=' ')
        {
            break;
        }
        pcNeedle++;
    }//Erase Space
    if(strncmp(pcNeedle,":",1) != 0){
        ////fprintf(stderr,"Host : Not Exist\n");
        return 400;
    }
    pcNeedle += 1;
    while(1)
    {
        if(*pcNeedle!=' ')
        {
            break;
        }
        pcNeedle++;
    }//Erase Space

    unsigned char* pcHostNameStart = pcNeedle;
    if(strncmp(pcHostNameStart,"http://",7) == 0)
    {
        pcHostNameStart +=7;
    }
    unsigned char* pcHostNameEnd = strpbrk(pcHostNameStart, " \r\n");
    int nHostLengthFromName = pcHostNameEnd-pcHostNameStart;


    if(nHostLengthFromCommand != nHostLengthFromName)
    {
        ////fprintf(stderr,"503 Service Unavailable\n");
        return 400;
    }
    else if (strncmp(pcURLFromCommand,pcHostNameStart,nHostLengthFromCommand) != 0)
    {
        ////fprintf(stderr,"400 Bad Request\n");
        return 400;
    }
    else
    {
        strncpy(pcOutHostname,pcURLFromCommand,nHostLengthFromCommand);
    }
    //TODO : Host & Blacklist Compare , 

    strncat(pcOutHeader,pcInHeader,4);// "GET " 
    //if nURLPathLength == 0 -> add '/' else add strncpy(pcOut, pcURLPathFromCommand ,nURLPahtLength);
    

    //TODO : BlackList

    if(IsRedirection(pcOutHostname,pcInBlackList,nBlackList))
    {
        strcat(pcOutHeader,"/ HTTP/1.0\r\nHost: ");
        strcat(pcOutHeader,REDIRECTIONURL);
        strcat(pcOutHeader,"\r\n\r\n");  
        memset(pcOutHostname,0,strlen(pcOutHostname)+1);
        strncpy(pcOutHostname,REDIRECTIONURL,strlen(REDIRECTIONURL));
    }
    else
    {
        if(nURLPathLength == 0)
        {
            strcat(pcOutHeader,"/");
        }
        else
        {
            strncat(pcOutHeader, pcURLPathFromCommand ,nURLPathLength);
        }
        strcat(pcOutHeader," HTTP/1.0\r\nHost: ");
        strcat(pcOutHeader,pcOutHostname);
        strcat(pcOutHeader,"\r\n\r\n");    
    }
    return 0;
}


int IsRedirection(unsigned char* pcInputHost, unsigned char** Blacklist, int nBlackList)
{
    if(Blacklist == NULL) return 0;
    for(int i=0;i<nBlackList;i++)
    {
        if(strcmp(pcInputHost,Blacklist[i])==0)
        {
            return 1;
        }
    }
    return 0;
}

unsigned char** FileToList(int* nListSize)
{
    if(isatty(STDIN_FILENO)==1) 
    {
        *nListSize = 0;
        return NULL;
    }
    int nBlackList = 0;
    int nMulBlacklist = 1;
    unsigned char** TempBlackList = NULL;
    unsigned char* pcReadLine = calloc(1,MAXURL+1);
    unsigned char* pcStartNeedle = NULL;
    unsigned char* pcEndNeedle = NULL;
    int nHostLength = 0;
    unsigned char** InBlacklist = calloc(sizeof(unsigned char*),BUFFERSIZE);

    while((fgets(pcReadLine,MAXURL,stdin)!= NULL))//http:// & \n not store!
    {
        nBlackList++;
        if (nBlackList > (nMulBlacklist*BUFFERSIZE))
        {
            //realloc Blacklist
            TempBlackList = calloc(sizeof(unsigned char*),(BUFFERSIZE*(nMulBlacklist+1)));
            for(int i=0;i<(BUFFERSIZE*nMulBlacklist);i++)
            {
                TempBlackList[i] = InBlacklist[i];
            }
            free(InBlacklist);
            InBlacklist = TempBlackList;
            nMulBlacklist++;
        }
        pcStartNeedle = strstr(pcReadLine,"http://");
        if(pcStartNeedle == NULL)
        {
            ////fprintf(stderr, "BlackList format Error\n");
            *nListSize = -1;
            return NULL;
        }
        pcStartNeedle+=7;
        pcEndNeedle = strchr(pcStartNeedle,'\n');

        InBlacklist[nBlackList-1] = calloc(1,strlen(pcReadLine)+1);

        if(pcEndNeedle == NULL)
        {
            strcpy(InBlacklist[nBlackList-1],pcStartNeedle);
        }
        else
        {
            strncpy(InBlacklist[nBlackList-1],pcStartNeedle,pcEndNeedle-pcStartNeedle);
        }
        pcStartNeedle = NULL;
        pcEndNeedle = NULL;
        memset(pcReadLine,0,MAXURL+1);
    }
    *nListSize = nBlackList;
    return InBlacklist;
}
void FreeArrayBuffer(unsigned char** pcBufferArray, int nTotalBuffer)
{
    for(int i=0;i<nTotalBuffer;i++)
    {
        free(pcBufferArray[i]);
    }
    free(pcBufferArray);
    return;
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
