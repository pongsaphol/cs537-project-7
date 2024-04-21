#ifndef _UTILITY_H_
#define _UTILITY_H_
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include "wfs.h"

extern char* mem;
extern struct wfs_sb* sb;

void print_inode_stat(struct wfs_inode* inode) {
  printf("--------------------\n");
  printf("num: %d\n", inode->num);
  printf("mode: %d\n", inode->mode);
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
  printf("uid: %d\n", inode->uid);
  printf("gid: %d\n", inode->gid);
  printf("size: %ld\n", inode->size);
  printf("nlinks: %d\n", inode->nlinks);
  printf("atim: %ld\n", inode->atim);
  printf("mtim: %ld\n", inode->mtim);
  printf("ctim: %ld\n", inode->ctim);
  for (int i = 0; i < N_BLOCKS; i++) {
    printf("block %d: %ld\n", i, inode->blocks[i]);
  }
  printf("--------------------\n");
}

void init_mem(char* file, int reset) {
	int fd = open(file, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd < 0)
		perror("open");
  
  struct stat st;
  fstat(fd, &st);

  int shm_size = st.st_size;

	mem = mmap(NULL, shm_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
  if (reset)
    memset(mem, 0, shm_size);
	if (mem == (void *)-1) 
		perror("mmap");

	close(fd);

  sb = (struct wfs_sb*)mem;
}


char **parse_path(char* path) {
  char **tokens = malloc(100 * sizeof(char*));
  memset(tokens, 0, 100 * sizeof(char*));
  char *token = strtok(path, "/");
  int i = 0;
  while (token != NULL) {
    tokens[i] = token;
    token = strtok(NULL, "/");
    i++;
  }
  return tokens;
}

struct wfs_dentry* get_dentry(int pos) {
  struct wfs_dentry* dentry = (struct wfs_dentry*)(mem + pos);
  return dentry;
}

int get_inode_rec(char** path, int inode) {
  if (path[0] == NULL) {
    return inode;
  }
  struct wfs_inode* inodes = (struct wfs_inode*)(mem + sb->i_blocks_ptr + inode * BLOCK_SIZE);
  if (inodes->mode & S_IFDIR) {
    for (int i = 0; i <= D_BLOCK; i++) {
      if (inodes->blocks[i] != 0) {
        struct wfs_dentry* dentry = get_dentry(inodes->blocks[i]);
        if (strcmp(dentry->name, path[0]) == 0) {
          return get_inode_rec(path + 1, dentry->num);
        } 
      }
    }
  }
  return -ENOENT;
}

int get_inode(const char* path) {
  char* path_copy = strdup(path);
  char** tokens = parse_path(path_copy);
  int inode = get_inode_rec(tokens, 0);
  free(tokens);
  free(path_copy);
  return inode;
}

int check_inode_bitmap(int inode) {
  if (inode >= sb->num_inodes) {
    return -ENOSPC;
  }
  char* i_bitmap = mem + sb->i_bitmap_ptr;
  while (inode > 8) {
    i_bitmap++;
    inode -= 8;
  }
  return (*i_bitmap >> inode) & 1;
}

int get_addr_new_inode() {
  for (int i = 0; i < sb->num_inodes; i++) {
    if (check_inode_bitmap(i) == 0) {
      return i;
    }
  }
  return -ENOSPC;
}


struct wfs_inode* get_inode_content(int inode) {
  return (struct wfs_inode*)(mem + sb->i_blocks_ptr + inode * BLOCK_SIZE);
}

int get_new_inode() {
  int inode = get_addr_new_inode();
  if (inode < 0) {
    return inode;
  }
  char* i_bitmap = mem + sb->i_bitmap_ptr;
  while (inode > 8) {
    i_bitmap++;
    inode -= 8;
  }
  *i_bitmap |= 1 << inode;

  struct wfs_inode* inode_c = get_inode_content(inode);
  inode_c->num = inode;
  inode_c->uid = getuid();
  inode_c->gid = getgid();
  inode_c->size = 0;
  inode_c->nlinks = 1;
  time_t now = time(NULL);
  inode_c->atim = inode_c->mtim = inode_c->ctim = now;

  return inode;
}


int check_dblock_bitmap(int dblock) {
  if (dblock >= sb->num_data_blocks) {
    return -ENOSPC;
  }
  char* d_bitmap = mem + sb->d_bitmap_ptr;
  while (dblock > 8) {
    d_bitmap++;
    dblock -= 8;
  }
  return (*d_bitmap >> dblock) & 1;
}


int get_addr_new_dblock() {
  for (int i = 0; i < sb->num_data_blocks; i++) {
    if (check_dblock_bitmap(i) == 0) {
      return i;
    }
  }
  return -ENOSPC;
}

int get_new_dblock() {
  int dblock = get_addr_new_dblock();
  if (dblock < 0) {
    return dblock;
  }
  char* d_bitmap = mem + sb->d_bitmap_ptr;
  while (dblock > 8) {
    d_bitmap++;
    dblock -= 8;
  }
  *d_bitmap |= 1 << dblock;
  return dblock * BLOCK_SIZE + sb->d_blocks_ptr;
}

char* get_dblock_content(int dblock) {
  return mem + dblock;
}

void free_block(int dblock) {
  struct wfs_dentry* entry = get_dentry(dblock);
  memset(entry, 0, BLOCK_SIZE);
  int idx = (dblock - sb->d_blocks_ptr) / BLOCK_SIZE;
  char* d_bitmap = mem + sb->d_bitmap_ptr;
  while (idx > 8) {
    d_bitmap++;
    idx -= 8;
  }
  *d_bitmap &= ~(1 << idx);
}

void free_inode(struct wfs_inode* inode) {
  for (int i = 0; i <= D_BLOCK; i++) {
    if (inode->blocks[i] != 0) {
      free_block(inode->blocks[i]);
    }
  }
  if (inode->blocks[IND_BLOCK] != 0) {
    int* ind_block = (int*)get_dblock_content(inode->blocks[IND_BLOCK]);
    for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
      if (ind_block[i] != 0) {
        free_block(ind_block[i]);
      }
    }
    free_block(inode->blocks[IND_BLOCK]);
  }
  int idx = inode->num;
  char* i_bitmap = mem + sb->i_bitmap_ptr;
  while (idx > 8) {
    i_bitmap++;
    idx -= 8;
  }
  *i_bitmap &= ~(1 << idx);
}

char get_byte_from_inode(struct wfs_inode* inode, int pos) {
  int block = pos / BLOCK_SIZE;
  int offset = pos % BLOCK_SIZE;
  if (block <= D_BLOCK) {
    char* content = get_dblock_content(inode->blocks[block]);
    printf("read ptr: %p %c\n", content + offset, content[offset]);
    return content[offset];
  } else {
    int* ind_block = (int*)get_dblock_content(inode->blocks[IND_BLOCK]);
    block -= D_BLOCK + 1;
    int idx = ind_block[block];
    char* content = get_dblock_content(idx);
    return content[offset];
  }
}

int set_byte_to_inode(struct wfs_inode* inode, int pos, char byte) {
  int block = pos / BLOCK_SIZE;
  int offset = pos % BLOCK_SIZE;
  if (block <= D_BLOCK) {
    if (inode->blocks[block] == 0) {
      int new_block = get_new_dblock();
      if (new_block < 0) {
        return new_block;
      }
      inode->blocks[block] = new_block; 
    }
    char* content = get_dblock_content(inode->blocks[block]);
    content[offset] = byte;
    printf("write ptr: %p %c\n", content + offset, content[offset]);
  } else {
    if (inode->blocks[IND_BLOCK] == 0) {
      int new_block = get_new_dblock();
      if (new_block < 0) {
        return new_block;
      }
      inode->blocks[IND_BLOCK] = new_block;
      char* ind_block = get_dblock_content(inode->blocks[IND_BLOCK]);
      memset(ind_block, 0, BLOCK_SIZE);
    }
    int* ind_block = (int*)get_dblock_content(inode->blocks[IND_BLOCK]);
    block -= D_BLOCK + 1;
    int idx = ind_block[block];
    if (idx == 0) {
      int new_block = get_new_dblock();
      if (new_block < 0) {
        return new_block;
      }
      idx = new_block;
      ind_block[block] = idx;
    }
    char* content = get_dblock_content(idx);
    content[offset] = byte;
  }
  return 0;
}
