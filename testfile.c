#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#define BUFFERSIZE 1000


int main (int argc, char* argv[]){



    return 0;
}

int HaveDoubleEnter(unsigned char* pcInOutBuffer){
    /*
        Input/Output : pcInOutBuffer -> DynamicCopyBuffer(_,pcSrcBuffer,_)
        return : Has "\r\n\r\n" 1 , If not 0 
    */
    void* pvNeedle = NULL;
    pvNeedle = memmem(pcInOutBuffer,strlen(pcInOutBuffer),"\r\n\r\n", 4);
    if(pvNeedle != NULL) return 1;
    else return 0;
}
int ProcessFromHeader(char* pcInHeader, char* pcOutHostname, int* pcOutport){

    /*    
        Input  : pcInHeader == GET http://www.google.co.kr/ HTTP/1.0
                               Host : http://www.google.co.kr
                               .... \r\n\r\n\0
        Output : pcOutHostname == www.google.co.kr
            : pcOutport == 80
        return : Error==0, Success==1
    */

    if(strncmp("GET",pcInHeader,3) != 0){ //should consider (only Start of Buffer)!
        fprintf(stderr,"<GET> Command Not Exist\n");
        return 0;
    }
    void* pvNeedle = NULL;
    pvNeedle = memmem(pcInHeader,strlen(pcInHeader),"Host", 4);
    if(pvNeedle == NULL){
        fprintf(stderr,"Host Command Not Exist\n");
        return 0;
    }
    //TODO : Host, Host from GET Must Same, -> 400 Bad Request

    return 1;
}
int DynamicCopyBuffer(unsigned char* pcDestBuffer, unsigned char* pcSrcBuffer, const int nDestBuffer){

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
   int nBufferLength = strlen(pcDestBuffer);
   int nSrcLength = strlen(pcSrcBuffer);

   if ((nBufferLength+nSrcLength) > nDestBuffer*BUFFERSIZE){
       unsigned char* pcTemp = calloc(1,((nDestBuffer+1)*BUFFERSIZE) +1);
       pcTemp = strcat(pcTemp,pcDestBuffer);
       pcTemp = strcat(pcTemp,pcSrcBuffer);
       free(pcDestBuffer);
       pcDestBuffer = pcTemp;
       return nDestBuffer+1;
   }
   else{
       pcDestBuffer = strcat(pcDestBuffer,pcSrcBuffer);
       return nDestBuffer;
   }
}