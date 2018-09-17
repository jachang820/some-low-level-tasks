/*
 * NAME: Jonathan Chang
 * EMAIL: j.a.chang820@gmail.com
 * ID: 104853981
 */

#include "ext2_fs.h"


struct ext2_indirect {
  __uint32_t inode;            /* inode number of owning file */
  __uint16_t indirect_level;   /* scanned block level of indirection */
  __uint32_t block_offset;     /* logical block offset */
  __uint32_t indirect_block;   /* indirect block number */
  __uint32_t referenced_block; /* referenced block number */
};


struct image {
  struct ext2_super_block sblock;
  struct ext2_group_desc *group;
  struct ext2_inode *inode;
  struct ext2_dir_entry *dirent;
  struct ext2_indirect *indirect;
  int *bfree;
  int *ifree;
  int num_groups;
  int num_bfree;
  int num_ifree;
  int num_inodes;
  int num_dirents;
  int num_indirects;
  int *inode_num;
  int *dirent_parent;
};


/* read */
void init_image(int);
void verify_valid_image(void);
struct image* scan_image(void);
void close_image(void);

/* write */
void print_csv(void);
