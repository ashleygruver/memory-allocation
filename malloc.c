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
	//Checks for heap size < 16 (Data structures take the whole heap)
	if (size <= 16)
		return;
	if (size % 8)
		size += 8 - size % 8;
	baseptr = malloc(size);

	//Set header and footer(1 represents free)
	*(unsigned*)(baseptr + headerSize) = (size - 2 * headerSize) | 1;
	*(unsigned*)(baseptr + size - 2 * headerSize) = (size - 2 * headerSize) | 1;

	//Set heap beginning and end buffer
	//TODO: Fix the buffers
	*(unsigned*)(baseptr) = 0; //Always marked as used, so it won't try to be merged
	*(unsigned*)(baseptr + size - headerSize) = ~0; //Will cause the loop to exit, but is an easy flag to catch
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

	addrs_t i = baseptr + headerSize;
	//iterate over all size blocks until the first fit is found
	while (((*(unsigned*)(i))&~1) < size + 2 * headerSize || !((*(unsigned*)(i)) & 1))
	{
		i += *(unsigned*)(i)&~1;
	}
	unsigned blockSize = *(unsigned*)(i)&~1; //Includes header and footer
	//Check for heap end
	if (*(unsigned*)(i) == ~0)
		return NULL;

	//Check if a block needs to be split
	if (blockSize == size + 2 * headerSize || blockSize == size + 8 + 2 * headerSize)
	{
		//update block to show not free
		*(unsigned*)(i) = blockSize;
		*(unsigned*)(i + size) = blockSize;
	}
	//Split a block
	//Update free block footer and header
	else 
	{
		*(unsigned*)(i + blockSize - headerSize) = (blockSize - size - 2 * headerSize) | 1;
		*(unsigned*)(i + size + 2 * headerSize) = (blockSize - size - 2 * headerSize) | 1;
		//Update used block footer and header
		*(unsigned*)(i + size + headerSize) = (size + 2 * headerSize);
		*(unsigned*)(i) = (size + 2 * headerSize);
	}
	
	return i + headerSize;
}

void Free(addrs_t addrs)
{
	//Define sizes of each block to reduce the clusterfuck of pointer(and memory accesses)
	unsigned freedBlockSize = *(unsigned*)(addrs - headerSize);
	unsigned beforeBlockSize = *(unsigned*)(addrs - 2*headerSize) & ~1;
	unsigned afterBlockSize = *(unsigned*)(addrs + freedBlockSize - headerSize) & ~1;

	//Same for whether blocks are free
	char beforeBlockFree = *(unsigned*)(addrs - 2 * headerSize) & 1;
	char afterBlockFree = *(unsigned*)(addrs + freedBlockSize - headerSize) & 1;

	//Save pointers too
	addrs_t beforeBlockHeader = addrs - beforeBlockSize - headerSize;
	addrs_t afterBlockHeader = addrs + freedBlockSize - headerSize;

	//Don't free after the heap
	if (*(unsigned*)(afterBlockHeader) == ~0)
		afterBlockFree = 0;

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
		*(unsigned*)(addrs - headerSize) = (afterBlockSize + freedBlockSize) | 1;
		*(unsigned*)(afterBlockHeader - headerSize) = (afterBlockSize + freedBlockSize) | 1;
	}
	else
	{
		*(unsigned*)(addrs - headerSize) = freedBlockSize | 1;
		*(unsigned*)(afterBlockHeader - headerSize) = freedBlockSize | 1;
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

addrs_t Vbaseptr;
addrs_t* redir_tabl;
unsigned int num_entries;
unsigned int table_size;
addrs_t freeptr;

void VInit(size_t size)
int main(int argc, char **argv)
{
	size += 8 - size % 8;

	Vbaseptr = malloc(size);
	freeptr = Vbaseptr;

	// Have chunks 8 bytes
	table_size = size / 8;
	// [redirection_table] is needed to help [redir_tabl] because of initialization problems and scope
	addrs_t redirection_table[table_size];
	redir_tabl = redirection_table;
}

addrs_t* VMalloc(size_t size)
{
	//TODO: Have things be put into the redirection table nicely (non-linear array traversal)

	// We do not desire to allocate zero bytes
	if (!size)
	{
		return NULL;
	}

	int index = 0;
	while (!!redir_tabl[index] && index < table_size)
	{
		index++;
	}

	// If the table appears to be full, we return null.
	if (index == table_size)
	{
		return NULL;
	}

	// Pad requested size to be a multiple of 8
	size += 8 - size % 8;

	//Assuming that none of the bad stuff happened, we can allocate the appropriate space, and then update [freeptr] accordingly.
	redir_tabl[index] = freeptr;
	freeptr += size;

	//As we've added a new element, the size of things in the table grows by one.
	num_entries++;

	return &redir_tabl[index];
}

void VFree(addrs_t* addr) //[addr] is like the index in the table?
{
	//TO-DO: We're given an address outside of redir_tabl (or do we not assume this will occur?)

	//Need to make something that gets all the addresses that need to be adjusted
	int need_shifting[num_entries];

	//Also somehow need to calculate the size of the memory that is to be freed, so that's what's [upper_bound] is for
	addrs_t upper_bound;

	/* Loop variables:
	  [index] is for all table values
	  [count] is for all non-NULL table values
	  [shifted] is for all table values that need to be tweaked after
	  [curr] is merely one of those save call variable things*/
	int count, index, shifted;
	addrs_t curr;

	while (count < num_entries)
	{
		curr = redir_tabl[index];
		if (curr)
		{
			if (curr > *addr) //Means [curr] needs to be adjusted
			{
				need_shifting[shifted++] = index;
				if (!upper_bound || curr < upper_bound)
				{
					upper_bound = curr;
				}
			}
			count++;
		}
		index++;
	}

	//Two scenarios: the block isn't/is already at the end of the heap
	if (upper_bound) //Isn't last
	{
		//Is this the variable type that the cool kids like to use?
		size_t size = upper_bound - *addr;

		int block;
		while (need_shifting[block])
		{
			//This *should* move a block over
			redir_tabl[need_shifting[block++]] -= size;
		}

		freeptr -= size;
	}
	else //Is last
	{
		//Assign free space to given address
		freeptr = *addr;
	}

	//Vacate the table spot
	*addr = NULL;

	//Block lost, decrement
	num_entries--;
}

//Haven't done either of these yet :<
addrs_t* VPut(any_t data, size_t size)
{
	/* Allocate size bytes from M2 using VMalloc().
	 Copy size bytes of data into Malloc'd memory.
	 You can assume data is a storage area outside M2.
	 Return pointer to redirection table for Malloc'd memory. */

	return NULL;
}

void VGet(any_t return_data, addrs_t* addr, size_t size)
{
	/*Copy size bytes from the memory area, M2, to data address. The
	  addr argument specifies a pointer to a redirection table entry.
	  As with VPut(), you can assume data is a storage area outside M2.
	  Finally, de-allocate size bytes of memory using VFree() with addr
	  pointing to a redirection table entry. */
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