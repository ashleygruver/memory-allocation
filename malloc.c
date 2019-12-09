#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef char *addrs_t;
typedef void *any_t;

const int headerSize = 4;
addrs_t baseptr;

void Init(size_t size)
{
	/*Defines the base address of the new heap block*/
	//TODO: check for size = 0 and size % 8 != 0
	baseptr = malloc(size);

	//Set header and footer(1 represents free)
	*(unsigned*)(baseptr + headerSize) = (size - 2 * headerSize) | 1;
	*(unsigned*)(baseptr + size - 2 * headerSize) = (size - 2 * headerSize) | 1;

	//Set heap beginning and end buffer
	*(unsigned*)(baseptr) = 0; //Always marked as used, so it won't try to be merged
	*(unsigned*)(baseptr + size - headerSize) = ~0; //Will cause the loop to exit, but is an easy flag to catch
}

addrs_t Malloc(size_t size)
{
	/*Returns the address of a free block of the given size. If no such block exists, returns null*/

	//TODO: Decide if we want more internal fragmentation and do it if we do

	//Internally fragment so stuff falls on boundries
	size += 8 - size % 8;

	char* i = baseptr + headerSize;
	//iterate over all size blocks until the first fit is found
	while ((*(unsigned*)(i))&~1 < size - 2 * headerSize || !((*(unsigned*)(i)) & 1))
	{
		i += *(unsigned*)(i)&~1;
	}
	unsigned blockSize = *(unsigned*)(i)&~1; //Includes header and footer
	//Check for heap end
	if (*(unsigned*)(i) == ~0)
		return NULL;

	//Check if a block needs to be split
	if (blockSize == size)
	{
		//update block to show not free
		*(unsigned*)(i) = blockSize;
		*(unsigned*)(i+size) = blockSize;
	}
	//Split a block
	//Update free block footer and header
	*(unsigned*)(i + blockSize - headerSize) = (blockSize - size) | 1;
	*(unsigned*)(i + size + 2 * headerSize) = (blockSize - size) | 1;
	//Update used block footer and header
	*(unsigned*)(i + size + headerSize) = (size + 2 * headerSize);
	*(unsigned*)(i) = (size + 2 * headerSize);
	
	return i + headerSize;
}

void Free(addrs_t addrs)
{
	//Define sizes of each block to reduce the clusterfuck of pointer(and memory accesses)
	unsigned freedBlockSize = *(unsigned*)(addrs - headerSize);
	unsigned beforeBlockSize = *(unsigned*)(addrs - 2*headerSize) & ~1;
	unsigned afterBlockSize = *(unsigned*)(addrs + freedBlockSize - headerSize) & ~1;

	//Same for whether blocks are free
	char beforeBlockFree = *(unsigned*)(addrs - 2*headerSize) & 1;
	char afterBlockFree = *(unsigned*)(addrs + freedBlockSize - headerSize) & 1;

	//Save pointers too
	addrs_t beforeBlockHeader = addrs - beforeBlockSize - headerSize;
	addrs_t afterBlockHeader = addrs + freedBlockSize - headerSize;

	//Check what to combine with
	if (beforeBlockFree && afterBlockFree)
	{
		//Update header and footer
		*(unsigned*)(beforeBlockHeader) = (beforeBlockSize + freedBlockSize + afterBlockSize)|1;
		*(unsigned*)(afterBlockHeader + afterBlockSize - headerSize) = (beforeBlockSize + freedBlockSize + afterBlockSize) | 1;
	}
	else if (beforeBlockFree)
	{
		*(unsigned*)(beforeBlockHeader) = (beforeBlockSize + freedBlockSize) | 1;
		*(unsigned*)(afterBlockHeader - headerSize) = (beforeBlockSize + freedBlockSize) | 1;
	}
	else if (afterBlockFree)
	{
		*(unsigned*)(addrs - headerSize) = (beforeBlockSize + freedBlockSize) | 1;
		*(unsigned*)(afterBlockHeader - headerSize) = (beforeBlockSize + freedBlockSize) | 1;
	}
	else
	{
		*(unsigned*)(addrs - headerSize) = *((unsigned*)(addrs - headerSize)) & 1;
		*(unsigned*)(afterBlockHeader - headerSize) = *((unsigned*)(addrs - headerSize)) & 1;
	}
}

addrs_t Put(any_t data, size_t size)
{
	addrs_t addrs = Malloc(size);
	memcpy(data, addrs, size);
	return addrs;
}

void Get(any_t returnData, addrs_t addrs, size_t size)
{
	memcpy(returnData, addrs, size);
	Free(addrs);
}

void main(int argc, char **argv) 
{
	Init(1048576);
	addrs_t a = Malloc(0);
	addrs_t b = Malloc(1);
	Free(b);
	Free(a);
}
/*README:
Allocated heap size includes the space used by all data structures to maintain the heap
Size in the header and footer includes the header and footer size. e.g. if i points to the header, i+size points to the header of the next
element in the heap.
*/