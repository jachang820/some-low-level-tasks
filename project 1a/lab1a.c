/*
 * NAME: Jonathan Chang
 * EMAIL: j.a.chang820@gmail.com
 * ID: 104853981
 */
#include <stdio.h>      /* for printf                  */
#include <stdlib.h>     /* for exit, atexit            */
#include <unistd.h>     /* for read, write, pipe, exec */
#include <getopt.h>     /* for getopt_long             */
#include <termios.h>    /* for tcgetattr, tcsetattr    */
#include <errno.h>
#include <string.h>     /* for strerror                */
#include <stdbool.h>
#include <poll.h>       /* for poll                    */
#include <sys/types.h>
#include <sys/wait.h>   /* for waitpid                 */
#include <signal.h>     /* for signal, kill            */


struct termios orig_attr, new_attr;
typedef enum { ATEXIT, PIPE, FORK, EXEC, KILL, WAIT, SETATTR } FuncName;
int rw_pipefd[2], wr_pipefd[2];
pid_t terminal_pid, bash_pid;

bool process_option(int, char**);
void noncanonical_settings(void);
void restore_orig_settings(void);
void close_pipes(void);
void SIGPIPE_handler(int);
void shell_status(void);
void check(int, FuncName);

int main(int argc, char* argv[]) {

  bool pass_shell = process_option(argc, argv);
  /* save existing terminal settings */
  tcgetattr(STDIN_FILENO, &orig_attr);
 
  enum Source {
    FROM_KEYBOARD = 0,
    FROM_SHELL = 1
  };

  const size_t BUFSIZE = 256;
  ssize_t num_chars;
  int i, n;
  char input;
  char buf[BUFSIZE];
  struct pollfd fds[2];
  int num_processes = 1;
  terminal_pid = getpid();

  /* Restore original terminal upon process termination */
  check(atexit(restore_orig_settings), ATEXIT);

  /* fork if --shell specified */
  if (pass_shell) {
    num_processes = 2;

    /* create pipes between terminal and shell */
    check(pipe(rw_pipefd), PIPE);
    check(pipe(wr_pipefd), PIPE);

    /* create the shell process */
    check((bash_pid = fork()), FORK);

    if (bash_pid == 0) {      /* shell process */
      close(wr_pipefd[1]);    /* shell reads from this pipe    */
      close(rw_pipefd[0]);    /* shell writes to this pipe     */
      dup2(wr_pipefd[0], STDIN_FILENO);
      dup2(rw_pipefd[1], STDOUT_FILENO);
      dup2(rw_pipefd[1], STDERR_FILENO);
      close(wr_pipefd[0]);
      close(rw_pipefd[1]);
      check(execvp("/bin/bash", NULL), EXEC);
    }
    if (bash_pid > 0) {       /* terminal process */
      close(wr_pipefd[0]);    /* terminal writes to this pipe  */
      close(rw_pipefd[1]);    /* terminal reads from this pipe */
    }
  }
  else {
    rw_pipefd[0] = -1;
  }
  
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;
  fds[1].fd = rw_pipefd[0];
  fds[1].events = POLLIN;

  /* if successfully called, exec() doesn't return, so the code
   * below only runs in terminal thread */
  
  if (signal(SIGPIPE, SIGPIPE_handler) == SIG_ERR) {
    fprintf(stderr,
	    "An error occurred while setting up a signal handler. %s\n",
	    strerror(errno));
  }

  noncanonical_settings();
  
  while (1) {
    poll(fds, num_processes, 0);
    
    /* poll keyboard and shell */
    for (n = 0; n < num_processes;  n++) {
      /* pending input exists */
      if (fds[n].revents & POLLIN) {
        num_chars = read(fds[n].fd, &buf, BUFSIZE);
        /* output from terminal */
        for (i = 0; i < num_chars; i++) {
          input = buf[i];
          if (input == '\r' || input == '\n') {
            write(STDOUT_FILENO, "\r\n", 2);
	    /* write to shell */
            if (n == FROM_KEYBOARD && pass_shell) 
	      write(wr_pipefd[1], "\n", 1);
          }
          else if (input == 3) { /* CTRL+C */
	    write(STDOUT_FILENO, "^C", 2);
	    if (pass_shell)
	      check(kill(bash_pid, SIGINT), KILL);
	  }
	  else if (input == 4) { /* CTRL+D */
	    write(STDOUT_FILENO, "^D", 2);
	    if (pass_shell) shell_status(); /* ends process */
	    else exit(0);
	  }
	  else if (input == 127) { /* DEL */
	    write(STDOUT_FILENO, &input, 1);
	  }
          else {
            write(STDOUT_FILENO, &input, 1);
            /* write to shell */
	     if (n == FROM_KEYBOARD && pass_shell) 
	       write(wr_pipefd[1], &input, 1);
          }
        }
      }
      else if (fds[n].revents & (POLLHUP | POLLERR)) {
        shell_status(); /* ends process */
      }
    }
  }

  tcflush(STDIN_FILENO, TCIFLUSH);
  check(tcsetattr(STDIN_FILENO, TCSANOW, &orig_attr), SETATTR);
  exit(1);
}

bool process_option(int argc, char* argv[]) {
  int opt, longindex;
  bool pass_shell = false;

  while(1) {
    /* getopt_long() stores the option index here */
    longindex = 0;

    /* flag 0 sets return of getopt_long() to val
     *  {name, arg_requirement, flag, val}
     */
    static struct option longopt[] = {
      {"shell", no_argument,    0,   's'  },
      {0,       0,              0,    0   }
    };

    /* optstring is "" since no short options */
    opt = getopt_long(argc, argv, "", longopt, &longindex);

    /* end of options index */
    if (opt == -1)
      break;

    switch (opt) {
    case 0:          /* no flag options */
      break;

    case 's':
      pass_shell = true;
      break;

    default:
      fprintf(stderr, "Unrecognized option. The correct usage is:\n"
	      "/lab1a --shell");
      exit(1);       /* unrecognized argument */
    }
  }

  /* any remaining options leftover */
  if (optind < argc) {
    fprintf(stderr, "Invalid arguments: ");
    while (optind < argc)
      fprintf(stderr, "%s ", argv[optind++]);
    fprintf(stderr, ". The correct usage is:\n"
	    "/lab1a --shell");
    exit(1);
  }
  return pass_shell;
}

void noncanonical_settings() {
  new_attr = orig_attr;
  new_attr.c_iflag = ISTRIP;          /* only lower 7 bits */
  new_attr.c_oflag = 0;               /* no processing     */
  new_attr.c_lflag = 0;               /* no processing     */
  new_attr.c_cc[VMIN] = 1;
  new_attr.c_cc[VTIME] = 0;

  /* flush data received but not read */
  tcflush(STDIN_FILENO, TCIFLUSH);
  /* non-canonical mode with no echo  */
  check(tcsetattr(STDIN_FILENO, TCSANOW, &new_attr), SETATTR);
}

void restore_orig_settings(void) {
  tcflush(STDIN_FILENO, TCIFLUSH);
  check(tcsetattr(STDIN_FILENO, TCSANOW, &orig_attr), SETATTR);
}

void close_pipes(void) {
  close(wr_pipefd[1]); /* terminal writes to this pipe */
  close(rw_pipefd[0]); /* terminal reads from this pipe */
}

void SIGPIPE_handler(int signum) {
  shell_status();
}

void shell_status() {
  int status;
  close_pipes();
  check(waitpid(bash_pid, &status, 0), WAIT);
  int exit_signal = status & 0x007f;
  int exit_status = (status & 0xff00) >> 8;
  fprintf(stderr,
	  "SHELL EXIT SIGNAL=%d STATUS=%d\n", exit_signal, exit_status);
  if (WIFEXITED(status)) exit(0);
  else exit(1);
}

void check(int code, FuncName name) {
  if (code < 0) {
    switch (name) {
    case ATEXIT :
      fprintf(stderr,
	      "An error occurred while registering functions to be called "
	      "at process termination. %s\n", 
	      strerror(errno));
      break;
    case PIPE :
      fprintf(stderr,
	      "An error occurred while creating pipes for interprocess "
	      "communication. %s\n",
	      strerror(errno));
      break;
    case FORK :
      fprintf(stderr,
	      "An error occurred while forking the terminal process. %s\n",
	      strerror(errno));
      break;
    case EXEC :
      fprintf(stderr,
	      "An error occurred while creating the shell process. %s\n",
	      strerror(errno));
      break;
    case KILL :
      fprintf(stderr,
	      "An error occurred while sending SIGINT to the shell. %s\n",
	      strerror(errno));
      break;
    case WAIT :
      fprintf(stderr,
	      "An error occurred while waiting for the termination "
	      "status of the shell process. %s\n",
	      strerror(errno));
      break;
    case SETATTR :
      fprintf(stderr,
	      "An error occurred while setting the terminal attribute. %s\n",
	      strerror(errno));
    }
    exit(1);
  }
}
