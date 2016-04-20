/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct frame_table_entry {
	int page;
	int count;
} frame_table_entry;


//global variables
int ALGORITHM;
int NFRAMES;
frame_table_entry *FRAMETABLE;
struct disk *DISK;

void page_fault_handler( struct page_table *pt, int page )
{
		
	printf("page fault on page #%d\n",page);

	//first, check if page is already in physical memory
	//if so, then just set its bits to read/write
	int f, b;
	page_table_get_entry(pt, page, &f, &b);
	if (b != 0) {
		//page is already in memory, so set read/write bits
		page_table_set_entry(pt, page, f, PROT_READ|PROT_WRITE);
	} else {

	//else, page is not already in memory, so need to set read bits
		//first check to see if there is an empty frame

		int frame = -1;
		int i;
		for (i=0; i<NFRAMES; i++) {
			if (FRAMETABLE[i].page == -1) {
				frame = i;
			}
		}

		//if there is no empty frame, then you need to use an algorithm

		if (frame == -1) {

		
			//rand
			if (ALGORITHM == 1) {
				frame = lrand48() % NFRAMES;
			}

			//fifo
			if (ALGORITHM == 2) {
				frame = 0;
				//select frame by choosing the one with the highest count
				for (i=0; i<NFRAMES; i++) {
					if (FRAMETABLE[i].count > FRAMETABLE[frame].count) {
						frame = i;
					}

				}
			}
		}

		//set count for selected frame to 1
		FRAMETABLE[frame].count = 1;
		FRAMETABLE[frame].page = page;

		//increment counts that are already > 0 (except for count for frame)
		for (i=0; i<NFRAMES; i++) {
			if (FRAMETABLE[i].count > 0 && i!=frame) {
				FRAMETABLE[i].count++;
			}
		}

		printf("Selected frame: %i\n", frame);
		
		page_table_set_entry(pt, page, frame, PROT_READ);
		disk_read(DISK, page, &page_table_get_physmem(pt)[3*BLOCK_SIZE]);
	}

}

int main( int argc, char *argv[] )
{
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}

	int npages = atoi(argv[1]);
	int nframes = atoi(argv[2]);
	NFRAMES = nframes;

	if (!strcmp(argv[3], "rand")) {
		ALGORITHM = 1;
	}
	if (!strcmp(argv[3], "fifo")) {
		ALGORITHM = 2;
	}
	if (!strcmp(argv[3], "custom")) {
		ALGORITHM = 3;
	}
	const char *program = argv[4];

	//need to get page replacement algorithm

	DISK = disk_open("myvirtualdisk",npages);
	if(!DISK) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}


	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	char *virtmem = page_table_get_virtmem(pt);

	char *physmem = page_table_get_physmem(pt);

	//FRAMETABLE keeps track of counts for each frame
	FRAMETABLE = malloc(NFRAMES * sizeof(frame_table_entry));

	//initialize FRAMETABLE values to -1
	int i;
	for (i=0; i<NFRAMES; i++) {
		FRAMETABLE[i].count = -1;
		FRAMETABLE[i].page = -1;
	}

	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);

	}

	page_table_print(pt);

//	for (i=0; i<NFRAMES; i++) {
//		printf("%i\n", FRAMETABLE[i].page);
//	}

	free (FRAMETABLE);
	page_table_delete(pt);
	disk_close(DISK);

	return 0;
}
