#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#define BUFFERSIZE 3
#define MAXURL 2048
#define REDIRECTIONURL "http://warning.or.kr"

int HaveDoubleEnter(unsigned char* pcInOutBuffer);
int DynamicCopyBuffer(unsigned char** pcDestBuffer, unsigned char* pcSrcBuffer, const int nDestBuffer);
int ProcessFromHeader(unsigned char* pcInHeader, unsigned char* pcOutHeader, unsigned char* pcOutHostname, int* pnOutport);
int IsRedirection(unsigned char* pcInputHost, unsigned char** Blacklist, int nBlackList);
int FileToList(unsigned char** InBlacklist);

int main (int argc, char* argv[]){


    unsigned char* pcTestText[] = {"GET http://www.kist.re.kr/kist_web/resource/image/common/footer_logo.png HTTP/1.0\r\nHost: www.kist.re.kr\r\n\r\n"};
    unsigned char* pcCopiedBuffer = calloc(1,BUFFERSIZE+1);
    int hasDoubleEnter;
    int nDestBuffer = 1;
    int i =0;

    do{
        nDestBuffer = DynamicCopyBuffer(&pcCopiedBuffer,pcTestText[i], nDestBuffer);
    }
    while(!HaveDoubleEnter(pcTestText[i++]));
    
    unsigned char* pcHost = calloc((MAXURL+1),1);
    int nPort = 0;
    int isError = 0;

    unsigned char* pcSendBuffer = calloc(1,strlen(pcCopiedBuffer)+1);
    isError = ProcessFromHeader(pcCopiedBuffer,pcSendBuffer,pcHost,&nPort);

    unsigned char** pcBlackList = calloc(sizeof(unsigned char*),BUFFERSIZE);
    int nListSize = FileToList(pcBlackList);
    int isRedirection = IsRedirection(pcHost,pcBlackList,nListSize);

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
       unsigned char* pcTemp = calloc(1,((nDestBuffer+1)*BUFFERSIZE) +1);// Memory Leak When Allocated 7 However 107 Data
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
        return 400;
    }


    unsigned char* pcHostNeedle = NULL;
    pcHostNeedle = strstr(pcInHeader,"Host");
    if(pcHostNeedle == NULL){
        fprintf(stderr,"Host Command Not Exist\n");
        return 400;
    }

    unsigned char* pcHTTPNeedle = NULL;
    pcHTTPNeedle = strstr(pcInHeader,"HTTP");
    if(strncmp(pcHTTPNeedle,"HTTP/1.0",8)!=0){
        fprintf(stderr,"Host Command Not Exist\n");
        return 400;
    }


    unsigned char* pcTempFromCommand = strstr(pcInHeader, "http://");
    if(pcTempFromCommand == NULL){
        fprintf(stderr,"GET url Not Exist\n");
        return 503;
    }
    pcTempFromCommand+=7;//strlen("http://") == 7
    unsigned char* pcTempFromCommandEnd = strpbrk(pcTempFromCommand, ":/ ");
    int nUrlLength = pcTempFromCommandEnd-pcTempFromCommand; //without '/'

    unsigned char* pcTempFromHost = strchr(pcHostNeedle, ' ');
    if(pcTempFromHost == NULL){
        fprintf(stderr,"HOST url Not Exist\n");
        return 503;
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
        fprintf(stderr,"503 Service Unavailable\n");
        return 503;
    }
    else if (strncmp(pcTempFromHost,pcTempFromCommand,nUrlLength) != 0)
    {
        fprintf(stderr,"400 Bad Request\n");
        return 400;
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
        strncat(pcOutHeader,pcTempFromHost,nUrlLength);
        strcat(pcOutHeader+nUrlLength,pcTempFromHost+nUrlLength);
    }
    
    return 0;
}


int IsRedirection(unsigned char* pcInputHost, unsigned char** Blacklist, int nBlackList)
{
    for(int i=0;i<nBlackList;i++)
    {
        if(strcmp(pcInputHost,Blacklist[i])==0)
        {
            return 1;
        }
    }
    return 0;
}

int FileToList(unsigned char** InBlacklist)
{
    int nBlackList = 0;
    int nMulBlacklist = 1;
    unsigned char** TempBlackList = NULL;
    unsigned char* pcReadLine = calloc(1,MAXURL+1);
    while((fgets(pcReadLine,MAXURL,stdin)!= NULL))
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
        InBlacklist[nBlackList-1] = calloc(1,strlen(pcReadLine)+1);
        strcpy(InBlacklist[nBlackList-1],pcReadLine);
        memset(pcReadLine,0,MAXURL+1);
    }

    return nBlackList;
}