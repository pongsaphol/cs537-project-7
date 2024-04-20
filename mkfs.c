#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include "wfs.h"

char* mem;

void init_mem(char* file) {
	int fd = open(file, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd < 0)
		perror("open");
  
  struct stat sb;
  fstat(fd, &sb);

  int shm_size = sb.st_size;

	mem = mmap(NULL, shm_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	if (mem == (void *)-1) 
		perror("mmap");

	close(fd);
}

char* disk;
int inode;
int block;


static int parse_args(int argc, char **argv) {
	int op;
	while ((op = getopt(argc, argv, "d:i:b:")) != -1) {
		switch(op) {									           
		case 'd':
    disk = optarg;
		break;
	
		case 'i':
    inode = atoi(optarg);
		break;

    case 'b':
    block = atoi(optarg);
    break;

		return 1;
		}
	}
	return 0;
}

int get_next_ptr(int pre, int size) {
  return pre + ((size + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
}

int main(int argc, char* argv[]) {
  if (parse_args(argc, argv) != 0)
		return -1;
  init_mem(disk);
  block = ((block + 31) >> 5) << 5;

  int inode_bit_count = 0;
  while (inode_bit_count < inode) {
    inode_bit_count += 8;
  }

  int dnote_bit_count = 0;
  while (dnote_bit_count < block) {
    dnote_bit_count += 8;
  }
  printf("%d\n", (int)sizeof(struct wfs_sb));
  printf("%d\n", (int)sizeof(struct wfs_inode));

  struct wfs_sb sb;
  sb.num_inodes = inode;
  sb.num_data_blocks = block;
  sb.i_bitmap_ptr = get_next_ptr(0, sizeof(sb));
  sb.d_bitmap_ptr = get_next_ptr(sb.i_bitmap_ptr, inode_bit_count / 8);
  sb.i_blocks_ptr = get_next_ptr(sb.d_bitmap_ptr, dnote_bit_count / 8);
  sb.d_blocks_ptr = get_next_ptr(sb.i_blocks_ptr, inode * sizeof(struct wfs_inode));
  memcpy(mem, &sb, sizeof(sb));
}