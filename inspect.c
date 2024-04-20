#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
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


static int parse_args(char* argv) {
  disk = argv;
}

int get_next_ptr(int pre, int size) {
  return pre + ((size + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
}

int main(int argc, char* argv[]) {
  printf("Inspecting disk image: %s\n", argv[1]);
  parse_args(argv[1]);
  init_mem(disk);
  struct wfs_sb* sb = (struct wfs_sb*)mem;
  printf("Superblock size: %ld\n", sizeof(struct wfs_sb));
  printf("Inode size: %ld\n", sizeof(struct wfs_inode));
  printf("--------------------\n");
  printf("num_inodes: %ld\n", sb->num_inodes);
  printf("num_data_blocks: %ld\n", sb->num_data_blocks);
  printf("i_bitmap_ptr: %ld\n", sb->i_bitmap_ptr);
  printf("d_bitmap_ptr: %ld\n", sb->d_bitmap_ptr);
  printf("i_blocks_ptr: %ld\n", sb->i_blocks_ptr);
  printf("d_blocks_ptr: %ld\n", sb->d_blocks_ptr);
  printf("\n-----------END FIRST PART---------\n\n");
  char* i_bitmap = mem + sb->i_bitmap_ptr;
  char* d_bitmap = mem + sb->d_bitmap_ptr;
  for (int i = 0; i < sb->num_inodes; i++) {
    if (i != 0 && i % 8 == 0) {
      i_bitmap++;
    } 
    if ((*i_bitmap >> (i % 8)) & 1) {
      printf("Inode %d is allocated\n", i);
      struct wfs_inode* inode = (struct wfs_inode*)(mem + sb->i_blocks_ptr + i * BLOCK_SIZE);
      printf("Inode num: %d\n", inode->num);
      printf("Inode mode: %o\n", inode->mode);
      printf("Permission: ");
      printf((inode->mode & S_IFDIR) ? "d" : "-");
      printf((inode->mode & S_IRUSR) ? "r" : "-");
      printf((inode->mode & S_IWUSR) ? "w" : "-");
      printf((inode->mode & S_IXUSR) ? "x" : "-");
      printf((inode->mode & S_IRGRP) ? "r" : "-");
      printf((inode->mode & S_IWGRP) ? "w" : "-");
      printf((inode->mode & S_IXGRP) ? "x" : "-");
      printf((inode->mode & S_IROTH) ? "r" : "-");
      printf((inode->mode & S_IWOTH) ? "w" : "-");
      printf((inode->mode & S_IXOTH) ? "x" : "-");
      printf("\n");
      printf("File type: %s\n", (inode->mode & S_IFDIR) ? "Directory" : "File");
      printf("Inode uid: %d\n", inode->uid);
      printf("Inode gid: %d\n", inode->gid);
      printf("Inode size: %ld\n", inode->size);
      printf("Inode nlinks: %d\n", inode->nlinks);
      printf("Inode atim: %ld\n", inode->atim);
      printf("Inode mtim: %ld\n", inode->mtim);
      printf("Inode ctim: %ld\n", inode->ctim);
      for (int j = 0; j < N_BLOCKS; j++) {
        printf("Inode block %d: %ld\n", j, inode->blocks[j]);
      }
      // char* now = (char*)inode;
      // for (int i = 0; i < 512; ++i) {
      //   printf("%02x ", (int)now[i]);
      // }
      printf("\n--------------------\n");
    }
  }
  printf("\n-----------END SECOND PART---------\n\n");
  for (int i = 0; i < sb->num_data_blocks; i++) {
    if (i != 0 && i % 8 == 0) {
      d_bitmap++;
    } 
    if ((*d_bitmap >> (i % 8)) & 1) {
      printf("Data block %ld is allocated %lx\n", i * BLOCK_SIZE + sb->d_blocks_ptr, i * BLOCK_SIZE + sb->d_blocks_ptr);
      struct wfs_dentry* dentry = (struct wfs_dentry*)(mem + sb->d_blocks_ptr + i * BLOCK_SIZE);
      printf("Dentry name: %s\n", dentry->name);
      printf("Dentry num: %d\n", dentry->num);
      char* now = (char*)dentry;
      for (int i = 0; i < 512; ++i) {
        printf("%02x ", (int)now[i]);
      }
      printf("\n");
      // for (int j = 0; j < dentry->num; ++j) {
      //   off_t* inode_ptr = (off_t*)(mem + sb->d_blocks_ptr + i * BLOCK_SIZE + sizeof(struct wfs_dentry) + j * sizeof(off_t));
      //   printf("Inode ptr: %d %ld\n", j, *inode_ptr);
      // }
    }
  }
}