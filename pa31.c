#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define rdtsc(x)      __asm__ __volatile__("rdtsc \n\t" : "=A" (*(x)))

typedef char *addrs_t;
typedef void *any_t;

addrs_t baseptr;

//Heapchecker stuff
unsigned padBytes = 0;
unsigned mallocs = 0;
unsigned frees = 0;
unsigned fails = 0;
unsigned mallocTime = 0;
unsigned freeTime = 0;
char pad[1024*1024];


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
	*(unsigned*)(baseptr) = 0; //Always marked as used, so it won't try to be merged
	*(unsigned*)(baseptr + size - 4) = ~0; //Will cause the loop to exit, but is an easy flag to catch
}

addrs_t Malloc(size_t size)
{
	/*Returns the address of a free block of the given size. If no such block exists, returns null*/
	//Start timer and check pad bytes
	unsigned necessaryPad;
	if(size%8)
	{
		necessaryPad = 8 - size % 8;
	}
	else
	{
		necessaryPad = 0;
	}
	unsigned long start, end;
	rdtsc(&start);

	//Check if size is 0
	if (!size)
	{
		//Heapcheker stuff
		rdtsc(&end);
		mallocTime += end - start;
		mallocs++;
		fails++;

		return NULL;
	}

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
	{
		//heapchecker stuff
		rdtsc(&end);
		mallocTime += end - start;
		mallocs++;
		fails++;

		return NULL;
	}

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

	//Update stuff for heapchecker
	rdtsc(&end);
	mallocTime += end - start;
	mallocs++;
	if(size < blockSize)
		necessaryPad += 8;
	pad[i - baseptr] = necessaryPad;
	padBytes += necessaryPad;

	
	return i + 4;
}

void Free(addrs_t addrs)
{
	//Heapchecker
	unsigned long start, end;
	rdtsc(&start);
	//Make sure address valid
	if (!addrs)
	{
		rdtsc(&end);
		freeTime += end - start;
		frees++;
		fails++;
		return;
	}

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
	//Heapchecker
	rdtsc(&end);
	freeTime += end - start;
	frees++;
	padBytes -= pad[addrs-4-baseptr];
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
	if (addrs)
	{
		memcpy(returnData, addrs, size);
		Free(addrs);
	}
}

void heapChecker()
{
	addrs_t i = baseptr + 4;
	unsigned allocBlocks = 0;
	unsigned allocBytes = 0;
	unsigned freeBlocks = 0;
	unsigned freeBytes = 0;
	while (*(unsigned*)(i) != ~0)
	{
		unsigned size = *(unsigned*)(i) & ~1;
		if (*(unsigned*)(i) & 1)
		{
			freeBlocks++;
			freeBytes += size - 8;
		}
		else
		{
			allocBlocks++;
			allocBytes += size - 8;
		}
		i += size;
	}
	printf("Number of allocated blocks : %d\n", allocBlocks);
	printf("Number of free blocks  : %d\n", freeBlocks);
	printf("Raw total number of bytes allocated : %d\n", allocBytes - padBytes);
	printf("Padded total number of bytes allocated : %d\n", allocBytes);
	printf("Raw total number of bytes free : %d\n", freeBytes);
	printf("Aligned total number of bytes free : %d\n", freeBytes);
	printf("Total number of Malloc requests: %d\n", mallocs);
	printf("Total number of Free requests: %d\n", frees);
	printf("Total number of request failures: %d\n", fails);
	if(mallocs)
		printf("Average clock cycles for a Malloc request: %d\n", (unsigned)(mallocTime / mallocs));
	if(frees)
		printf("Average clock cycles for a Free request: %d\n", (unsigned)(freeTime / frees));
	printf("Total clock cycles for all requests: %d\n", mallocTime + freeTime);

	/*
Number of allocated blocks : XXXX
Number of free blocks  : XXXX (discounting padding bytes)
Raw total number of bytes allocated : XXXX (which is the actual total bytes requested)
Padded total number of bytes allocated : XXXX (which is the total bytes requested plus internally fragmented blocks wasted due to padding/alignment)
Raw total number of bytes free : XXXX
Aligned total number of bytes free : XXXX (which is sizeof(M1) minus the padded total number of bytes allocated. You should account for meta-datastruqctures inside M1 also)
Total number of Malloc requests : XXXX
Total number of Free requests: XXXX
Total number of request failures: XXXX (which were unable to satisfy the allocation or de-allocation requests)
Average clock cycles for a Malloc request: XXXX
Average clock cycles for a Free request: XXXX
Total clock cycles for all requests: XXXX
	*/
}
