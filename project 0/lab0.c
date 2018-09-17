/*
NAME: Jonathan Chang
EMAIL: j.a.chang820@gmail.com
ID: 104853981
*/

#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <unistd.h>    /* for read, write, exit */
#include <getopt.h>    /* for getopt_long */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>     /* for open, creat, dup */
#include <signal.h>    /* for signal */
#include <errno.h>
#include <stdbool.h>   /* for bool */
#include <string.h>    /* for strerror */ 

void sighandler(int);
void segfault(void);

int main(int argc, char *argv[]) {
  int opt, longindex;
  int in_fd, out_fd; // file descriptors
  char *in_file = (char*) 0;
  char *out_file = (char*) 0;
  bool seg_option = false;

  while(1) {

    // getopt_long() stores the option index here
    longindex = 0;

    // flag 0 sets return of getopt_long() to val
    //{name,       arg_requirement, flag, val}
    static struct option longopt[] = {
      {"input",    required_argument, 0, 'i' },
      {"output",   required_argument, 0, 'o' },
      {"segfault", no_argument,       0, 's' },
      {"catch",    no_argument,       0, 'c' },
      {0,          0,                 0,  0  }
    };

    // optstring is "" since no short options
    opt = getopt_long(argc, argv, "", longopt, &longindex);

    // end of options index
    if (opt == -1)
      break;

    switch (opt) {
    case 0:       // no flag options
      break;

    case 'i':     // --input=filename
      in_file = optarg;
      break;

    case 'o':     // --output=filename
      out_file = optarg;
      break;

    case 's':     // --segfault
      seg_option = true;
      break;

    case 'c':     // --catch
      signal(SIGSEGV, sighandler);
      break;
    
    default:
      fprintf(stderr, "Unrecognized options. The correct usage is:\n"
	      "/lab0 --input=filename --output=filename --segfault --catch");
      exit(1);    // unrecognized argument
    }
  }

  // any remaining options leftover
  if (optind < argc) {
    fprintf(stderr, "Invalid arguments: ");
    while (optind < argc)
      fprintf(stderr, "%s ", argv[optind++]);
    fprintf(stderr, ". The correct usage is:\n"
	    "/lab0 --input=filename --output=filename --segfault --catch");
    exit(1);
  }

  // input file redirection
  if (in_file) {
    in_fd = open(in_file, O_RDONLY);
    if (in_fd >= 0) {
      close(0);   // closes stdin
      dup(in_fd); // sets stdin to file
      close(in_fd);
    }
    else {
      if (errno == ENOENT)
	fprintf(stderr, "An error has occurred while processing --input=%s.\n"
		"%s", in_file, strerror(errno));
      if (errno == EINVAL)
	fprintf(stderr, "An error has occurred while processing --input=%s.\n"
		"%s", in_file, strerror(errno));
      exit(2);
    }
  }

  // output file redirection
  if (out_file) {
    // 0666 gives read/write permission
    out_fd = creat(out_file, 0666);
    if (out_fd >= 0) {
      close(1);   // closes stdout
      dup(out_fd);// sets stdout to file
      close(out_fd);
    }
    else {
      if (errno == EACCES)
	fprintf(stderr, "An error has occurred while processing --output=%s.\n"
		"%s", out_file,strerror(errno));
      if (errno == EINVAL)
	fprintf(stderr, "An error has occurred while processing --output=%s.\n"
		"%s", out_file, strerror(errno));
      exit(3);
    }
  }

  if (seg_option) {
    segfault();
  }

  ssize_t state;
  size_t file_size = (size_t) lseek(0, 0, SEEK_END);
  lseek(0, 0, SEEK_SET);
  char *buf = (char*) malloc((file_size)*sizeof(char));
  state = read(0, buf, file_size);
  write(1, buf, file_size);
  free(buf);
  exit(0);        // exit success
}

void sighandler(int signum) {
  fprintf(stderr, "Segmentation fault generated has been caught by --catch.");
  exit(4);
}

void segfault() {
  char *manthara = 0;
  *manthara = 'c'; // force seg fault
}
