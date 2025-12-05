#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include "p3Heap.h"
#include <limits.h>
#include <stdint.h>

/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block.
 */
typedef struct blockHeader {           

    /*
     * 1) The size of each heap block must be a multiple of 8
     * 2) Heap blocks have blockHeaders that contain size and status bits
     * 3) Free heap blocks contain a footer, but we can use the blockHeader 
     *
     * All heap blocks have a blockHeader with size and status
     * Free heap blocks have a blockHeader as its footer with size only
     *
     * Status is stored using the two least significant bits
     *   Bit0 => least significant bit, last bit
     *   Bit0 == 0 => free block
     *   Bit0 == 1 => allocated block
     *
     *   Bit1 => second last bit 
     *   Bit1 == 0 => previous block is free
     *   Bit1 == 1 => previous block is allocated
     * 
     * Start Heap: 
     *  The blockHeader for the first block of heap is after skipping 4 bytes.
     *  This ensures alignment requirements can be met.
     * 
     * End Mark: 
     *  The end of the available memory is indicated using a size_status of 1.
     * 
     * Examples:
     * 
     * 1. Allocated block of size 24 bytes:
     *    Allocated Block Header:
     *      If the previous block is free      p-bit=0 size_status would be 25
     *      If the previous block is allocated p-bit=1 size_status would be 27
     * 
     * 2. Free block of size 24 bytes:
     *    Free Block Header:
     *      If the previous block is free      p-bit=0 size_status would be 24
     *      If the previous block is allocated p-bit=1 size_status would be 26
     *    Free Block Footer:
     *      size_status should be 24
     */
    int size_status;

} blockHeader;         

/* Global variable - DO NOT CHANGE NAME or TYPE. 
 * It must point to the first block in the heap and is set by init_heap()
 * i.e., the block at the lowest address.
 */
blockHeader *heap_start = NULL;     

/* Size of heap allocation padded to round to the nearest page size.
 */
int alloc_size;

/*
 * Additional global variables may be added as needed below
 * TODO: add global variables as needed by your function
 */
char* heap_end = NULL; // Address of where the heap ends



/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block (payload) on success.
 * Returns NULL on failure.
 *
 * This function must:
 * - Check size - Return NULL if size < 1 
 * - Determine block size rounding up to a multiple of 8 
 *   and possibly add padding as a result.
 *
 * - Use BEST-FIT PLACEMENT POLICY to chose a free block
 *
 * - If the BEST-FIT block that is found is exact size match
 *   - 1. Update all heap blocks as needed for any affected blocks
 *   - 2. Return the address of the allocated block payload
 *
 * - If the BEST-FIT block that is found is large enough to split 
 *   - 1. SPLIT the free block into two valid heap blocks:
 *         1. an allocated block
 *         2. a free block
 *         NOTE: both blocks must meet heap block requirements 
 *       - Update all heap block header(s) and footer(s) 
 *              as needed for any affected blocks.
 *   - 2. Return the address of the allocated block payload
 *
 *   Return NULL if unable to find and allocate block of required size
 *
 * Note: payload address that is returned is NOT the address of the
 *       block header. It is the address of the start of the 
 *       available memory for the requester.
 *
 */
void* alloc(int size) {     
	//DONE: Your code goes in here.
	// !! only changes of note since p3A are style changes
	if(size < 1) { return NULL; }

	// Add header bytes to size
	size += 4;

	// Add padding if needed to make total block size (w/ header) a multiple of 8
	if(size % 8 != 0) {
		size = size + (8 - (size % 8));
	}

	// Init search values (current block and best fit/size so far)
	// that will be used in the best fit search
	blockHeader *current = heap_start;
	blockHeader *bestFit = NULL;
	int bestSize = INT_MAX;
	heap_end = (char*)heap_start + alloc_size;

	// Best-Fit search, repeat until at end of heap
	while((char*)current < heap_end && current->size_status != 1) {
		// Store raw size of the current block for usage later
		int blockSize = current->size_status & ~0x3;
		// Store next block header for usage later
		blockHeader* nextHeader = (blockHeader*)((char*)current + blockSize);

		// If the current block being searched isnt big enough, continue search
		if(blockSize < size) {
			current = nextHeader;
			continue;
		}

		// Only perform size checks on free blocks
		if((current->size_status & 0x1) == 0) {
			// If current search block matches size exactly, use it (just update its header)
			// otherwise search through heap normally
			if(blockSize == size) {
				current->size_status += 1; // set a-bit to 1

				// If next header's pbit is 0, make it 1, dont change if its the end of heap
				if((char*)nextHeader < heap_end && (nextHeader->size_status & 0x2) == 0) {
					nextHeader->size_status += 2;
				}
				// Return the updated block header's payload address
				return (void*)((char*)current + 4);
			}
			else {
				// Find lowest size free block that fits size
				if(blockSize >= size && blockSize < bestSize) {
					bestSize = blockSize;
					bestFit = current;
				}
			}
		}
		// Iterate to next block
		current = nextHeader;
	}

	// Split bestFit block if one was found
	// newBlock will be the inserted block
	// bestFit will be updated to be the free block
	if(bestFit != NULL) {
		// Store size of block we split, and new bestFit size after split
		int totalFreeSize = bestFit->size_status & ~0x3;
		int bestFitSize = totalFreeSize - size;

		// Create new block of memory (keep p-bit same, a-bit is 1)
		blockHeader* newBlock = bestFit;
		newBlock->size_status = size + (bestFit->size_status & 0x2) + 1;

		// Update bestFit's payload addr
		bestFit = (blockHeader*)((char*)newBlock + size);
		// Update bestFit's footer size
		((blockHeader*)((char*)bestFit + bestFitSize - 4))->size_status = bestFitSize;
		// Update bestFit's header value
		// !! updated since p3A turn in, just better logic instead of adding 2
		bestFit->size_status = bestFitSize | (newBlock->size_status & 0x2);

		// Return payload addr of newBlock
		return (void*)((char*)newBlock + 4);
	}

	return NULL;
}

/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - Update header(s) and footer as needed.
 *
 * If free results in two or more adjacent free blocks,
 * they will be immediately coalesced into one larger free block.
 * so free blocks require a footer (blockHeader works) to store the size
 *
 */
int free_block(void *ptr) {
	//DONE: Your code goes in here.
	// !! all work done here was after p3A turn in
	// Checks if ptr is NULL or is not a multiple of 8, fails if so
	if(ptr == NULL || (uintptr_t)ptr % 8 != 0) { return -1; }

	// Init heap_end (as char*) and then
	// Check if ptr is inside the heap space
	heap_end = (char*)heap_start + alloc_size;
	if((char*)ptr < (char*)heap_start || (char*)ptr >= heap_end) { return -1; }

	// Get ptr's header for later use, then check if its freed already
	blockHeader* ptr_header = (blockHeader*)((char*)ptr - 4);
	if((ptr_header->size_status & 0x1) == 0) { return -1; }

	// Set ptr_header a-bit to 0/free
	ptr_header->size_status = ptr_header->size_status & ~0x1;

	// Init size of ptr for later use
	int ptr_size = ptr_header->size_status & ~0x3;

	// Init next_header to point to next block's header
	blockHeader* next_header = (blockHeader*)((char*)ptr_header + ptr_size);

	// If the next block is free, coalesce it into ptr
	if((next_header->size_status & 0x1) == 0) {
		// Add next block's size to ptr
		int next_size = (next_header->size_status & ~0x3);
		ptr_size += next_size;
		ptr_header->size_status = ptr_size | (ptr_header->size_status & 0x2);
	}

	// Check if previous block is free, if so
	// we will coalesce it into ptr
	if((ptr_header->size_status & 0x2) == 0) {
		// Get footer of previous block
		blockHeader* prev_footer = (blockHeader*)((char*)ptr_header - 4);
		// Init prev_header to point to previous block's header
		blockHeader* prev_header = (blockHeader*)((char*)ptr_header - prev_footer->size_status);

		// Update ptr_header to be where prev_header is
		ptr_header = prev_header;

		// Update ptr's size to ptr + size of previous block
		ptr_size += prev_footer->size_status;
		ptr_header->size_status = ptr_size | (prev_header->size_status & 0x2);

		// Update payload address of ptr
		ptr = (char*)ptr_header + 4;
	}

	// Now that all coalescing is done, properly set p-bit of next_header
	next_header = (blockHeader*)((char*)ptr_header + ptr_size);
	if((next_header->size_status & ~0x3) != 0) {
		next_header->size_status = (next_header->size_status & ~0x2);
	}

	// Assign a footer to ptr
	blockHeader* ptr_footer = (blockHeader*)((char*)ptr_header + ptr_size - 4);
	if ((char*)ptr_footer < heap_end) {
		ptr_footer->size_status = ptr_size;
	}

	return 0;
}


/*
 * Initializes the memory allocator.
 * Called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */                    
int init_heap(int sizeOfRegion) {    

    static int allocated_once = 0; //prevent multiple myInit calls

    int   pagesize; // page size
    int   padsize;  // size of padding when heap size is not a multiple of page size
    void* mmap_ptr; // pointer to memory mapped area
    int   fd;

    blockHeader* end_mark;

    if (0 != allocated_once) {
        fprintf(stderr, 
                "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }

    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize from O.S. 
    pagesize = getpagesize();

    // Calculate padsize, as padding is required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    alloc_size = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }

    allocated_once = 1;

    // for double word alignment and end mark
    alloc_size -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heap_start = (blockHeader*) mmap_ptr + 1;

    // Set the end mark
    end_mark = (blockHeader*)((void*)heap_start + alloc_size);
    end_mark->size_status = 1;

    // Set size in header
    heap_start->size_status = alloc_size;

    // Set p-bit as allocated in header
    // Note a-bit left at 0 for free
    heap_start->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader*) ((void*)heap_start + alloc_size - 4);
    footer->size_status = alloc_size;

    return 0;
} 

/*
 * Can be used for DEBUGGING to help you visualize your heap structure.
 * It traverses heap blocks and prints info about each block found.
 * 
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */                     
void disp_heap() {     

    int    counter;
    char   status[6];
    char   p_status[6];
    char * t_begin = NULL;
    char * t_end   = NULL;
    int    t_size;

    blockHeader *current = heap_start;
    counter = 1;

    int used_size =  0;
    int free_size =  0;
    int is_used   = -1;

    fprintf(stdout, 
            "********************************** HEAP: Block List ****************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, 
            "--------------------------------------------------------------------------------\n");

    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;

        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "alloc");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "FREE ");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "alloc");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "FREE ");
        }

        if (is_used) 
            used_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;

        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%4i\n", counter, status, 
                p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);

        current = (blockHeader*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, 
            "--------------------------------------------------------------------------------\n");
    fprintf(stdout, 
            "********************************************************************************\n");
    fprintf(stdout, "Total used size = %4d\n", used_size);
    fprintf(stdout, "Total free size = %4d\n", free_size);
    fprintf(stdout, "Total size      = %4d\n", used_size + free_size);
    fprintf(stdout, 
            "********************************************************************************\n");
    fflush(stdout);

    return;  
}