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
#include <ctype.h> // isalpha

#define BACKLOG 10

//Message Header Structure
typedef struct _MessageHeader{
	unsigned char bOperate;
	unsigned char bShift;
	unsigned short int nChecksum;
	unsigned int nLength;	
}MessageHeader;

//using Big-Endian Serialize/Deserialize
unsigned char * pcSerializedInt(unsigned char * buffer, unsigned int value);
unsigned char * pcSerializedShortInt(unsigned char* buffer, unsigned short int value);
unsigned char * pcSerializedChar(unsigned char *buffer, unsigned char value);
unsigned char * pcSerializedHeader(unsigned char *buffer, MessageHeader* pHeader);
MessageHeader DeSerializedFromBuffer(unsigned char * buffer);
void FreeArrayBuffer(unsigned char** pcBufferArray, int nTotalBuffer);

//Caeser En/DeCoding
unsigned char cCaeserAlphabet(unsigned char cInputAlphabet, const unsigned char cOperate, const int nkey);
void ConvertCaeserText(unsigned char* cOutput,const unsigned char* cInput, const unsigned char cOperate, const int nkey);

//CheckSum
unsigned short checksum(const char *buf, unsigned size);

void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa); // get sockaddr, IPv4 or IPv6:

MessageHeader UpdateMessageHeaderFromText(MessageHeader messageHeader,unsigned char* pcText);
int nValidCheck(MessageHeader messageHeader, unsigned char* pcTextData);

int main(int argc, char *argv[])
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    //Header Structure
    MessageHeader recvMessageHeader;
    MessageHeader sendMessageHeader;

    //Header Buffer
    unsigned char *pcRecvHeaderBuffer = NULL;
    unsigned char *pcRecvHeaderBufferEnd = NULL;    
    unsigned char *pcSendHeaderBuffer = NULL;
    unsigned char *pcSendHeaderBufferEnd = NULL;
    
    //Data Buffer
    unsigned char *pcRecvTextData = NULL;
    unsigned char *pcSendTextData = NULL;

    //Data Size
    unsigned int nTextSize = 0;


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    //Port Parser 
    int nOption = 0;
	char* pcPort = NULL;
	while((nOption = getopt(argc, argv, "p:")) != EOF )
	{
		switch (nOption)
		{
		case 'p':
			pcPort = (char*)malloc(strlen(optarg));
			strcpy(pcPort,optarg);
			break;	
		case '?':
			fprintf(stderr,"Unknown flag : %c\n", optopt);
			break;
		}

	}

    if ((rv = getaddrinfo(NULL, pcPort, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
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
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            //TODO : receive MessageHeader, Message, Process
            

            unsigned char** pcRecvArray = NULL;
            unsigned char** pcTempArray = NULL;
            unsigned char** pcSendArray = NULL;
            unsigned char** pcTempSendArray = NULL;


            pcRecvHeaderBuffer = (unsigned char*)malloc(sizeof(MessageHeader));
            memset(pcRecvHeaderBuffer,0,sizeof(MessageHeader));
            int nRecvArray = 0;
            int nRecvHeader;
            int nRecvHeaderTemp;
            while ((nRecvHeader = recv(new_fd, pcRecvHeaderBuffer, sizeof(MessageHeader), MSG_DONTWAIT)))// recv Data
            {
                            
                if(nRecvHeader == 0) break;
                else if(nRecvHeader == -1) break;

                while (nRecvHeader < sizeof(MessageHeader))
                {
                    unsigned char* pcRecvTemp = calloc(1,sizeof(MessageHeader)-nRecvHeader);
                    if ((nRecvHeaderTemp = recv(new_fd, pcRecvTemp, sizeof(MessageHeader)-nRecvHeader, 0)) == -1) 
                    {
                        perror("recv");
                        return 0;
                    }
                    memcpy((pcRecvHeaderBuffer+nRecvHeader),pcRecvTemp,nRecvHeaderTemp);
                    free(pcRecvTemp);
                    nRecvHeader += nRecvHeaderTemp;
                }


                recvMessageHeader = DeSerializedFromBuffer(pcRecvHeaderBuffer);
                if(pcRecvArray == NULL) 
                {
                    pcRecvArray = malloc(sizeof(unsigned char *));
                    memset(pcRecvArray,0,sizeof(unsigned char *));
                    pcSendArray = malloc(sizeof(unsigned char *));
                    memset(pcSendArray,0,sizeof(unsigned char *));
                }
                else 
                {
                    pcTempArray = malloc(sizeof(unsigned char *)*(nRecvArray+1));
                    memset(pcTempArray,0,sizeof(unsigned char *)*(nRecvArray+1));
                    pcTempSendArray = malloc(sizeof(unsigned char *)*(nRecvArray+1));
                    memset(pcTempSendArray,0,sizeof(unsigned char *)*(nRecvArray+1));
                    for(int j= 0; j<nRecvArray;j++)
                    {
                        pcTempArray[j] = pcRecvArray[j];
                        pcTempSendArray[j] = pcSendArray[j];
                    }
                    free(pcRecvArray);
                    free(pcSendArray);
                    pcRecvArray = pcTempArray;
                    pcSendArray = pcTempSendArray;
                }
                nTextSize = recvMessageHeader.nLength-((unsigned int)sizeof(MessageHeader));
                pcRecvArray[nRecvArray] = malloc(nTextSize+1);
                memset(pcRecvArray[nRecvArray],0,nTextSize+1);
                pcRecvArray[nRecvArray][nTextSize] = '\0';
                pcSendArray[nRecvArray] = malloc(nTextSize+1);
                memset(pcSendArray[nRecvArray],0,nTextSize+1);
                pcSendArray[nRecvArray][nTextSize] = '\0';
                int nRecvFromSend;
                int nRecvTemp;
                if ((nRecvFromSend = recv(new_fd, pcRecvArray[nRecvArray], nTextSize, 0)) == -1) perror("recv");
                while (nRecvFromSend < nTextSize)
                {
                    unsigned char* pcRecvTemp = calloc(1,nTextSize-nRecvFromSend+1);
                    if ((nRecvTemp = recv(new_fd, pcRecvTemp, nTextSize-nRecvFromSend, 0)) == -1) 
                    {
                        perror("recv");
                        return 0;
                    }
                    strcat(pcRecvArray[nRecvArray],pcRecvTemp);
                    free(pcRecvTemp);
                    nRecvFromSend += nRecvTemp;
                }


                if(((int)strlen(pcRecvArray[nRecvArray]))!=nTextSize)
                {
                    int a = (int)strlen(pcRecvArray[nRecvArray]);
                    int b = nTextSize;
                }
                //Check
                //fprintf(stdout,"Text : %s \nLength : %d\n",pcRecvArray[nRecvArray],(int)strlen(pcRecvArray[nRecvArray]));
                //CheckSum Valid
                if(nValidCheck(recvMessageHeader,pcRecvArray[nRecvArray])==0)
                {
                    fprintf(stderr, "CheckSum Error\n");
                    close(new_fd);
                    exit(0);
                }
                //If not -> Recv Header, Recv Array
                nRecvArray++;
            }


            pcSendHeaderBuffer = (unsigned char*)malloc(sizeof(MessageHeader));
            memset(pcSendHeaderBuffer,0,sizeof(MessageHeader));
            for(int j=0;j<nRecvArray;j++)
            {
                ConvertCaeserText(pcSendArray[j],pcRecvArray[j],recvMessageHeader.bOperate,recvMessageHeader.bShift);
                sendMessageHeader = UpdateMessageHeaderFromText(recvMessageHeader,pcSendArray[j]);
                nTextSize = sendMessageHeader.nLength-sizeof(MessageHeader);
                pcSendHeaderBufferEnd = pcSerializedHeader(pcSendHeaderBuffer,&sendMessageHeader);
                if (send(new_fd, pcSendHeaderBuffer, sizeof(MessageHeader), 0) == -1) perror("send");
                //Send Text
                if (send(new_fd, pcSendArray[j], nTextSize, 0) == -1) perror("send");
                //Check
                //fprintf(stdout,"Text : %s \nLength : %d\n",pcSendArray[j],(int)strlen(pcSendArray[j]));

            }
            close(new_fd);
            exit(0);
            
            if(pcRecvArray != NULL) FreeArrayBuffer(pcRecvArray,nRecvArray);
            if(pcSendArray != NULL) FreeArrayBuffer(pcSendArray,nRecvArray);
            if(pcRecvHeaderBuffer != NULL) free(pcRecvHeaderBuffer);
            if(pcSendHeaderBuffer != NULL) free(pcSendHeaderBuffer);
        }
        close(new_fd);  // parent doesn't need this
    }
    if(pcPort != NULL) free(pcPort);
    return 0;
}

unsigned char * pcSerializedInt(unsigned char * buffer, unsigned int value)
{
    buffer[3] = value >> 24;
    buffer[2] = value >> 16;
    buffer[1] = value >> 8;
    buffer[0] = value;
    return buffer + 4;
}
unsigned char * pcSerializedShortInt(unsigned char* buffer, unsigned short int value)
{
    //only use checksum
    buffer[1] = value >> 8;
    buffer[0] = value;
    return buffer + 2;
}
unsigned char * pcSerializedChar(unsigned char *buffer, unsigned char value)
{
    buffer[0] = value;
    return buffer + 1;
}
unsigned char * pcSerializedHeader(unsigned char *buffer, MessageHeader* pHeader)
{
    buffer = pcSerializedChar(buffer, pHeader->bOperate);
    buffer = pcSerializedChar(buffer, pHeader->bShift);
    buffer = pcSerializedShortInt(buffer, pHeader->nChecksum);
    buffer = pcSerializedInt(buffer,htonl(pHeader->nLength));
    return buffer;
}

//using Big-Endian
MessageHeader DeSerializedFromBuffer(unsigned char * buffer)
{
    MessageHeader messageHeader;
    messageHeader.bOperate = *buffer;
    messageHeader.bShift = *(buffer+1);
    messageHeader.nChecksum = *((short unsigned int*)(buffer+2));
    messageHeader.nLength = ntohl(*((unsigned int *)(buffer+4)));
    return messageHeader;
}
void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
unsigned char cCaeserAlphabet(unsigned char cInputAlphabet, const unsigned char cOperate, const int nkey)
{
	unsigned char cOutputAlphabet;
	// Only for Alphabet
	if (isalpha(cInputAlphabet)){ //Alphabet
		if ((cInputAlphabet>='A') && (cInputAlphabet<='Z'))//UPPER Alphabet
		{
			cInputAlphabet+=32;
		}
		if (cOperate == 0)
		{
			//encryption
			if(cInputAlphabet+nkey > 'z')
			{
				cOutputAlphabet = cInputAlphabet+nkey - 26;
			}
			else
			{
				cOutputAlphabet = cInputAlphabet+nkey;
			}
		} 
		else if (cOperate == 1)
		{
			//decryption
			if(cInputAlphabet - nkey < 'a')
			{
				cOutputAlphabet = cInputAlphabet - nkey + 26;
			}
			else
			{
				cOutputAlphabet = cInputAlphabet - nkey;
			}
		}
	}
	else 
	{
		cOutputAlphabet = cInputAlphabet;
	}
	return cOutputAlphabet;
}
void ConvertCaeserText(unsigned char* cOutput,const unsigned char* cInput, const unsigned char cOperate, const int nkey)
{
	//	char* cOutput = (char*)malloc(nTextLength);
	//  already allocated memory 
	for(int i=0;i<strlen(cInput);i++)
	{
		cOutput[i] = cCaeserAlphabet(cInput[i],cOperate, nkey);
	}
	return;
}
unsigned short checksum(const char *buf, unsigned size)
{
	unsigned long long sum = 0;
	const unsigned long long *b = (unsigned long long *) buf;

	unsigned t1, t2;
	unsigned short t3, t4;

	/* Main loop - 8 bytes at a time */
	while (size >= sizeof(unsigned long long))
	{
		unsigned long long s = *b++;
		sum += s;
		if (sum < s) sum++;
		size -= 8;
	}

	/* Handle tail less than 8-bytes long */
	buf = (const char *) b;
	if (size & 4)
	{
		unsigned s = *(unsigned *)buf;
		sum += s;
		if (sum < s) sum++;
		buf += 4;
	}

	if (size & 2)
	{
		unsigned short s = *(unsigned short *) buf;
		sum += s;
		if (sum < s) sum++;
		buf += 2;
	}

	if (size)
	{
		unsigned char s = *(unsigned char *) buf;
		sum += s;
		if (sum < s) sum++;
	}

	/* Fold down to 16 bits */
	t1 = sum;
	t2 = sum >> 32;
	t1 += t2;
	if (t1 < t2) t1++;
	t3 = t1;
	t4 = t1 >> 16;
	t3 += t4;
	if (t3 < t4) t3++;

	return ~t3;
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
MessageHeader UpdateMessageHeaderFromText(MessageHeader messageHeader,unsigned char* pcText)
{
    MessageHeader updatedMessageHeader = messageHeader; //Copy Operator, Shift
    updatedMessageHeader.nChecksum = (unsigned short int)0; // init 0
    updatedMessageHeader.nLength = (unsigned int)(strlen(pcText)+8);
    unsigned char* pcWholeMessage = calloc(1,updatedMessageHeader.nLength+1);
    unsigned char* pcTemp = pcSerializedHeader(pcWholeMessage,&updatedMessageHeader);
    memcpy(pcTemp,pcText,strlen(pcText));
    pcTemp[strlen(pcText)] = '\0';
    updatedMessageHeader.nChecksum = checksum(pcWholeMessage,updatedMessageHeader.nLength);
    free(pcWholeMessage);
    return updatedMessageHeader;
}
int nValidCheck(MessageHeader messageHeader, unsigned char* pcTextData)
{
    int isValid;
    unsigned char* pcWholeMessage = calloc(1,messageHeader.nLength+1);
    unsigned char* pcTemp = pcSerializedHeader(pcWholeMessage,&messageHeader);
    memcpy(pcTemp,pcTextData,strlen(pcTextData));
    pcTemp[strlen(pcTextData)] = '\0';
    if(checksum(pcWholeMessage,messageHeader.nLength)==0) isValid=1;
    else isValid=0;
    free(pcWholeMessage);
    return isValid;
}