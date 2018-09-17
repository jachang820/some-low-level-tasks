/*
 * NAME: Jonathan Chang
 * EMAIL: j.a.chang820@gmail.com
 * ID: 104853981
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include "ImgReader.h"
#include "print_error.h"


/* function declarations */
int process_args(int, char**);
void print_error(int);


int main(int argc, char *argv[]) {
  struct image *img;
  int img_fd = process_args(argc, argv);
  init_image(img_fd);
  verify_valid_image();
  img = scan_image();
  print_csv();

  /* free heap memory */
  close_image();
  exit(0);
}


int process_args(int argc, char* argv[]) {
  int img_fd;
  char correct_usage[34] =
    "Correct usage: /lab3a image_name\r\n";

  if (argc != 2) {
    fprintf(stderr, "Invalid number of arguments.\r\n");
    fprintf(stderr, "%s", correct_usage);
    exit(1);
  }

  img_fd = open(argv[1], O_RDONLY);
  if (img_fd < 0) {
    print_error(errno);
    fprintf(stderr, "%s", correct_usage);
    exit(1);
  }
  return img_fd;
}


