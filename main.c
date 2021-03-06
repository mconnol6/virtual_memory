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
	int count; //use for fifo
} frame_table_entry;

void update_frame_table(int frame, int page);
void manage_memory(int page, int frame, int current_page, struct page_table *pt);
int fifo();
int custom(struct page_table *pt);

//global variables
int ALGORITHM;
int NFRAMES;
frame_table_entry *FRAMETABLE;
struct disk *DISK;
int NPAGEFAULTS;
int NDISKWRITES;
int NDISKREADS;

void page_fault_handler( struct page_table *pt, int page )
{
	//increment NPAGEFAULTS
	NPAGEFAULTS++;
		
	//first, check if page is already in physical memory
	//if so, then just set its bits to read/write
	int f, b;
	page_table_get_entry(pt, page, &f, &b);

	if (b != 0) {
		//page is already in memory, so set read/write bits
		page_table_set_entry(pt, page, f, PROT_READ|PROT_WRITE);
	} else {
		int frame = -1;

		//else, page is not already in memory, so need to set read bits
		//if there are more frames than pages, then each page maps to its own frame
		if (NFRAMES > page_table_get_npages(pt)) {
			frame = page;
		} else {
			//else, you need to find a free frame
			//first check to see if there is an empty frame

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
					frame = fifo();
				}

				//custom algorithm
				if (ALGORITHM == 3) {
					frame = custom(pt);
				}
			}
		}

		//update memory and frame table
		int current_page = FRAMETABLE[frame].page;
		update_frame_table(frame, page);
		manage_memory(page, frame, current_page, pt);
	}

}

int fifo() {

	int frame = 0;
	int i;
	//select frame by choosing the one with the highest count
	for (i=0; i<NFRAMES; i++) {
		if (FRAMETABLE[i].count > FRAMETABLE[frame].count) {
			frame = i;
		}

	}
	return frame;
}

//custom algorithm
int custom(struct page_table *pt) {

	static int prev = 0; //keep track of which page was last loaded

	int frame_selected = 0; //boolean value
	int frame = 0;
	int i;
	int f, b;
	//select frame by choosing the one with the highest count
	for (i=0; i<NFRAMES; i++) {
		int curr_page = FRAMETABLE[i].page;
		page_table_get_entry(pt, curr_page, &f, &b);
		//ensure that the dirty bit is not set
		//and that the page is not the one that was just loaded
		if (b == (PROT_READ) && i != prev) {
			frame = i;
			frame_selected = 1;
			prev = i;
			break;
		}

	}

	//if frame is -1, then no frame was selected, so resort to fifo
	if (frame_selected == 0) {
		frame = fifo();
	}
	return frame;
}

void manage_memory(int page, int frame, int current_page, struct page_table *pt) {

	//check if the block is dirty if current page is != -1
	//if so, write data to disk
	//update page table regardless
	if (current_page != -1) {
		int f2, b2;
		page_table_get_entry(pt, current_page, &f2, &b2);
		if (b2 == (PROT_READ|PROT_WRITE)) {
			disk_write(DISK, current_page, &page_table_get_physmem(pt)[frame*BLOCK_SIZE]);
			NDISKWRITES++;
		}
		page_table_set_entry(pt, current_page, 0, 0);
	}
	
	//read from disk and update page table
	NDISKREADS++;
	disk_read(DISK, page, &page_table_get_physmem(pt)[frame*BLOCK_SIZE]);
	page_table_set_entry(pt, page, frame, PROT_READ);
}


void update_frame_table(int frame, int page) {

	//set count for selected frame to 1
	FRAMETABLE[frame].count = 1;
	FRAMETABLE[frame].page = page;

	//increment counts that are already > 0 (except for count for frame)
	int i;
	for (i=0; i<NFRAMES; i++) {
		if (FRAMETABLE[i].count > 0 && i!=frame) {
			FRAMETABLE[i].count++;
		}
	}
}


int main( int argc, char *argv[] )
{
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}

	NPAGEFAULTS = 0;
	NDISKWRITES = 0;
	NDISKREADS = 0;

	int npages = atoi(argv[1]);

	if (npages <= 0) {
		printf("npages must be greater than or equal to 0.\n");
		exit(1);
	}

	int nframes = atoi(argv[2]);

	if (nframes <= 1) {
		printf("nframes must be great than or equal to 1.\n");
		exit(1);
	}

	NFRAMES = nframes;

	if (!strcmp(argv[3], "rand")) {
		ALGORITHM = 1;
	}
	else if (!strcmp(argv[3], "fifo")) {
		ALGORITHM = 2;
	}
	else if (!strcmp(argv[3], "custom")) {
		ALGORITHM = 3;
	} else {
		printf("Unknown algorithm: %s\n", argv[3]);
		exit(1);
	}

	const char *program = argv[4];

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

	//char *physmem = page_table_get_physmem(pt);

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
		fprintf(stderr,"unknown program: %s\n",argv[4]);
		exit(1);

	}

	free (FRAMETABLE);
	page_table_delete(pt);
	disk_close(DISK);

	printf("There were %i reads\n", NDISKREADS);
	printf("There were %i writes\n", NDISKWRITES);
	printf("There were %i page faults\n", NPAGEFAULTS);

	return 0;
}
