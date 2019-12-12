#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define rdtsc(x)      __asm__ __volatile__("rdtsc \n\t" : "=A" (*(x)))

typedef char *addrs_t;
typedef void *any_t;

//Heapchecker stuff
unsigned padBytes = 0;
unsigned mallocs = 0;
unsigned frees = 0;
unsigned fails = 0;
unsigned mallocTime = 0;
unsigned freeTime = 0;
char pad[65536];

addrs_t baseptr;
addrs_t redir_tabl[65536];

void VInit(size_t size)
{
	//Make [size] a multiple of 8 for neatness
	if(!!(size % 8))
		size += 8 - size % 8;

	baseptr = malloc(size + 16);
	
	//Counts the number of table entries
	*(unsigned*)(baseptr) = 0;

	//Stores the size of the given space
	*(unsigned*)(baseptr + 4) = size + 16;

	//Stores the address where free space begins
	*(addrs_t*)(baseptr + 8) = baseptr + 16;
}

addrs_t* VMalloc(size_t size)
{
	//TODO: Have things be put into the redirection table nicely (non-linear array traversal)
  
  //Initiate timer, record padded bytes
	unsigned necessaryPad;
	if(size % 8)
		necessaryPad = 8 - size % 8;
	else
		necessaryPad = 0;
  
  unsigned long start, end;
	rdtsc(&start);

	//We do not desire to allocate zero bytes
	if (!size)
  {
    //Update HC info
    rdtsc(&end);
		mallocTime += end - start;
		mallocs++;
		fails++;

		return NULL;
  }

	int index = 0;
	while (!!redir_tabl[index] && index < 65536)
		index++;

	//If the table appears to be full, we return null.
	if (index == 65536)
  {
		//Update HC info
    rdtsc(&end);
		mallocTime += end - start;
		mallocs++;
		fails++;

    return NULL;
  }

	//Pad requested size to be a multiple of 8
	if(!!(size % 8))
		size += 8 - size % 8;
	
	//Can't go over that size limit!
	if(*(addrs_t*)(baseptr + 8) + size > baseptr + *(unsigned*)(baseptr + 4))
  {
		//Update HC info
    rdtsc(&end);
		mallocTime += end - start;
		mallocs++;
		fails++;
    
    return NULL;
  }
	
	//Assuming that none of the bad stuff happened, we can allocate the appropriate space, and then update [freeptr] accordingly.
	redir_tabl[index] = *(addrs_t*)(baseptr + 8);
	*(addrs_t*)(baseptr + 8) += size;

	//As we've added a new element, the size of things in the table grows by one.
	*(unsigned*)(baseptr) += 1;

  //Update HC info
  rdtsc(&end);
	mallocTime += end - start;
	mallocs++;
  pad[index] = necessaryPad;
	padBytes += necessaryPad;

	return &redir_tabl[index];
}

void VFree(addrs_t* addr) //[addr] is like the index in the table
{
  //Initiate timer
	unsigned long start, end;
	rdtsc(&start);

	//Can't free anything if there's nothing there
	if(!*(unsigned*)(baseptr))
  {
		//Update HC info
    rdtsc(&end);
		freeTime += end - start;
		frees++;
		fails++;

    return;
  }

	//Address out of bounds
	if(!addr || addr > &redir_tabl[65535] || addr < &redir_tabl[0])
	{
		//Update HC info
		rdtsc(&end);
		freeTime += end - start;
		frees++;
		fails++;
		
		return;
	}
	
	//Need to make something that gets all the addresses that need to be adjusted
	int need_shifting[*(unsigned*)(baseptr)];
	int i = 0;
	for(i = 0; i < *(unsigned*)(baseptr); i++)
		need_shifting[i] = 0;

	//Also somehow need to calculate the size of the memory that is to be freed, so that's what's [upper_bound] is for
	addrs_t upper_bound = NULL;

	/* Loop variables:
	  [index] is for all table values
	  [count] is for all non-NULL table values
	  [shifted] is for all table values that need to be tweaked after
	  [curr] is merely one of those save call variable things*/
	int count = 0, index = 0, shifted = 0;
	addrs_t curr;

  	int addr_index = -1;
  
	while(count < *(unsigned*)(baseptr) && index < 65536)
	{
    	curr = redir_tabl[index];
		if(curr)
		{
			if(curr > *addr) //Means [curr] needs to be adjusted
			{
        		need_shifting[shifted++] = index;
				if (!upper_bound || curr < upper_bound)
        			{
					upper_bound = curr;
        			}	
			}
			if(curr == *addr)
				addr_index = index;
			count++;
		}
		index++;
	}

	//Two scenarios: the block isn't/is already at the end of the heap
	if(upper_bound) //Isn't last
	{
		unsigned size = upper_bound - *addr;

		int block = 0;
		while(block <= 65536 && need_shifting[block])
			redir_tabl[need_shifting[block++]] -= size;
      //This *should* move a block over per iteration

		*(addrs_t*)(baseptr + 8) -= size;
	}
	else //Is last, assign free space to given address
		*(addrs_t*)(baseptr + 8) = *addr;

	//Vacate the table spot
	*addr = NULL;

	//Block lost, decrement
	*(unsigned*)(baseptr) -= 1;

  //Update HC info
  rdtsc(&end);
	freeTime += end - start;
	frees++;

  padBytes -= pad[addr_index];
  pad[addr_index] = 0;
}

addrs_t* VPut(any_t data, size_t size)
{
	addrs_t* addr = VMalloc(size);
	if(addr)
		memcpy(*addr, data, size);
	return addr;
}

void VGet(any_t return_data, addrs_t* addr, size_t size)
{
	if(addr)
  {
    memcpy(return_data, *addr, size);
	  VFree(addr);
  }
}

void heapChecker()
{
	unsigned allocBlocks = *(unsigned*)(baseptr);
	unsigned allocBytes = *(addrs_t*)(baseptr + 8) - baseptr;
	unsigned freeBlocks = 0, freeBytes = 0;
  //Check if the allocated blocks occupy the whole space
  if(allocBytes < *(unsigned*)(baseptr + 4))
  {
    //Discount data structures size (which is 16 bytes)
    freeBytes = *(unsigned*)(baseptr + 4) - allocBytes;
    freeBlocks++;
  }
  //Discount data structures size
  allocBytes -= 16;

	printf("--- HEAP CHECK ---\n");
  printf("Number of allocated blocks : %d\n", allocBlocks);
	printf("Number of free blocks  : %d\n", freeBlocks);
	printf("Raw total number of bytes allocated : %d\n", allocBytes - padBytes);
	printf("Padded total number of bytes allocated : %d\n", allocBytes);
	printf("Raw total number of bytes free : %d\n", freeBytes);
	printf("Aligned total number of bytes free : %d\n", freeBytes);
	printf("Total number of VMalloc requests: %d\n", mallocs);
	printf("Total number of VFree requests: %d\n", frees);
	printf("Total number of request failures: %d\n", fails);
	if(mallocs)
		printf("Average clock cycles for a VMalloc request: %d\n", (unsigned)(mallocTime / mallocs));
	if(frees)
		printf("Average clock cycles for a VFree request: %d\n", (unsigned)(freeTime / frees));
	printf("Total clock cycles for all requests: %d\n", mallocTime + freeTime);

}
/*
int main(char* args, int arg)
{
	VInit(32);
	addrs_t* a = VMalloc(1);
	addrs_t* b = VMalloc(7);
	//heapChecker();
	VFree(a);
	VFree(b);
	heapChecker();

}
*/
