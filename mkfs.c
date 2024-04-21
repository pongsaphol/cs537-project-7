#include <sys/mman.h>
#include <sys/stat.h>
#include "utility.h"

char* mem;
struct wfs_sb* sb;

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

int main(int argc, char* argv[]) {
  if (parse_args(argc, argv) != 0)
		return -1;
  init_mem(disk, 1);

  inode = ((inode + 31) >> 5) << 5;
  block = ((block + 31) >> 5) << 5;

  int inode_bit_count = 0;
  while (inode_bit_count < inode) {
    inode_bit_count += 8;
  }

  int dnote_bit_count = 0;
  while (dnote_bit_count < block) {
    dnote_bit_count += 8;
  }

  sb->num_inodes = inode;
  sb->num_data_blocks = block;
  sb->i_bitmap_ptr = (int)sizeof(struct wfs_sb);
  sb->d_bitmap_ptr = sb->i_bitmap_ptr + (inode_bit_count + 7) / 8;
  sb->i_blocks_ptr = sb->d_bitmap_ptr + (dnote_bit_count + 7) / 8;
  sb->d_blocks_ptr = sb->i_blocks_ptr + inode * BLOCK_SIZE;

  int new_inode = get_new_inode();
  struct wfs_inode* inode = get_inode_content(new_inode);
  inode->mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR;
  inode->nlinks = 2;
}