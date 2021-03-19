#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXBUFFERSIZE 100000
#define BACKLOG 10

typedef struct _MessageHeader{
	unsigned char bOperate;
	unsigned char bShift;
	unsigned short int nChecksum;
	unsigned int nLength;	
}MessageHeader;
//using Big-Endian
unsigned char * pcSerializedInt(unsigned char * buffer, unsigned int value);
unsigned char * pcSerializedShortInt(unsigned char* buffer, unsigned short int value);
unsigned char * pcSerializedChar(unsigned char *buffer, unsigned char value);
unsigned char * pcSerializedHeader(unsigned char *buffer, MessageHeader* pHeader);
MessageHeader DeSerializedFromBuffer(unsigned char * buffer);
void FreeArrayBuffer(unsigned char** pcBufferArray, int nTotalBuffer);
unsigned char** pcSliceBufferArray(unsigned char* pcInputBuffer, int nStrCpyMode);
MessageHeader UpdateMessageHeaderFromText(MessageHeader messageHeader,unsigned char* pcText);
int nValidCheck(MessageHeader messageHeader, unsigned char* pcTextData);
unsigned short checksum(const char *buf, unsigned size);
void *get_in_addr(struct sockaddr *sa);

int main(int argc, char *argv[])
{
    //Socket
    int sockfd, numbytes;  
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
	char* pcServerAddr = NULL;
	char* pcPort = NULL;
    //Header
    MessageHeader recvMessageHeader;
    MessageHeader sendMessageHeader;
    //Header Buffer
    unsigned char *pcSendHeaderBuffer = (unsigned char*)malloc(sizeof(MessageHeader));
    memset(pcSendHeaderBuffer,0,sizeof(MessageHeader));
    unsigned char *pcSendHeaderBufferEnd = NULL;
    unsigned char *pcRecvHeaderBuffer = (unsigned char*)malloc(sizeof(MessageHeader));
    memset(pcRecvHeaderBuffer,0,sizeof(MessageHeader));
    unsigned char *pcRecvHeaderBufferEnd = NULL;
    //TODO : argument Parser & Header Initialize & Text I/O

    //Argment Parser
    int nOption = 0;
    int TempOp;
	while( (nOption = getopt(argc, argv, "h:p:o:s:")) != EOF )
	{
		switch (nOption)
		{
		case 'h':
			pcServerAddr = (char*)calloc(1,strlen(optarg)+1);
			strcpy(pcServerAddr,optarg);
			break;
		case 'p':
			pcPort = (char*)calloc(1,strlen(optarg)+1);
			strcpy(pcPort,optarg);
			break;	
		case 'o':
			// if not optarg == 0,1  Exception
            TempOp = (int)strtol(optarg,NULL,10);
            if((TempOp!=0) && (TempOp!=1))
            {
                fprintf(stderr,"Operation must (0 or 1)\n");
                return 0;
            }
			sendMessageHeader.bOperate = TempOp; // 0.1 not '0','1'		
			break;
		case 's':
			TempOp = (int)strtol(optarg,NULL,10);
            if((TempOp>255))
            {
                fprintf(stderr,"Operation must (0~255) 1 byte code\n");
                return 0;
            }
			sendMessageHeader.bShift = TempOp%26;
			break;
		case '?':
			fprintf(stderr,"Unknown flag : %c\n", optopt);
			break;
		}

	}
    //Larger Than 10M -> Split & Send , Server Recv Send 

    //Input Text Stdin
	// fprintf(stdout,"Plaintext: ");
	unsigned char* pcSendTextData = NULL;
    unsigned char* pcRecvTextData = NULL;
	char cValue;
    unsigned int nTextSize = 0;
    int nMAXSIZE = 1;

    pcSendTextData = malloc(nMAXSIZE + 1);
    while ((cValue = getchar()) != EOF) {
        if (nTextSize == nMAXSIZE) {
            char* temp = malloc(2 * nMAXSIZE + 1);
            pcSendTextData[nTextSize] = '\0';
            strcpy(temp, pcSendTextData);
            free(pcSendTextData);
            pcSendTextData = temp;
            nMAXSIZE = 2 * nMAXSIZE;
        }
        pcSendTextData[nTextSize++] = cValue;
    }

    pcSendTextData[nTextSize] = '\0';
    pcRecvTextData = (unsigned char*)malloc(nTextSize+1);
    memset(pcRecvTextData,0,nTextSize+1);
    pcRecvTextData[nTextSize] = '\0';
	// https://www.javaer101.com/ko/article/45198781.html    
    // Split
    unsigned char ** pcSendBufferArray = NULL;
    unsigned char ** pcRecvBufferArray = NULL;


    pcSendBufferArray = pcSliceBufferArray(pcSendTextData, 1); //str copy mode


    pcRecvBufferArray = pcSliceBufferArray(pcSendTextData, 0); //memory allocate mode 

    
    if ((rv = getaddrinfo(pcServerAddr, pcPort, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }
    
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    // fprintf(stdout,"client: connecting to %s\n", s);

    freeaddrinfo(servinfo); 
    //OK
    //send
    
    int nInputBufferLength = nTextSize;
    int nSendTextSize;
    int nRecvTextSize;
    int nTotalBuffer;
    int nRemainder = nInputBufferLength%MAXBUFFERSIZE;
    if (nRemainder == 0 ) nTotalBuffer = (nInputBufferLength/MAXBUFFERSIZE);
    else nTotalBuffer = (nInputBufferLength/MAXBUFFERSIZE) +1;

    for(int j=0;j<nTotalBuffer;j++)
    {

        sendMessageHeader = UpdateMessageHeaderFromText(sendMessageHeader,pcSendBufferArray[j]);
        nSendTextSize = sendMessageHeader.nLength-sizeof(MessageHeader);
        pcSendHeaderBufferEnd = pcSerializedHeader(pcSendHeaderBuffer,&sendMessageHeader);
        int nSendHeader, nSend;
        if (nSendHeader = send(sockfd, pcSendHeaderBuffer, sizeof(MessageHeader), 0) == -1) perror("send");
        if(((int)strlen(pcSendBufferArray[j]))!=nSendTextSize )
        {
            int a = (int)strlen(pcSendBufferArray[j]);
            int b = nSendTextSize;
        }
        //Send Text
        if (nSend = send(sockfd, pcSendBufferArray[j], nSendTextSize, 0) == -1) perror("send");
        //fprintf(stdout,"SendText : %s \nLength : %d\n",pcSendBufferArray[j],(int)strlen(pcSendBufferArray[j]));
    }    
    //recv
    int nRecvHeader;
    int nRecvHeaderTemp;
    for(int j=0;j<nTotalBuffer;j++)
    {
        if ((nRecvHeader=recv(sockfd, pcRecvHeaderBuffer, sizeof(MessageHeader), 0)) == -1) 
        {
            perror("recv");
        }
        //fprintf(stderr,"length : %d\n", nRecvHeader);
        while(nRecvHeader < sizeof(MessageHeader))
        {
            
            unsigned char* pcRecvTemp = calloc(1,sizeof(MessageHeader)-nRecvHeader);
            if ((nRecvHeaderTemp = recv(sockfd, pcRecvTemp, sizeof(MessageHeader)-nRecvHeader, 0)) == -1) 
            {
                perror("recv");
            }
            memcpy((pcRecvHeaderBuffer+nRecvHeader),pcRecvTemp,nRecvHeaderTemp);
            free(pcRecvTemp);
            nRecvHeader += nRecvHeaderTemp;
        }

        recvMessageHeader = DeSerializedFromBuffer(pcRecvHeaderBuffer);
        nRecvTextSize = recvMessageHeader.nLength-sizeof(MessageHeader);
        
        int nRecvFromSend;
        int nRecvTemp;
        if ((nRecvFromSend = recv(sockfd, pcRecvBufferArray[j], nRecvTextSize, 0)) == -1) perror("recv");
        while (nRecvFromSend < nRecvTextSize)
        {
            
            unsigned char* pcRecvTemp = calloc(1,nRecvTextSize-nRecvFromSend+1);
            if ((nRecvTemp = recv(sockfd, pcRecvTemp, nRecvTextSize-nRecvFromSend, 0)) == -1) 
            {
                perror("recv");
            }
            strcat(pcRecvBufferArray[j],pcRecvTemp);
            free(pcRecvTemp);
            nRecvFromSend += nRecvTemp;
        }

        //Check
        //fprintf(stdout,"RecvText : %s \nLength : %d\n",pcRecvBufferArray[j],(int)strlen(pcRecvBufferArray[j]));
        if(nValidCheck(recvMessageHeader,pcRecvBufferArray[j])==0)
        {
            fprintf(stderr, "CheckSum Error\n");
            close(sockfd);
            return 0;
        }
    }
    close(sockfd);
    // fprintf(stdout,"%s\n",pcRecvTextData);
    
    for(int j=0;j<nTotalBuffer;j++)
    {
        fflush(stdout);
        fprintf(stdout,"%s",pcRecvBufferArray[j]);
        fflush(stdout);
    }

    if(pcRecvBufferArray != NULL) FreeArrayBuffer(pcRecvBufferArray,nTotalBuffer);
    if(pcSendBufferArray != NULL) FreeArrayBuffer(pcSendBufferArray,nTotalBuffer);
    if(pcRecvHeaderBuffer != NULL) free(pcRecvHeaderBuffer);      
    if(pcSendHeaderBuffer != NULL) free(pcSendHeaderBuffer);
    if(pcSendTextData != NULL) free(pcSendTextData);
    if(pcRecvTextData != NULL) free(pcRecvTextData);
    if(pcServerAddr != NULL) free(pcServerAddr);
    if(pcPort != NULL) free(pcPort);

    return 0;
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
unsigned char** pcSliceBufferArray(unsigned char* pcInputBuffer, int nStrCpyMode)
{
    int nInputBufferLength = strlen(pcInputBuffer);
    int nTotalBuffer;
    int nRemainder = nInputBufferLength%MAXBUFFERSIZE;
    if (nRemainder == 0 ) nTotalBuffer = (nInputBufferLength/MAXBUFFERSIZE);
    else nTotalBuffer = (nInputBufferLength/MAXBUFFERSIZE) +1;
    unsigned char** pcBufferArray = calloc(sizeof(unsigned char*),nTotalBuffer);
    for(int i=0;i<nTotalBuffer-1;i++)
    {
        pcBufferArray[i] = malloc(MAXBUFFERSIZE+1); // \0
        if (nStrCpyMode == 1){
            strncpy(pcBufferArray[i],(pcInputBuffer+i*MAXBUFFERSIZE),MAXBUFFERSIZE);
            pcBufferArray[i][MAXBUFFERSIZE] = '\0';
        }
        else
        {
            memset(pcBufferArray[i],0,MAXBUFFERSIZE+1);
            pcBufferArray[i][MAXBUFFERSIZE] = '\0';
        }
    }
    if (nRemainder==0)
    {
        pcBufferArray[nTotalBuffer-1] = malloc(MAXBUFFERSIZE+1); // \0
        if (nStrCpyMode == 1){
            strncpy(pcBufferArray[nTotalBuffer-1],(pcInputBuffer+((nTotalBuffer-1)*MAXBUFFERSIZE)),MAXBUFFERSIZE);
            pcBufferArray[nTotalBuffer-1][MAXBUFFERSIZE] = '\0';
        }
        else
        {
            memset(pcBufferArray[nTotalBuffer-1],0,MAXBUFFERSIZE+1);
            pcBufferArray[nTotalBuffer-1][MAXBUFFERSIZE] = '\0';
        }

    }
    else
    {
        pcBufferArray[nTotalBuffer-1] = malloc(nRemainder+1);
        if (nStrCpyMode == 1){
            strncpy(pcBufferArray[nTotalBuffer-1],(pcInputBuffer+(nTotalBuffer-1)*MAXBUFFERSIZE),nRemainder);
            pcBufferArray[nTotalBuffer-1][nRemainder] = '\0';
        }
        else
        {
            memset(pcBufferArray[nTotalBuffer-1],0,nRemainder+1);
            pcBufferArray[nTotalBuffer-1][nRemainder] = '\0';
        }
    }
    return pcBufferArray;
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
//pcOutputBuffer memory size is strlen(pcInputBuffer+1) // Because of '\0'
//using Big-Endian
unsigned char * pcSerializedInt(unsigned char * buffer, unsigned int value)
{
    buffer[3] = value >> 24;
    buffer[2] = value >> 16;
    buffer[1] = value >> 8;
    buffer[0] = value;
    return buffer + 4;
}
//using Big-Endian
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
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
