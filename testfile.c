#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#define BUFFERSIZE 3
#define MAXURL 2048
#define REDIRECTIONURL "http://warning.or.kr"

int HaveDoubleEnter(unsigned char* pcInOutBuffer);
int DynamicCopyBuffer(unsigned char** pcDestBuffer, unsigned char* pcSrcBuffer, const int nDestBuffer);
int ProcessFromHeader(unsigned char* pcInHeader, unsigned char* pcOutHeader, unsigned char* pcOutHostname, int* pnOutport, unsigned char**pcInBlackList, int nBlackList);
int IsRedirection(unsigned char* pcInputHost, unsigned char** Blacklist, int nBlackList);
unsigned char** FileToList(int* nListSize);

int main (int argc, char* argv[]){


    unsigned char* pcTestText[] = {"GET"," ht","tp:","//w","ww.","kis","t.r","e.k","r/k","ist","_we","b/r","eso","urc","e/i","mag","e/c","omm","on/","foo","ter","_lo","go.","png"," HT","TP/","1.0","\r\nH","ost",": w","ww.","kis","t.r","e.k","r\r\n","\r\n"};
    unsigned char* pcCopiedBuffer = calloc(1,BUFFERSIZE+1);
    int hasDoubleEnter;
    int nDestBuffer = 1;
    int i =0;

    int nBlackList = 0;
    unsigned char** pcBlackList = FileToList(&nBlackList);

    do{
        nDestBuffer = DynamicCopyBuffer(&pcCopiedBuffer,pcTestText[i], nDestBuffer);
        i++;
    }
    while(!HaveDoubleEnter(pcCopiedBuffer));
    
    unsigned char* pcHost = calloc((MAXURL+1),1);
    int nPort = 0;
    int isError = 0;

    unsigned char* pcSendBuffer = calloc(1,strlen(pcCopiedBuffer)+1);
    isError = ProcessFromHeader(pcCopiedBuffer,pcSendBuffer,pcHost,&nPort, pcBlackList, nBlackList);

    //int isRedirection = IsRedirection(pcHost,pcBlackList,nBlackList);

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
int ProcessFromHeader(unsigned char* pcInHeader, unsigned char* pcOutHeader, unsigned char* pcOutHostname, int* pnOutport, unsigned char**pcInBlackList, int nBlackList){

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

    if(strncmp("GET ",pcInHeader,4) != 0)//TODO : space
    { 
        fprintf(stderr,"<GET> Command Not Exist\n");
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
        fprintf(stderr,"GET url Not Exist\n");
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
                fprintf(stderr,"<GET> Command Not Exist\n");
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
        fprintf(stderr,"HTTP Command Not Exist\n");
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
        fprintf(stderr,"Host Command Not Exist\n");
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
        fprintf(stderr,"Host : Not Exist\n");
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
        fprintf(stderr,"503 Service Unavailable\n");
        return 503;
    }
    else if (strncmp(pcURLFromCommand,pcHostNameStart,nHostLengthFromCommand) != 0)
    {
        fprintf(stderr,"400 Bad Request\n");
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
        strcat(pcOutHeader,"/ HTTP/1.0\nHost: ");
        strcat(pcOutHeader,REDIRECTIONURL);
        strcat(pcOutHeader,"\r\n\r\n");  
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
        strcat(pcOutHeader," HTTP/1.0\nHost: ");
        strcat(pcOutHeader,pcOutHostname);
        strcat(pcOutHeader,"\r\n\r\n");    
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

unsigned char** FileToList(int* nListSize)
{
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
            fprintf(stderr, "BlackList format Error\n");
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