#include "utility.h"
#include <fuse.h>

char* mem;
struct wfs_sb* sb;


static int wfs_getattr(const char *path, struct stat *stbuf) {
  printf("GETATTR %s\n", path);
  // print_inode_stat(get_inode_content(0));
  int inode = get_inode(path);
  if (inode < 0) {
    return inode;
  }
  struct wfs_inode* inodes = get_inode_content(inode);
  memset(stbuf, 0, sizeof(struct stat));
  stbuf->st_uid = inodes->uid;
  stbuf->st_gid = inodes->gid;
  stbuf->st_atime = inodes->atim;
  stbuf->st_mtime = inodes->mtim;
  stbuf->st_mode = inodes->mode;
  stbuf->st_size = inodes->size;
  return 0; // Return 0 on success
}

static int wfs_mkdir(const char* path, mode_t mode) {
  char* path_copy = strdup(path);
  char** tokens = parse_path(path_copy);
  int last = -1;
  while (tokens[last + 1] != NULL) {
    last++;
  }
  if (last == -1) {
    return -ENOENT;
  }
  char* name = tokens[last];
  tokens[last] = NULL;
  int parent = get_inode_rec(tokens, 0);
  if (parent < 0) {
    return parent;
  }
  struct wfs_inode* inodes = get_inode_content(parent);


  int num_free = -1;
  // check duplicate
  for (int i = 0; i < D_BLOCK; ++i) {
    if (inodes->blocks[i] == 0) {
      if (num_free == -1) {
        num_free = i;
      }
      continue;
    }
    struct wfs_dentry* entry = get_dentry(inodes->blocks[i]);
    if (strcmp(entry->name, name) == 0) {
      return -EEXIST;
    }
  }


  // not enough space
  if (num_free == -1) {
    return -ENOSPC;
  }

  int new_inode = get_new_inode();
  if (new_inode < 0) {
    return new_inode;
  }
  struct wfs_inode* new_inodes = get_inode_content(new_inode);
  new_inodes->mode = S_IFDIR | mode;
  int new_dentry_block = get_new_dblock();
  if (new_dentry_block < 0) {
    return new_dentry_block;
  }

  inodes->blocks[num_free] = new_dentry_block;
  struct wfs_dentry* new_dentry = get_dentry(inodes->blocks[num_free]);
  strcpy(new_dentry->name, name);
  new_dentry->num = new_inode;

  inodes->size += BLOCK_SIZE;
  return 0;
}

int wfs_rmdir(const char* path) {
  char* path_copy = strdup(path);
  char** tokens = parse_path(path_copy);
  int last = -1;
  while (tokens[last + 1] != NULL) {
    last++;
  }
  if (last == -1) {
    return -ENOENT;
  }
  char* name = tokens[last];
  tokens[last] = NULL;

  struct wfs_inode* inodes = get_inode_content(get_inode_rec(tokens, 0));
  struct wfs_inode* del_inode = get_inode_content(get_inode(path));
  if (!(del_inode->mode & S_IFDIR)) {
    return -ENOTDIR;
  }
  if (del_inode->size != 0) {
    return -ENOTEMPTY;
  }
  for (int i = 0; i < D_BLOCK; ++i) {
    if (inodes->blocks[i] == 0) {
      continue;
    }
    struct wfs_dentry* entry = get_dentry(inodes->blocks[i]);
    if (strcmp(entry->name, name) == 0) {
      free_block(inodes->blocks[i]);
      inodes->blocks[i] = 0;
      inodes->size -= BLOCK_SIZE;
      return 0;
    }
  }
  return 1;
}

static struct fuse_operations ops = {
  .getattr = wfs_getattr,
  // .mknod   = wfs_mknod,
  .mkdir   = wfs_mkdir,
  // .unlink  = wfs_unlink,
  .rmdir   = wfs_rmdir,
  // .read    = wfs_read,
  // .write   = wfs_write,
  // .readdir = wfs_readdir,
};

int main(int argc, char *argv[]) {
  // Initialize FUSE with specified operations
  // Filter argc and argv here and then pass it to fuse_main
  init_mem(argv[1], 0);
  return fuse_main(argc - 1, argv + 1, &ops, NULL);
}
