## README
===========
### CS 525 - Summer 2020
### Assignment 2
#### by
### Chris Wszolek, Lingxiao Zhu, Mert Kocagil

### To Run
==========

1. In terminal, type: "$make test_buffer1"
2. In terminal, type: "$./test_buffer1"

If necessary, "$make clean" is an option.

### Functions
==========
For this assignment, we chose to closely follow the interace design that was provided by
the project helper. We also use mgmtData to point to a queue struct and relevant queue functions are added.

We chose to make a buffer_mgr.c file that would hold the methods within the buffer_mgr.h file.
Below we list the functions used in buffer_mgr.c.

Implimentations:
FIFO = set within a circular queue structure.  Front and Rear are maintained by index number.
LRU = Double Linked List set in a Queue.  Not the best inplimentation, but easy to impliment.  Front and rear
are maintained by index numbers to the array, and can iterate through list via next and prev (next goes to the
more recent direction, prev goes in the less recent direction)

**Note** Because test doesn't call free() on statistic methods, this will leak a small amount of memory.  Fixed most.

* createQueue()

Returns queue to be used by both FIFO and LRU replacement strategies. Returns Queue if successful.

* enqueueFIFO()

Adds a page to FIFO queue.  If queue is full calls dequeue to remove a page.
Behaves appropriately based on if queue is empty, inserting page at end
of queue, or not.  Sets page added to rear of queue.
Returns RC_OK if successful.

* dequeueFIFO()

Removes page from FIFO queue based on when it was enqueued.  Will remove page that is at front and
set the next page to front.  If item has fixNum > 0, will find next item that has fixNum = 0 and
set this as the new front, and the item after it as the new rear.
If queue is empty returns RC_QUEUE_EMPTY, otherwise returns RC_OK.

*  enqueueLRU()

Adds a page to LRU queue.  Sets new item as the most recently accessed.  If queue is full, will call
dequeue to remove a page. Uses linear search to find free space in array.
Uses index based linked array structure and will appropriately manage linked array items.
Checks if queue is full and cannot be removed and returns RC_QUEUE_FULL if so. Otherwise returns RC_OK.

*  dequeueLRU()

Removes page from LRU Linked Array List.  Maintains the indexed based linked array structure when removing page from list.
If queue is empty returns RC_QUEUE_EMPTY.  Will return RC_FIX_COUNT_GREATER_THAN_ZERO if
every page is pinned. Otherwise returns RC_OK.

*  initBufferPool()

Creates a buffer pool. Returns RC_OK if successful.

*  shutdownBufferPool()

Closes the buffer pool and removes all pages from memory to free memory. Returns RC_OK if successful.

*  forceFlushPool()

Dirty pages get written to disk. Returns RC_OK if successful.

*  markDirty()

Indicates if a page has been changed and labels it as dirty. Returns RC_OK if successful.

*  pinPage()

Pin page will put a page in "pinned" status by increasing fixNum by one, stopping it from being removed from the queue.  
Pin implimentation varies based on if queue is using strategy FIFO, or LRU.  Will search in data struct if it contains the page number,
and if it does will increase its fixNum.  In LRU, this will send it to the end of the link list or the rear, which keeps
the least recently accessed order.  In FIFO, fixNum is adjusted
within the dequeueFIFO method.  If not found, will enqueue the page into the buffer pool.

*  unpinPage()

Unpin page will search through buffer pools to find pool with matching page number.  If found, will decrease
fixNum by 1.

*  forcePage()

Will search through buffer pools to find pool with matching page number. If found, will force a write to disk.

*  getFrameContents()

Will return a pointer to an array that contains the page numbers kept in the frames of the buffer pools.  
Returns NO_PAGE on empty.  **Note** Caller must use free() once done with pointer, otherwise will leak memory.

*  getDirtyFlags()
Will return a pointer to an array that contains the dirty flag status kept in the frames of the buffer pools.  
TRUE if dirty, FALSE if clean.  **Note** Caller must use free() once done with pointer, otherwise will leak memory.

*  getFixCounts()

Will return a pointer to an array that contains the fixNum of the frames in the buffer pools.
**Note** Caller must use free() once done with pointer, otherwise will leak memory.


*  getNumReadIO()

Counts pages read from disk and returns the count.

*  getNumWriteIO()

Counts pages written to page file and returns the count.

