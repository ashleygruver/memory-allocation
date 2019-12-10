#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef char *addrs_t;
typedef void *any_t;

addrs_t baseptr;

//All of this needs to go
addrs_t* redir_tabl;
unsigned int num_entries;
unsigned int table_size;
addrs_t freeptr;

void VInit(size_t size)
int main(int argc, char **argv)
{
	size += 8 - size % 8;

	baseptr = malloc(size);
	freeptr = baseptr;

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

