#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef char *addrs_t;
typedef void *any_t;

addrs_t baseptr;

void Init(size_t size)
{
	/*Defines the base address of the new heap block*/
	//Checks for heap size < 16 (Data structures take the whole heap)
	if (size <= 16)
		return;
	if (size % 8)
		size += 8 - size % 8;
	baseptr = malloc(size);

	//Set header and footer(1 represents free)
	*(unsigned*)(baseptr + 4) = (size - 8) | 1;
	*(unsigned*)(baseptr + size - 8) = (size - 8) | 1;

	//Set heap beginning and end buffer
	//TODO: Fix the buffers
	*(unsigned*)(baseptr) = 0; //Always marked as used, so it won't try to be merged
	*(unsigned*)(baseptr + size - 4) = ~0; //Will cause the loop to exit, but is an easy flag to catch
}

addrs_t Malloc(size_t size)
{
	/*Returns the address of a free block of the given size. If no such block exists, returns null*/

	//Check if size is 0
	if (!size)
		return NULL;

	//Internally fragment so stuff falls on boundries
	if (size % 8)
	{
		size += 8 - size % 8;
	}

	addrs_t i = baseptr + 4;
	//iterate over all size blocks until the first fit is found
	while (((*(unsigned*)(i))&~1) < size + 8 || !((*(unsigned*)(i)) & 1))
	{
		i += *(unsigned*)(i)&~1;
	}
	unsigned blockSize = *(unsigned*)(i)&~1; //Includes header and footer
	//Check for heap end
	if (*(unsigned*)(i) == ~0)
		return NULL;

	//Check if a block needs to be split
	if (blockSize == size + 8 || blockSize == size + 16)
	{
		//update block to show not free
		*(unsigned*)(i) = blockSize;
		*(unsigned*)(i + size) = blockSize;
	}
	//Split a block
	//Update free block footer and header
	else 
	{
		*(unsigned*)(i + blockSize - 4) = (blockSize - size - 8) | 1;
		*(unsigned*)(i + size + 8) = (blockSize - size - 8) | 1;
		//Update used block footer and header
		*(unsigned*)(i + size + 4) = (size + 8);
		*(unsigned*)(i) = (size + 8);
	}
	
	return i + 4;
}

void Free(addrs_t addrs)
{
	//Define sizes of each block to reduce the clusterfuck of pointer(and memory accesses)
	unsigned freedBlockSize = *(unsigned*)(addrs - 4);
	unsigned beforeBlockSize = *(unsigned*)(addrs - 8) & ~1;
	unsigned afterBlockSize = *(unsigned*)(addrs + freedBlockSize - 4) & ~1;

	//Same for whether blocks are free
	char beforeBlockFree = *(unsigned*)(addrs - 8) & 1;
	char afterBlockFree = *(unsigned*)(addrs + freedBlockSize - 4) & 1;

	//Save pointers too
	addrs_t beforeBlockHeader = addrs - beforeBlockSize - 4;
	addrs_t afterBlockHeader = addrs + freedBlockSize - 4;

	//Don't free after the heap
	if (*(unsigned*)(afterBlockHeader) == ~0)
		afterBlockFree = 0;

	//Check what to combine with
	if (beforeBlockFree && afterBlockFree)
	{
		//Update header and footer
		*(unsigned*)(beforeBlockHeader) = (beforeBlockSize + freedBlockSize + afterBlockSize)|1;
		*(unsigned*)(afterBlockHeader + afterBlockSize - 4) = (beforeBlockSize + freedBlockSize + afterBlockSize) | 1;
	}
	else if (beforeBlockFree)
	{
		*(unsigned*)(beforeBlockHeader) = (beforeBlockSize + freedBlockSize) | 1;
		*(unsigned*)(afterBlockHeader - 4) = (beforeBlockSize + freedBlockSize) | 1;
	}
	else if (afterBlockFree)
	{
		*(unsigned*)(addrs - 4) = (afterBlockSize + freedBlockSize) | 1;
		*(unsigned*)(afterBlockHeader - 4) = (afterBlockSize + freedBlockSize) | 1;
	}
	else
	{
		*(unsigned*)(addrs - 4) = freedBlockSize | 1;
		*(unsigned*)(afterBlockHeader - 4) = freedBlockSize | 1;
	}
}

addrs_t Put(any_t data, size_t size)
{
	addrs_t addrs = Malloc(size);
	if (addrs)
		memcpy(addrs, data, size);
	return addrs;
}

void Get(any_t returnData, addrs_t addrs, size_t size)
{
	memcpy(returnData, addrs, size);
	Free(addrs);
}

void heapChecker()
{
	addrs_t i = baseptr + 4;
	while (*(unsigned*)(i) != ~0)
	{

	}
}

/*README:
Allocated heap size includes the space used by all data structures to maintain the heap
Size in the header and footer includes the header and footer size. e.g. if i points to the header, i+size points to the header of the next
element in the heap.
Heap sizes are rounded up to the nearest multiple of 8
A heap cannot be allocated without at least 24 bytes of memory allocated. Attempts to do so will result in Init returning without
initilizing the heap
Attempts to Malloc 0 bytes will result in a null pointer return
Any free blocks of 8 bytes will be incorperated as internal fragmentation, as the free list takes the 
*/