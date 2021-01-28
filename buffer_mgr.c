#include <stdio.h>
#include <stdlib.h>

#include "buffer_mgr.h"
#include "dberror.h"
#include "storage_mgr.h"
#include "dt.h"
//change all frame array iterations later


typedef struct Frame{
    char *data;  //Pointer to data of frame
    bool dirty;  // If written to or not
    int fixNum;  // Number of fixes
    PageNumber pageNum; // Number of page stored in frame
    int nextFrame; //Used for LRU
    int prevFrame; //Used for LRU
} Frame;

//Circular Queue Implementation for FIFO/Linked List in Array for LRU
typedef struct Queue{
    int front; //Value of framePtr[] of the front
    int rear; //Value of framePtr[] of the rear
    int currSize;
    int maxSize;
    int readIO;
    int writeIO;
    Frame *framePtr; 
    SM_FileHandle *fileHandle;  
} Queue;

//Used for FIFO and LRU.
Queue * 
createQueue(int numPages, SM_FileHandle *fileHandle)  
{
    Queue *queue = (Queue *)malloc(sizeof(Queue)); //Declare for stack I suppose.
    queue->fileHandle = fileHandle;
    Frame *frameArray = (Frame *)malloc(numPages * sizeof(Frame));
    //Frame *frameArray[numPages]; //If this doesn't work, its lost in scope 
    for(int i = 0; i < numPages; i++){
        frameArray[i].dirty = false;
        frameArray[i].fixNum = 0;
        // TA's comment: in .h, we define macro NO_PAGE -1. Use it to make your code modifiable.
        frameArray[i].pageNum = NO_PAGE; 
        frameArray[i].data = (char*)calloc(PAGE_SIZE, (sizeof(char) ));  
    }
    queue->currSize = -1; //Empty
    queue->maxSize = numPages-1; //**UNSURE ABOUT THIS MAXSIZE**
    queue->front = -1;
    queue->rear =  -1;
    queue->framePtr = frameArray;
    queue->writeIO = 0;
    queue->readIO = 0;

    return queue;
}

//Typically dequeue would return the item being dequeued, but thats not necessary here.
// Just want to remove it from the queue
RC
dequeueFIFO(Queue *queue)
{
    // if No pages in queue
    if(queue->currSize == -1)
        return RC_QUEUE_EMPTY;
    if(queue->framePtr[queue->front].fixNum > 0){  //Swap algorithm to put front at next fixNum0 and rear at previous front
        int loopCount;
        for(int i = queue->front; i <= queue->maxSize ; i++ ){
            if(queue->framePtr[i].fixNum == 0){
                int temp = queue->front;
                queue->front = i;
                queue->rear = i-1;
                break;
            }
            if(i == queue->maxSize)
                i = -1;
            if(i == queue->rear)  //Only happens when every item has a fixNum
                return RC_ALL_PAGES_HAVE_FIX_COUNT;       
        }
    }
    if(queue->framePtr[queue->front].dirty == true){
        RC returnCode = writeBlock(queue->framePtr[queue->front].pageNum, queue->fileHandle, queue->framePtr[queue->front].data);
        if(returnCode != RC_OK)
            return returnCode;
        queue->writeIO++;
        queue->framePtr[queue->front].dirty = false;
    }
    queue->framePtr[queue->front].pageNum = -1;
    if(queue->currSize == 0){  //Last item in Queue
        queue->front = -1;
        queue->rear = -1;
    } else if (queue->front == (queue->maxSize)){  //Item is not full but last item in queue structure.
        queue->front = 0;
    } else {   // Item is in the middle or beginning of queue
        queue->front = queue->front + 1;
    }
    //queue->front->prevFrame = NULL; - may keep double linked list, but not needed for queue
    queue->currSize--;
    //Should free() be here?  
    
    return RC_OK;
}

//Want to add a page to the queue into a frame
//Not sure about the arguments here.
RC 
enqueueFIFO(Queue *queue, BM_PageHandle * const page)
{
    RC returnCode;
    //Queue is full, dequeue one.
    if(queue->currSize == queue->maxSize){
    
        returnCode = (dequeueFIFO(queue));
        if(returnCode != 0){  //Two options here: either dequeue one and move on (fifo), or throw an error and let the caller fix.
            return RC_QUEUE_FULL;
        }
    
    }
    
    if(queue->currSize == -1 ){  // Queue is empty
        queue->front = 0;
        queue->rear = 0;
        RC returnCode = readBlock(page->pageNum, queue->fileHandle, queue->framePtr[queue->rear].data);
        
        if(returnCode != RC_OK){
            return returnCode;
        }
        page->data = queue->framePtr[queue->rear].data;
        queue->framePtr[queue->rear].pageNum = page->pageNum;
        
    } else if (queue->rear == queue->maxSize) {  //Queue isn't full, but at end of array
        queue->rear = 0;
        RC returnCode = readBlock(page->pageNum, queue->fileHandle, queue->framePtr[queue->rear].data);
        if(returnCode != RC_OK){
            return returnCode;
        }
        page->data = queue->framePtr[queue->rear].data;
        queue->framePtr[queue->rear].pageNum = page->pageNum;
    } else {  //Queue isn't full, next slot is open
        queue->rear = queue->rear + 1;
        RC returnCode = readBlock(page->pageNum, queue->fileHandle, queue->framePtr[queue->rear].data);
        if(returnCode != RC_OK){

            return returnCode;
        }
        page->data = queue->framePtr[queue->rear].data;
        queue->framePtr[queue->rear].pageNum = page->pageNum;
    }
    queue->framePtr[queue->rear].fixNum++;
    queue->currSize++;
    queue->readIO++;
    return RC_OK;
}

//Removes page from queue, refactors.
//Front - least recently used rear - most recently used
RC
dequeueLRU(Queue *queue)
{
    if(queue->currSize == -1)
        return RC_QUEUE_EMPTY;
    else if(queue->framePtr[queue->front].fixNum > 0){ //Should only happen when every page is pinned - is maintained in pinPage
        return RC_FIX_COUNT_GREATER_THAN_ZERO;        
        }
    if(queue->framePtr[queue->front].dirty == true){
        RC returnCode = writeBlock(queue->framePtr[queue->front].pageNum, queue->fileHandle, queue->framePtr[queue->front].data);
        if(returnCode != RC_OK)
            return returnCode;
        queue->writeIO++;
        queue->framePtr[queue->front].dirty = false;
    }
    queue->framePtr[queue->front].pageNum = -1;
    if(queue->currSize == 0){
        if(queue->framePtr[queue->front].fixNum > 0)
            return RC_FIX_COUNT_GREATER_THAN_ZERO; //This error likely will never come up
        queue->front = -1;
        queue->rear = -1;
    } else{
        int temp = queue->front;
        queue->front = queue->framePtr[queue->front].nextFrame;
    }
    queue->currSize--;
    //Free here?

    return RC_OK;
}

//Not sorted due to time constraints
//Rear = most recent //Front = least recently accessed
RC
enqueueLRU(Queue *queue, BM_PageHandle * const page)
{
    if(queue->currSize == queue->maxSize){
        if(dequeueLRU(queue) != 0 ){
            return RC_QUEUE_FULL;
        }
    }
    if(queue->currSize == -1){
        queue->front = 0;
        queue->rear = 0;
        RC returnCode = readBlock(page->pageNum, queue->fileHandle, queue->framePtr[queue->rear].data);
        if (returnCode != RC_OK)
            return returnCode;
        page->data = queue->framePtr[queue->rear].data;
        queue->framePtr[queue->rear].pageNum = page->pageNum;
        queue->framePtr[queue->rear].nextFrame = -1; //Might be redundant but just throwing it in there jic.
        queue->framePtr[queue->rear].prevFrame = -1;

    } else{
        int i;
        //Linear Search to find a free space (wouldlike to change to free list when time available)
        for(i = 0; i <= queue->maxSize; i++){
            if(queue->framePtr[i].pageNum == -1){
                break;
            }
        }
        int temp = queue->rear;
        queue->rear = i;
        queue->framePtr[temp].nextFrame = queue->rear;
        queue->framePtr[queue->rear].prevFrame = temp;
        queue->framePtr[queue->rear].nextFrame = -1; //Most recently accessed points to -1.
        RC returnCode = readBlock(page->pageNum, queue->fileHandle, queue->framePtr[queue->rear].data);
        if(returnCode != RC_OK)
            return returnCode;
        page->data = queue->framePtr[queue->rear].data;
        queue->framePtr[queue->rear].pageNum = page->pageNum;
    }
    queue->readIO++;
    queue->framePtr[queue->rear].fixNum++;
    queue->currSize++;
    return RC_OK;
}

RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, 
		const int numPages, ReplacementStrategy strategy,
		void *stratData)
{

    SM_FileHandle *fileHandle = (SM_FileHandle *)malloc(sizeof(SM_FileHandle));
    bm->pageFile = (char*)pageFileName;  
    bm->numPages = numPages;
    bm->strategy = strategy;
    RC returnCode = openPageFile(bm->pageFile, fileHandle);
    if(returnCode != RC_OK)
        return returnCode;
    Queue *queue = createQueue(bm->numPages, fileHandle);
    bm->mgmtData = queue;
    return RC_OK;
    
}

RC
shutdownBufferPool(BM_BufferPool *const bm)
{
    RC returnCode; 

    Queue *queue = (Queue *) bm->mgmtData;  //Needs cast?
    for (int i = 0; i < bm->numPages; i++){
        if(queue->framePtr[i].fixNum != 0){
            return RC_SHUTDOWN_ERROR;
        }
        // TA's comment: call forceFlushPool(bm) to avoid repeating yourself.
        returnCode = forceFlushPool(bm);
        if(returnCode != forceFlushPool)
            return returnCode;
        
        // All Frame related free's should go here!
    }
    returnCode = closePageFile(queue->fileHandle);
    if(returnCode != RC_OK)
        return returnCode;
    for(int i = 0; i <= queue->maxSize;i++){
        free(queue->framePtr[i].data);
    }
    free(queue->framePtr);
    free(queue->fileHandle);
    free(queue);
    //free FH,queue,etc

    return RC_OK;

}

RC 
forceFlushPool(BM_BufferPool *const bm)
{
    Queue *queue = (Queue *) bm->mgmtData;
    for(int i = 0; i < bm->numPages; i++){
        if(queue->framePtr[i].dirty == true && queue->framePtr[i].fixNum == 0){
            RC returnCode = writeBlock(queue->framePtr[i].pageNum, queue->fileHandle, queue->framePtr[i].data);
            if(returnCode != RC_OK)
                return returnCode;
            queue->writeIO++;
            queue->framePtr[i].dirty = false;
        }
    }
    return RC_OK;
}

// Buffer Manager Interface Access Pages
RC 
markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	Queue *queue = (Queue *)bm->mgmtData;
	
	int i;
	for(i = 0; i <= queue->maxSize; i++)
	{
		if(queue->framePtr[i].pageNum == page->pageNum)
		{
			queue->framePtr[i].dirty = true;
			return RC_OK;		
		}			
	}
	return RC_PAGE_NOT_FOUND;
}

RC 
pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, 
		const PageNumber pageNum)
{
    RC returnCode;

    page->pageNum = pageNum;
    //page->data = (char*)calloc(PAGE_SIZE, (sizeof(char)));
    //******STRATEGY = FIFO*******
    if (bm->strategy == RS_FIFO){
        Queue *queue = (Queue *) bm->mgmtData;
        page->pageNum = pageNum;
        returnCode = ensureCapacity(page->pageNum, queue->fileHandle);
        if(returnCode != 0)
            return returnCode;
        //Search if it already exists in buffer
        for(int i = 0; i <= queue->currSize; i++){
            if(queue->framePtr[i].pageNum == pageNum){
                queue->framePtr[i].fixNum++;  //Queue is full, need to adjust so fix count isn't dequeued
                page->data = queue->framePtr[i].data;
                return RC_OK;
            }
        }
        //Page not already in buffer;
        returnCode = enqueueFIFO(queue, page);
            return returnCode;
    } 
    //*******STRATEGY = LRU ******
    else if (bm->strategy == RS_LRU){
        Queue *queue = (Queue *) bm->mgmtData;
        page->pageNum = pageNum;
        returnCode = ensureCapacity(page->pageNum, queue->fileHandle);
        if(returnCode != 0)
            return returnCode;
            //Search if already exists in buffer
        for(int i = 0; i <= queue->maxSize; i++){
            //Found page in buffer
            if(queue->framePtr[i].pageNum == page->pageNum){ 
                queue->framePtr[i].fixNum++; //Accessed 
                page->data = queue->framePtr[i].data;
                //Need to update most recently accessed (rear)
                if(i == queue->rear){  //Already the most recently accessed
                    return RC_OK;
                } else if (i == queue->front){ //front prev points to -1 so handles differently. 
                    queue->front = queue->framePtr[queue->front].nextFrame;
                    queue->framePtr[queue->front].prevFrame = -1;
                    int rearTemp = queue->rear;
                    queue->rear = i;
                    queue->framePtr[rearTemp].nextFrame = queue->rear;
                    queue->framePtr[queue->rear].nextFrame = -1;
                    queue->framePtr[queue->rear].prevFrame = rearTemp;
                    return RC_OK;
                } else { //Somewhere inbetween front and rear.
                    int nextTemp = queue->framePtr[i].nextFrame;
                    int prevTemp = queue->framePtr[i].prevFrame;
                    queue->framePtr[nextTemp].prevFrame = prevTemp;
                    queue->framePtr[prevTemp].nextFrame = nextTemp;
                    int rearTemp = queue->rear;
                    queue->rear = i;
                    queue->framePtr[rearTemp].nextFrame = queue->rear;
                    queue->framePtr[queue->rear].nextFrame = -1;
                    queue->framePtr[queue->rear].prevFrame = rearTemp;
                    return RC_OK;
                }
            }
        }
        //Doesn't already exist in buffer so use enqueue
        returnCode = enqueueLRU(queue, page);

            return returnCode;
        
        return RC_OK;
    }

}        

RC 
unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
    Queue *queue = (Queue*) bm->mgmtData;
    int i;

    for(i = 0; i < bm->numPages; i++){
        if (queue->framePtr[i].pageNum == page->pageNum && (queue->framePtr[i].fixNum > 0)){
            queue->framePtr[i].fixNum--;
            return RC_OK;
        }
    }
    free(page->data);

    return RC_PAGE_NOT_FOUND;
}

RC 
forcePage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
    Queue *queue = (Queue *)bm->mgmtData;

    int i;
    for(i = 0; i <= queue->maxSize; i++)
    {
        if(queue->framePtr[i].pageNum == page->pageNum)
        {
            if(writeBlock(queue->framePtr[i].pageNum, queue->fileHandle, queue->framePtr[i].data) == 0){
                queue->writeIO++;
                return RC_OK;
            } else 
                return RC_WRITE_FAILED;
        }
    }
    return RC_PAGE_NOT_FOUND;
}

// Statistics Interface
PageNumber *getFrameContents (BM_BufferPool *const bm)
{
    Queue *frames = bm->mgmtData;
    // TA's comment: queue->maxSize = numPages-1; 
    // For numPages pages, you should alloc numPages, not maxSize, which is numPages-1.
    PageNumber *result = (PageNumber *)malloc(sizeof(PageNumber)*bm->numPages);
    
    int i;
    for(i = 0; i <= frames->maxSize; i++ ){
        if(frames->framePtr[i].pageNum < 0)
            result[i] = NO_PAGE;
        else{
            result[i] = frames->framePtr[i].pageNum;
        }
    }
    
    return result;
}

bool *getDirtyFlags (BM_BufferPool *const bm)
{

    Queue *frames = bm->mgmtData;
    // TA's comment: Similarly, should be malloc(sizeof(bool)*bm->numPages);
    bool *result = (bool *)malloc(sizeof(bool)*bm->numPages);
    
    int i;
    for(i = 0; i <= frames->maxSize; i++){
        if(frames->framePtr[i].dirty == true){
            result[i] = TRUE;
        } else
            result[i] = FALSE;
        
    }
    
    return result;
}

int *getFixCounts (BM_BufferPool *const bm)
{
    Queue *frames = bm->mgmtData;
    // TA's comment: Similarly, should be malloc(sizeof(int)*bm->numPages);
    int *result = (int *)malloc(sizeof(int)*bm->numPages);
    
    int i;
    for( i = 0; i <= frames->maxSize; i++){
        result[i] = frames->framePtr[i].fixNum;
    }
    
    return result;
}
int getNumReadIO (BM_BufferPool *const bm)
{
    Queue *queue = bm->mgmtData;
    return queue->readIO;
}

int getNumWriteIO (BM_BufferPool *const bm)
{
    Queue *queue = bm->mgmtData;
    return queue->writeIO;
}