#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef char *addrs_t;
typedef void *any_t;

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

	//We do not desire to allocate zero bytes
	if (!size)
		return NULL;

	int index = 0;
	while (!!redir_tabl[index] && index < 65536)
		index++;

	//If the table appears to be full, we return null.
	if (index == 65536)
		return NULL;

	//Pad requested size to be a multiple of 8
	if(!!(size % 8))
		size += 8 - size % 8;
	
	//Can't go over that size limit!
	if(*(addrs_t*)(baseptr + 8) + size > baseptr + *(unsigned*)(baseptr + 4))
		return NULL;
	
	//Assuming that none of the bad stuff happened, we can allocate the appropriate space, and then update [freeptr] accordingly.
	redir_tabl[index] = *(addrs_t*)(baseptr + 8);
	*(addrs_t*)(baseptr + 8) += size;

	//As we've added a new element, the size of things in the table grows by one.
	*(unsigned*)(baseptr) += 1;

	return &redir_tabl[index];
}

void VFree(addrs_t* addr) //[addr] is like the index in the table?
{
	//TO-DO: We're given an address outside of redir_tabl (or do we not assume this will occur?)
	//Can't free anything if there's nothing there
	if(!*(unsigned*)(baseptr))
		return;
	//Need to make something that gets all the addresses that need to be adjusted
	int need_shifting[*(unsigned*)(baseptr)];

	//Also somehow need to calculate the size of the memory that is to be freed, so that's what's [upper_bound] is for
	addrs_t upper_bound = NULL;

	/* Loop variables:
	  [index] is for all table values
	  [count] is for all non-NULL table values
	  [shifted] is for all table values that need to be tweaked after
	  [curr] is merely one of those save call variable things*/
	int count, index, shifted;
	addrs_t curr;

	while(count < *(unsigned*)(baseptr) && index < 65536)
	{
		curr = redir_tabl[index];
		if(curr)
		{
			if(curr > *addr) //Means [curr] needs to be adjusted
			{
				need_shifting[shifted++] = index;
				if (!upper_bound || curr < upper_bound)
					upper_bound = curr;
			}
			count++;
		}
		index++;
	}

	//Two scenarios: the block isn't/is already at the end of the heap
	if(upper_bound) //Isn't last
	{
		unsigned size = upper_bound - *addr;

		int block;
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
