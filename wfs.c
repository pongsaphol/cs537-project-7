#include "wfs.h"
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

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

static int wfs_getattr(const char *path, struct stat *stbuf) {

  return 0; // Return 0 on success
}

static struct fuse_operations ops = {
  .getattr = wfs_getattr,
  // .mknod   = wfs_mknod,
  // .mkdir   = wfs_mkdir,
  // .unlink  = wfs_unlink,
  // .rmdir   = wfs_rmdir,
  // .read    = wfs_read,
  // .write   = wfs_write,
  // .readdir = wfs_readdir,
};

int main(int argc, char *argv[]) {
  // Initialize FUSE with specified operations
  // Filter argc and argv here and then pass it to fuse_main
  init_mem(argv[1]);
  return fuse_main(argc - 1, argv + 1, &ops, NULL);
}
