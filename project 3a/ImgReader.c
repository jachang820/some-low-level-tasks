/*
 * NAME: Jonathan Chang
 * EMAIL: j.a.chang820@gmail.com
 * ID: 104853981
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "ImgReader.h"
#include "print_error.h"


static const int BLOCK_SIZE = 1024;
static const int GROUP_DESC_SIZE = 32;
static const int INODE_SIZE = 128;
static const int DIRENT_SIZE = 8;
static const int INDIRECT_SIZE = 18;
static int fd;               /* image file descriptor     */
static int block_offset = 0; /* offset for a section      */

struct image *img;    /* entire structure of image */


enum datatypes {
  BYTE = 256, INT16, INT32,
  SBLOCK, GROUP, INODE, DIRENT, INDIRECT};


static void set_offset(int);
static void img_read(void*, int, enum datatypes);
static void read_superblock(void);
static void read_group(void);
static int* read_free_list(int*, int);
static void read_bfree(void);
static void read_ifree(void);
static void read_inode(void);
static void read_dirent(void);
static void read_indirect(void);
static void indirect_recursive(int, int, int, int, int*);


struct image* scan_image(void) {
  read_superblock();
  read_group();
  read_bfree();
  read_ifree();
  read_inode();
  read_dirent();
  read_indirect();
  return img;
}


void init_image(int img_fd) {
  fd = img_fd;
  img = calloc(1, sizeof(struct image));
}


void verify_valid_image(void) {
  struct ext2_super_block *s = &img->sblock;
  set_offset(BLOCK_SIZE);
  img_read(&s->s_magic, 56, INT16);
  if (s->s_magic != 0xEF53) {
    fprintf(stderr, "Image does not contain valid signature.\r\n");
    fprintf(stderr, "Should be 0xEF53. %X detected.\r\n", s->s_magic); 
    exit(2);
  }
}


void close_image(void) {
  free(img->ifree);
  free(img->bfree);
  free(img->group);
  free(img->inode);
  free(img->dirent);
  free(img->indirect);
  free(img->inode_num);
  free(img->dirent_parent);
  free(img);
  close(fd);
}


static void set_offset(int offset) {
  block_offset = offset;
}


static void img_read(void * dest, int offset, enum datatypes type) {
  int size;
  if (type == BYTE) size = 1;
  else if (type == INT16) size = 2;
  else if (type == INT32) size = 4;
  else if (type == SBLOCK) size = BLOCK_SIZE;
  else if (type == GROUP) size = GROUP_DESC_SIZE;
  else if (type == INODE) size = INODE_SIZE;
  else if (type == DIRENT) size = DIRENT_SIZE;
  else if (type == INDIRECT) size = INDIRECT_SIZE;
  else size = type;

  void * buf = calloc(1, size);
  int bytes = pread(fd, buf, size, block_offset + offset);
  if (bytes < 0) {
    print_error(errno);
    free(buf);
    exit(2);
  }
  
  if (type == BYTE) *(__uint8_t*)dest = *(__uint8_t*)buf;
  else if (type == INT16) *(__uint16_t*)dest = *(__uint16_t*)buf;
  else if (type == INT32) *(__uint32_t*)dest = *(__uint32_t*)buf;
  else if (type == SBLOCK)
    *(struct ext2_super_block*)dest = *(struct ext2_super_block*)buf;
  else if (type == GROUP)
    *(struct ext2_group_desc*)dest = *(struct ext2_group_desc*)buf;  
  else if (type == INODE)
    *(struct ext2_inode*)dest = *(struct ext2_inode*)buf;
  else if (type == DIRENT)
    *(struct ext2_dir_entry*)dest = *(struct ext2_dir_entry*)buf;
  else if (type == INDIRECT)
    *(struct ext2_indirect*)dest = *(struct ext2_indirect*)buf;
  else strncpy((unsigned char*)dest, (unsigned char*)buf, bytes);
  free(buf);
}


static void read_superblock(void) {
  struct ext2_super_block *s = &img->sblock;
  set_offset(BLOCK_SIZE);
  img_read(s, 0, SBLOCK);
}


static void read_group(void) {
  struct ext2_super_block *s = &img->sblock;
  img->num_groups = 1 + (s->s_blocks_count - 1) / s->s_blocks_per_group;
  img->group = calloc(img->num_groups, sizeof(struct ext2_group_desc));
  
  int i;
  for (i = 0; i < img->num_groups; i++) {
    set_offset(2*BLOCK_SIZE + i*GROUP_DESC_SIZE);
    img_read(&img->group[i], 0, GROUP);
  }
}


static void read_bfree(void) {
  int size;
  img->bfree = read_free_list(&size, 1);
  img->num_bfree = size;
}


static void read_ifree(void) {
  int size;
  img->ifree = read_free_list(&size, 0);
  img->num_ifree = size;
}


static int * read_free_list(int *size, int is_bfree) {
  int i, j, k, free_count = 0;
  int byte, mask;
  int bitmap, frees, per_group;

  per_group = (is_bfree) ?
    img->sblock.s_blocks_per_group :
    img->sblock.s_inodes_per_group;
  
  int * free_list = calloc(0, sizeof(int));

  for (i = 0; i < img->num_groups; i++) {
    frees = (is_bfree) ?
      img->group[i].bg_free_blocks_count :
      img->group[i].bg_free_inodes_count;
    bitmap = (is_bfree) ?
      img->group[i].bg_block_bitmap :
      img->group[i].bg_inode_bitmap;
    set_offset(bitmap * BLOCK_SIZE);

    for(j = 0; j < per_group; j++) {
      byte = 0;
      mask = 1;
      img_read(&byte, j, BYTE);
      for (k = 0; k < 8; k++) {
	if (!(byte & mask)) {
	  free_count++;
	  free_list = realloc(free_list, free_count*sizeof(int));
	  free_list[free_count-1] = i*per_group + j*8 + k + 1;
	}
	if (free_count >= frees) {
	  *size = free_count;
	  return free_list;
	}
	mask <<= 1;
      }
    }
  }
  *size = free_count;
  return free_list;
}


static void read_inode(void) {
  int i, j, count = 0;
  img->inode = calloc(0, sizeof(struct ext2_inode));
  img->inode_num = calloc(0, sizeof(int));
  
  int inodes_per_table, inode_count;
  inodes_per_table = img->sblock.s_inodes_per_group;
  inode_count = img->sblock.s_inodes_count;

  int table, mode = 0, link_count = 0;
  for (i = 0; i < img->num_groups; i++) {
    for (j = 0; j < inodes_per_table; j++) {
      if (i*inodes_per_table+j < inode_count) {
	table = img->group[i].bg_inode_table;
	set_offset(table*BLOCK_SIZE + j*INODE_SIZE);
	img_read(&mode, 0, INT16);
	img_read(&link_count, 26, INT16);
	if (mode > 0 && link_count > 0) {
	  count++;
	  img->inode = realloc(img->inode, count*sizeof(struct ext2_inode));
	  img->inode_num = realloc(img->inode_num, count*sizeof(int));
	  img_read(&img->inode[count-1], 0, INODE);
	  img->inode_num[count-1] = j + 1;
	}
      }
    }
  }
  img->num_inodes = count;
}


static void read_dirent(void) {
  int i, j;
  int mode = 0;
  int read_block = 0;
  int count = 0;
  int offset, feeler;
  img->dirent = calloc(0, sizeof(struct ext2_dir_entry));
  img->dirent_parent = calloc(0, sizeof(int));
  
  for (i = 0; i < img->num_inodes; i++) {
    mode = img->inode[i].i_mode & 0xF000;
    if (mode == 0x4000) { /* directory */
      for (j = 0; j < 12; j++) {
	read_block = img->inode[i].i_block[j];
	if (read_block > 0) {
	  offset = 0;
	  set_offset(read_block * BLOCK_SIZE);
	  img_read(&feeler, 0, INT32);
	  while (feeler != 0 && offset < BLOCK_SIZE) {
	    count++;
	    img->dirent = realloc(img->dirent,
				  count*sizeof(struct ext2_dir_entry));
	    img->dirent_parent = realloc(img->dirent_parent,
					 count*sizeof(int));
	    img_read(&img->dirent[count-1], 0, DIRENT);
	    img_read(img->dirent[count-1].name, DIRENT_SIZE,
		     img->dirent[count-1].name_len);
	    img->dirent_parent[count-1] = img->inode_num[i];
	    offset += img->dirent[count-1].rec_len;
	    img_read(&feeler, img->dirent[count-1].rec_len, INT32);
	    set_offset(read_block * BLOCK_SIZE + offset);
	  }
	}
      }
    }
  }
  img->num_dirents = count;
}


static void read_indirect(void) {
  int i, j, count = 0;
  int read_block;
  img->indirect = calloc(0, sizeof(struct ext2_indirect));
  
  for (i = 0; i < img->num_inodes; i++) {    
    for (j = 12; j < 15; j++) {
      read_block = img->inode[i].i_block[j];
      indirect_recursive(read_block,
			 j - 11,
			 img->inode_num[i],
			 0,
			 &count);
    }
  }
  img->num_indirects = count;
}


static void indirect_recursive(int block, int level, int inode,
			       int logical, int *count) {
  int root = block;
  int offset = 0;
  if (logical == 0) {
    if (level == 3) logical = 256*256 + 256;
    if (level == 2) logical = 256;
    logical += 12;
  }
  while (offset < BLOCK_SIZE) {
    set_offset(root * BLOCK_SIZE);
    img_read(&block, offset, INT32);
    if (block > 0) {
      (*count)++;
      img->indirect = realloc(img->indirect,
			      *count*sizeof(struct ext2_indirect));
      img->indirect[*count-1].inode = inode;
      img->indirect[*count-1].indirect_level = level;
      img->indirect[*count-1].block_offset = logical;
      img->indirect[*count-1].indirect_block = root;
      img->indirect[*count-1].referenced_block = block;

      if (level > 1)
	indirect_recursive(block, level - 1, inode, logical, count);
    }
    offset += 4;
    logical++;
  }
}
