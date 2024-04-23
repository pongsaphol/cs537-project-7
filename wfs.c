#include "utility.h"
#include <fuse.h>

char* mem;
struct wfs_sb* sb;


static int wfs_getattr(const char *path, struct stat *stbuf) {
  printf("GETATTR %s\n", path);
  printf("%d\n", (int) sizeof(struct wfs_dentry));
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
  printf("MKDIR %s\n", path);
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

  if (check_duplicate(inodes, name) < 0) {
    return -EEXIST;
  }

  int new_inode = get_new_inode();
  struct wfs_inode* new_inodes = get_inode_content(new_inode);
  new_inodes->mode = S_IFDIR | mode;

  struct wfs_dentry* new_dentry;

  int num = get_next_free_dentry(inodes, &new_dentry);
  if (num < 0) {
    return num;
  }

  inodes->size += num;

  strcpy(new_dentry->name, name);
  new_dentry->num = new_inode;

  return 0;
}

int wfs_rmdir(const char* path) {
  printf("RMDIR %s\n", path);
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
  for (int i = 0; i <= D_BLOCK; ++i) {
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

int wfs_mknod(const char* path, mode_t mode, dev_t rdev) {
  printf("MKNOD %s\n", path);
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
  // print_inode_stat(inodes);

  if (!(inodes->mode & S_IFDIR)) {
    return -ENOENT;
  }

  if (check_duplicate(inodes, name) < 0) {
    return -EEXIST;
  }

  int new_inode = get_new_inode();
  
  struct wfs_inode* new_inodes = get_inode_content(new_inode);
  new_inodes->mode = mode;

  struct wfs_dentry* new_dentry;
  int num = get_next_free_dentry(inodes, &new_dentry);
  if (num < 0) {
    return num;
  }

  inodes->size += num;
  strcpy(new_dentry->name, name);
  new_dentry->num = new_inode;
  
  return 0;
}

int wfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
  printf("READ %s\n", path);
  int inode = get_inode(path);
  if (inode < 0) {
    return inode;
  }
  struct wfs_inode* inodes = get_inode_content(inode);
  if (offset > inodes->size) {
    return 0;
  }
  if (offset + size > inodes->size) {
    size = inodes->size - offset;
  }
  for (int i = 0; i < size; ++i) {
    buf[i] = get_byte_from_inode(inodes, offset + i);
  }
  return size;
}

int wfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
  printf("WRITE %s\n", path);
  int inode = get_inode(path);
  if (inode < 0) {
    return inode;
  }

  struct wfs_inode* inodes = get_inode_content(inode);
  
  if (offset > inodes->size) {
    return 0;
  }

  for (int i = 0; i < size; ++i) {
    set_byte_to_inode(inodes, offset + i, buf[i]);
    if (offset + i >= inodes->size) {
      inodes->size = offset + i + 1;
    }
  }
  return size;
}

int wfs_unlink(const char* path) {
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
  if (del_inode->mode & S_IFDIR) {
    return -EISDIR;
  }
  for (int i = 0; i <= D_BLOCK; ++i) {
    if (inodes->blocks[i] == 0) {
      continue;
    }
    struct wfs_dentry* entry = get_dentry(inodes->blocks[i]);
    if (strcmp(entry->name, name) == 0) {
      free_block(inodes->blocks[i]);
      inodes->blocks[i] = 0;
      inodes->size -= BLOCK_SIZE;

      del_inode->nlinks--;
      if (del_inode->nlinks == 0) {
        free_inode(del_inode);
      }
      return 0;
    }
  }
  return 1;
}

// int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {

// }

static struct fuse_operations ops = {
  .getattr = wfs_getattr,
  .mknod   = wfs_mknod,
  .mkdir   = wfs_mkdir,
  .unlink  = wfs_unlink,
  .rmdir   = wfs_rmdir,
  .read    = wfs_read,
  .write   = wfs_write,
  // .readdir = wfs_readdir,
};

int main(int argc, char *argv[]) {
  // Initialize FUSE with specified operations
  // Filter argc and argv here and then pass it to fuse_main
  init_mem(argv[1], 0);
  return fuse_main(argc - 1, argv + 1, &ops, NULL);
}
