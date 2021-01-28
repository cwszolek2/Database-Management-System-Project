#include<stdio.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>
#include<math.h>

#include "storage_mgr.h"

//NOTE: post submission edits are marked with ** in comments

FILE *pFile;

extern void initStorageManager (void) {
	printf("CS 525 - Assignment Two\n");
    printf("by Group 3:Chris, Mert, Lingxiao\n");
}

extern RC createPageFile (char *fileName) {
	pFile = fopen(fileName, "w+");
	
    //TA's comment: Memory leak here. calloc is allocating memory too.

	SM_PageHandle eBlock = (SM_PageHandle)calloc(PAGE_SIZE,sizeof(char));

	if(fwrite(eBlock,sizeof(char),PAGE_SIZE,pFile) >= PAGE_SIZE) {
            free(eBlock);

			return RC_OK;
	}

    //**Free added to correct memory loss.    
    
	free(eBlock);
    return RC_FILE_HANDLE_NOT_INIT;
}

extern RC openPageFile (char *fileName, SM_FileHandle *fHandle) {
	
	pFile = fopen(fileName, "r+");

    
    if(pFile == NULL) {
        return RC_FILE_NOT_FOUND;
    }

    else{
        fHandle->fileName = fileName;
    
        fHandle->curPagePos = 0;
        fHandle->mgmtInfo = pFile;
    
    
        fseek(pFile,0L,SEEK_END);
    
        int total = ftell(pFile);
        fHandle->totalNumPages =  total/ PAGE_SIZE;
        fseek(pFile, 0L, SEEK_SET);

    //TA's comment: Why close file here? Although your code passed the test, it is not right.
    //**Removing close here
    //fclose(pFile);

        return RC_OK;
    }

}
extern RC closePageFile (SM_FileHandle *fHandle) {

    //TA's comment: fclose() should be used here.

	if(pFile != NULL)
		fclose(pFile);
        //fclose(fHandle->mgmtInfo);
        //**Unsure I need the below line.
        pFile = NULL;
    	
	return RC_OK; 
}


extern RC destroyPageFile (char *fileName) {

	//TA's comment: What if your file does not exist? Return RC_FILE_NOT_FOUND
    //**Adding error check here
    if(access(fileName, F_OK) == -1)
    {
        return RC_FILE_NOT_FOUND;
    } else { 
	    remove(fileName);
	    return RC_OK;
    }
}

RC 
readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    int size;

    if(fHandle->fileName == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;    
    }        
    //makking sure pageNum is a valid page number
    if((pageNum + 1) > fHandle->totalNumPages || pageNum < 0)
    {
        return RC_READ_NON_EXISTING_PAGE;
    }
    if(fseek(fHandle->mgmtInfo, pageNum*PAGE_SIZE, SEEK_SET) != 0)
        return RC_OTHER_READ_FAIL;

    size = fread(memPage, sizeof(char), PAGE_SIZE, fHandle->mgmtInfo);
    if(size > PAGE_SIZE || size < PAGE_SIZE)
        return RC_READ_FAILED; 
    //Maybe handle fread error with size?  Unsure its necessary.
    //TA's comment: You forget to update fHandle->curPagePos.
    //**Added update to pageNum;
    fHandle->curPagePos = pageNum;

    return RC_OK; 
}

//Argument is an initialized SM_FileHandle
//If curPagePos is invalid returns -1
//If Valid returns the current page position
int 
getBlockPos (SM_FileHandle *fHandle)
{
    if(fHandle->fileName == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;    
    }    

    return fHandle->curPagePos;
}

RC 
readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    //readBlock(0, fHandle, mempage) <- this is an option, but probably less efficient
    //TA's comment: readBlock(0, fHandle, mempage) is better, you are repeating yourself.
    int size; 
    RC returnCode;
    //** changing readFirstBlock here
    returnCode = readBlock(0,fHandle, memPage);

    return returnCode;
}

RC 
readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    //TA's comment: call readBlock() is easier, isn't it? 
    // Same comment for the following methods.

    int size;
    RC returnCode;

    returnCode = readBlock(fHandle->curPagePos-1, fHandle, memPage);

    return returnCode;
    //First need to go to previous block in disk and then read
    //TA's comment: fseek() input is byte-wise. You need to multiply PAGE_SIZE. 
    //Same comment for the following methods. Your readBlock() is correct though.
    //Would be nice to have a read error RC, but this will do
    //TA's comment: You can define your own error code if needed.   
}

RC 
readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    int size;
    RC returnCode;

    returnCode = readBlock(fHandle->curPagePos, fHandle, memPage);

    return returnCode;
}

RC 
readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    int size;
    RC returnCode;

    returnCode = readBlock(fHandle->curPagePos+1, fHandle, memPage);

    return returnCode;
}

RC 
readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    int size;
    RC returnCode;

    returnCode = readBlock(fHandle->totalNumPages-1, fHandle, memPage);

    return returnCode;
}


extern RC writeBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    //TA's comment: Checked RC_FILE_HANDLE_NOT_INIT?
    //**
    if(fHandle == NULL){
        return RC_FILE_HANDLE_NOT_INIT;
    }
	if( fseek(fHandle->mgmtInfo, (pageNum * PAGE_SIZE), SEEK_SET) != 0) {
        return RC_WRITE_FAILED;
	} else {
        if( fwrite(memPage, sizeof(char), strlen(memPage), fHandle->mgmtInfo) != strlen(memPage)){
            return RC_WRITE_FAILED;
        }
		fHandle->curPagePos = pageNum;     
	}	
	
	return RC_OK;
}

extern RC writeCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{

    int curPageNo = fHandle->curPagePos / PAGE_SIZE;
    RC returnCode = writeBlock(curPageNo, fHandle, memPage);

	//TA's comment: Why add totalNumPages here?

    return returnCode;
}

extern RC appendEmptyBlock (SM_FileHandle *fHandle)
{
    int writesize;
    SM_PageHandle *emptyBlock;

    if(fHandle->fileName==NULL)
        return RC_FILE_HANDLE_NOT_INIT;
    //Make a string of size PAGE_SIZE of 0's.
    emptyBlock = (SM_PageHandle*) calloc(PAGE_SIZE, sizeof(char));
    //Go to end of file, and write empty block into it
    fseek(fHandle->mgmtInfo, 0, SEEK_END);
    writesize = fwrite(emptyBlock, 1, PAGE_SIZE, fHandle->mgmtInfo);
    //Ensure write worked properly.
    if(writesize > PAGE_SIZE || writesize < PAGE_SIZE)
    {
        free(emptyBlock);
        return RC_WRITE_FAILED;
    }
    //Increase the capacity of file for 1 page;
    
    fHandle->totalNumPages++;
    free(emptyBlock);

    return RC_OK;
}

extern RC ensureCapacity (int numberOfPages, SM_FileHandle *fHandle)
{
    int pageDiff;

    ;
    if(fHandle->fileName==NULL)
        return RC_FILE_HANDLE_NOT_INIT;
    //File is already large enough    
    if(fHandle->totalNumPages > numberOfPages){
        return RC_OK;
    } else {

        pageDiff = numberOfPages - fHandle->totalNumPages;
        for(int i = 0; i <= pageDiff; i++){
            //attempt to append empty blocks - check if fails
            if(appendEmptyBlock(fHandle) != 0)
                return RC_WRITE_FAILED;
        }
    }
    return RC_OK;
}


