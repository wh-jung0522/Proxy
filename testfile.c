#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#define BUFFERSIZE 100
#define MAXURL 2048

int HaveDoubleEnter(unsigned char* pcInOutBuffer);
int DynamicCopyBuffer(unsigned char** pcDestBuffer, unsigned char* pcSrcBuffer, const int nDestBuffer);
int ProcessFromHeader(unsigned char* pcInHeader, unsigned char* pcOutHeader, unsigned char* pcOutHostname, int* pnOutport);

int main (int argc, char* argv[]){


    unsigned char* pcTestText[] = {"GET http://www.google.com:79/ HTTP/1.0\nHost: www.google.com\r\n\r\n"};
    unsigned char* pcCopiedBuffer = calloc(1,BUFFERSIZE+1);
    int hasDoubleEnter;
    int nDestBuffer = 1;
    int i =0;

    do{
        nDestBuffer = DynamicCopyBuffer(&pcCopiedBuffer,pcTestText[i], nDestBuffer);
    }
    while(!HaveDoubleEnter(pcTestText[i++]));
    char* pcHost = calloc(1,MAXURL+1); 
    int nPort = 0;
    int isError = 0;

    unsigned char* pcSendBuffer = calloc(1,strlen(pcCopiedBuffer)+1);
    isError = ProcessFromHeader(pcCopiedBuffer,pcSendBuffer,pcHost,&nPort);


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

    unsigned char* pvHostNeedle = NULL;
    pvHostNeedle = strstr(pcInHeader,"Host");
    if(pvHostNeedle == NULL){
        fprintf(stderr,"Host Command Not Exist\n");
        return 0;
    }
    //TODO : Host, Host from GET Must Same, -> 400 Bad Request
    
    unsigned char* pcTempFromCommand = strstr(pcInHeader, "http://");
    if(pcTempFromCommand == NULL){
        fprintf(stderr,"GET url Not Exist\n");
        return 0;
    }
    pcTempFromCommand+=7;//strlen("http://") == 7
    unsigned char* pcTempFromCommandEnd = strpbrk(pcTempFromCommand, ":/");
    int nUrlLength = pcTempFromCommandEnd-pcTempFromCommand; //without '/'

    unsigned char* pcTempFromHost = strchr(pvHostNeedle, ' ');
    if(pcTempFromHost == NULL){
        fprintf(stderr,"HOST url Not Exist\n");
        return 0;
    }
    pcTempFromHost+=1;
    unsigned char* pcTempFromHostEnd = strstr(pcTempFromCommand, "\r\n");
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
        pcTempFromCommandEnd = strchr(pcTempFromCommandEnd,  '/');
    }

    strncat(pcOutHeader,pcInHeader,4);// "GET " 
    strcat(pcOutHeader,pcTempFromCommandEnd);// "/ HTTP/1.0 .... \0"

    return 1;
}