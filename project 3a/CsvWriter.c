/*
 * NAME: Jonathan Chang
 * EMAIL: j.a.chang820@gmail.com
 * ID: 104853981
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "ImgReader.h"

extern struct image *img;

static int fd;

void print_csv(void);
static void print_superblock(void);
static void print_group(void);
static void print_bfree(void);
static void print_ifree(void);
static void print_inode(int);
static void print_dirent(int);
static void print_indirect(int);


void print_csv(void) {
  print_superblock();
  print_group();
  print_bfree();
  print_ifree();

  int i;
  for (i = 1; i <= img->sblock.s_inodes_count; i++) {
    print_inode(i);
    print_dirent(i);
    print_indirect(i);
  }
}


static void print_superblock(void) {
  char buf[64];
  memset(buf, 0, 64);
  sprintf(buf, "SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n",
	  img->sblock.s_blocks_count,
	  img->sblock.s_inodes_count,
	  1024 << img->sblock.s_log_block_size,
	  img->sblock.s_inode_size,
	  img->sblock.s_blocks_per_group,
	  img->sblock.s_inodes_per_group,
	  img->sblock.s_first_ino);
  
  fprintf(stdout, "%s", buf);
}


static void print_group(void) {
  char buf[64];
  memset(buf, 0, 64);
  int i;
  int n_blocks = img->sblock.s_blocks_count;
  int n_inodes = img->sblock.s_inodes_count;
  int blocks_per = img->sblock.s_blocks_per_group;
  int inodes_per = img->sblock.s_inodes_per_group;
  int current_blocks, current_inodes;
  for (i = 0; i < img->num_groups; i++) {
    current_blocks = (n_blocks >= blocks_per) ?
      blocks_per : n_blocks;
    current_inodes = (n_inodes >= inodes_per)?
      inodes_per : n_inodes;
    n_blocks -= blocks_per;
    n_inodes -= inodes_per;
    
    sprintf(buf, "GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",
	    i,
	    current_blocks,
	    current_inodes,
	    img->group[i].bg_free_blocks_count,
	    img->group[i].bg_free_inodes_count,
	    img->group[i].bg_block_bitmap,
	    img->group[i].bg_inode_bitmap,
	    img->group[i].bg_inode_table);
    
    fprintf(stdout, "%s", buf);
  }
}


static void print_bfree(void) {
  char buf[16];
  memset(buf, 0, 16);
  int i;
  for (i = 0; i < img->num_bfree; i++) {
    sprintf(buf, "BFREE,%d\n", img->bfree[i]);
    fprintf(stdout, "%s", buf);
  }
}


static void print_ifree(void) {
  char buf[16];
  memset(buf, 0, 16);
  int i;
  for (i = 0; i < img->num_ifree; i++) {
    sprintf(buf, "IFREE,%d\n", img->ifree[i]);
    fprintf(stdout, "%s", buf);
  }
}


static void print_inode(int inode) {
  char buf[255];
  char *atime, *crtime, *mtime;
  memset(buf, 0, 255);
  atime = calloc(255, sizeof(char));
  mtime = calloc(255, sizeof(char));
  crtime = calloc(255, sizeof(char));
  long a, c, m;
  int format = 0;
  char format_symbol;
  int len;
  char text[84] = "INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d";

  int i, j;
  for (i = 0; i < img->num_inodes; i++) {
    if (img->inode_num[i] == inode) {
      format = img->inode[i].i_mode;
      format &= 0xF000;
      if (format == 0x4000) format_symbol = 'd'; /* directory */
      else if (format == 0x8000) format_symbol = 'f'; /* file */
      else if (format == 0xA000) format_symbol = 's'; /* symbolic */
      else format_symbol = '?'; /* other */

      a = (long) img->inode[i].i_atime;
      m = (long) img->inode[i].i_mtime;
      c = (long) img->inode[i].i_ctime;
      strftime(crtime, 255, "%D %T", gmtime(&c));
      strftime(mtime, 255, "%D %T", gmtime(&m));
      strftime(atime, 32, "%D %T", gmtime(&a));
    
      len = sprintf(buf, text,
		    img->inode_num[i],
		    format_symbol,
		    img->inode[i].i_mode % 4096,
		    img->inode[i].i_uid,
		    img->inode[i].i_gid,
		    img->inode[i].i_links_count,
		    crtime,
		    mtime,
		    atime,
		    img->inode[i].i_size,
		    img->inode[i].i_blocks);

      if (format_symbol == 's') {
	len += sprintf(buf+len, ",%d", img->inode[i].i_block[0]);
      }
      else {
	for (j = 0; j < 15; j++) {
	  len += sprintf(buf+len, ",%d", img->inode[i].i_block[j]);
	}
      }
      len += sprintf(buf+len, "\n");
    
      fprintf(stdout, "%s", buf);
    }
  }
  free(atime);
  free(mtime);
  free(crtime);
}


static void print_dirent(int inode) {
  char buf[300];
  char file[255];
  memset(buf, 0, 300);
  int offset = 0;
  
  int i;
  for (i = 0; i < img->num_dirents; i++) {
    if (img->dirent_parent[i] == inode) {
      memset(file, 0, 255);
      strncpy(file, img->dirent[i].name, img->dirent[i].name_len);
      sprintf(buf, "DIRENT,%d,%d,%d,%d,%d,'%s'\n",
	      img->dirent_parent[i],
	      offset,
	      img->dirent[i].inode,
	      img->dirent[i].rec_len,
	      img->dirent[i].name_len,
	      file);
      offset += img->dirent[i].rec_len;

      fprintf(stdout, "%s", buf);
    }
  }
}


static void print_indirect(int inode) {
  char buf[32];
  memset(buf, 0, 32);

  int i;
  for (i = 0; i < img->num_indirects; i++) {
    if (img->indirect[i].inode == inode) {
      sprintf(buf, "INDIRECT,%d,%d,%d,%d,%d\n",
	      img->indirect[i].inode,
	      img->indirect[i].indirect_level,
	      img->indirect[i].block_offset,
	      img->indirect[i].indirect_block,
	      img->indirect[i].referenced_block);

      fprintf(stdout, "%s", buf);
    }
  }
}
