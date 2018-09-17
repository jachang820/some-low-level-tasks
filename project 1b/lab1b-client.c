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
#include <sys/socket.h> /* for socket                  */
#include <netinet/in.h>
#include <netdb.h>      /* for gethostbyname           */
#include <mcrypt.h>     /* for mcrypt                  */
#include <sys/stat.h>
#include <fcntl.h>      /* for open, close             */

bool is_server;

struct termios orig_attr, new_attr;
typedef enum { ATEXIT, PIPE, FORK, EXEC, KILL, WAIT, SETATTR, 
	       OPEN, KEYSIZE, SOCK, BIND, ACCEPT, CONNECT, SIGNAL, 
	       GETHOST, READ, WRITE, ENCRYPT, DECRYPT} FuncName;
int rw_pipefd[2], wr_pipefd[2];
int sockfd, newsockfd;
MCRYPT encrypt_td, decrypt_td;
struct pollfd fds[2];
int log_fd;
pid_t terminal_pid, bash_pid;

const size_t BUFSIZE = 512;
int blocksize;
char *IV;

struct param_args {
  int port_number;
  char *key;
  char *log_file;
  char *host_name;
};
struct param_args* args_ptr;
typedef bool (*comparator)(int);

const struct param_args process_option(int, char**);
void noncanonical_settings(void);
void restore_orig_settings(void);
void check(int, FuncName, comparator);
void handle_signals(void);
void poll_inputs(int, int, int, int, struct param_args*);
void init_encryption(struct param_args*);
void deinit_encryption(void);
int num_digits(int);
void write_log(void*, int, bool);
void write_to_socket(int, void*, void*, int, bool);
void initiate_shutdown(struct param_args*);
void close_sockets(void);
void sighandler(int);
int calculate_block(int);

/* client specific */
void connect_host(struct param_args*);

/* server specific */
void close_pipes(void);
void establish_shell(void);
void accept_client(struct param_args*);

/* comparators */
bool lt_0(int);
bool not_0(int);
bool is_NULL(int);
bool is_SIG_ERR(int);
bool gt_key_size(int);

int main(int argc, char* argv[]) {

  size_t proc_length = strlen(argv[0]);
  is_server = (argv[0][proc_length - 6] == 's');

  /* Get parameters from argument list */
  struct param_args args = process_option(argc, argv);
  args_ptr = &args;
  
  /* Initialize encryption descriptors */
  init_encryption(&args);

  /* Make terminal into non-canonical mode with no echo */
  noncanonical_settings();

  if (is_server) {

    /* Accept connection to a client */
    accept_client(&args);

    /* Fork a new process to execute shell and set up pipes */
    establish_shell();

    /* Repond to these signals */
    handle_signals();

    /* Poll and respond to inputs */
    poll_inputs(newsockfd, newsockfd, rw_pipefd[0], wr_pipefd[1], &args);
  }
  else {
    /* Establish connection to server */
    connect_host(&args);

    /* Respond to these signals */
    handle_signals();

    /* Poll and respond to inputs */
    poll_inputs(STDIN_FILENO, STDOUT_FILENO, sockfd, sockfd, &args);
  }

  /* this should not be executed! */
  fprintf(stderr, "Unknown error!\r\n");
  exit(1);
}

const struct param_args process_option(int argc, char* argv[]) {
  int opt, longindex;
  bool port_exists = false;
  int key_fp;
  int key_length;
  char *correct_usage;
  char buf[BUFSIZE];

  /* argument defaults */
  int port_number = 0;
  char *key = "";
  char *host_name = "";
  char *log_file = "";

  /* argument options */
  struct option longopt[5];
  struct option opt_p = {"port"   , required_argument, 0, 'p' };
  struct option opt_e = {"encrypt", required_argument, 0, 'e' };
  struct option opt_h = {"host"   , required_argument, 0, 'h' };
  struct option opt_l = {"log"    , required_argument, 0, 'l' };
  struct option opt_0 = {0        , 0                , 0,  0  };

  while(1) {
    /* getopt_long() stores the option index here */
    longindex = 0;

    if (is_server) {
      longopt[0] = opt_p;
      longopt[1] = opt_e;
      longopt[2] = opt_0;
      longopt[3] = opt_0;
      longopt[4] = opt_0; 
    }
    else {
      longopt[0] = opt_p;
      longopt[1] = opt_e;
      longopt[2] = opt_h;
      longopt[3] = opt_l;
      longopt[4] = opt_0; 
    }

    /* optstring is "" since no short options */
    opt = getopt_long(argc, argv, "", longopt, &longindex);

    if (is_server) {
      correct_usage = "/lab1b-server --port=portno --encrypt=filename";
    }
    else {
      correct_usage = "/lab1b-client --port=portno --encrypt=filename"
	" --host=name --log=filename";
    }

    /* end of options index */
    if (opt == -1)
      break;

    switch (opt) {
    case 0:          /* no flag options */
      break;

    case 'p':
      port_number = atoi(optarg);
      break;

    case 'e':
      check((key_fp = open(optarg, O_RDONLY)), OPEN, lt_0);
      key_length = read(key_fp, buf, BUFSIZE);
      key = strndup(buf,key_length);
      close(key_fp);
      break;

    case 'l':
      if (!is_server) {
	log_file = strdup(optarg);
	break;
      }
    case 'h':
      if (!is_server) {
	host_name = strdup(optarg);
	break;
      }
    default:
      fprintf(stderr, "Unrecognized option. The correct usage is:\r\n%s\r\n",
	      correct_usage);
      exit(1);       /* unrecognized argument */
    }
  }

  /* any remaining options leftover */
  if (optind < argc) {
    fprintf(stderr, "Invalid arguments: ");
    while (optind < argc)
      fprintf(stderr, "%s ", argv[optind++]);
    fprintf(stderr, ". The correct usage is:\r\n%s\r\n",
	    correct_usage);
    exit(1);
  }

  /* --port is mandatory */
  if (port_number == 0) {
    fprintf(stderr, "The argument --port=portno is mandatory.\r\n"
	    "Port must be a positive number.\r\n");
    exit(1);
  }

  /* set default */
  if (*host_name == '\0') host_name = strdup("lnxsrv09.seas.ucla.edu");
  if (*key == '\0') key = strdup("");
  if (*log_file == '\0') log_file = strdup("");

  struct param_args args = {port_number, key, log_file, host_name};

  return args;
}

void noncanonical_settings(void) {
  /* save existing terminal settings */
  tcgetattr(STDIN_FILENO, &orig_attr);
 
  /* Restore original terminal upon process termination */
  check(atexit(restore_orig_settings), ATEXIT, lt_0);

  new_attr = orig_attr;
  new_attr.c_iflag = ISTRIP;          /* only lower 7 bits */
  new_attr.c_oflag = 0;               /* no processing     */
  new_attr.c_lflag = 0;               /* no processing     */
  new_attr.c_cc[VMIN] = 1;
  new_attr.c_cc[VTIME] = 0;

  /* flush data received but not read */
  tcflush(STDIN_FILENO, TCIFLUSH);
  /* non-canonical mode with no echo  */
  check(tcsetattr(STDIN_FILENO, TCSANOW, &new_attr), SETATTR, lt_0);
}

void restore_orig_settings(void) {
  tcflush(STDIN_FILENO, TCIFLUSH);
  check(tcsetattr(STDIN_FILENO, TCSANOW, &orig_attr), SETATTR, lt_0);
}

void establish_shell(void) {
  /* create pipes between terminal and shell */
  check(pipe(rw_pipefd), PIPE, lt_0);
  check(pipe(wr_pipefd), PIPE, lt_0);

  /* create the shell process */
  check((bash_pid = fork()), FORK, lt_0);

  if (bash_pid == 0) {      /* shell process */
    close(wr_pipefd[1]);    /* shell reads from this pipe    */
    close(rw_pipefd[0]);    /* shell writes to this pipe     */
    dup2(wr_pipefd[0], STDIN_FILENO);
    dup2(rw_pipefd[1], STDOUT_FILENO);
    dup2(rw_pipefd[1], STDERR_FILENO);
    close(wr_pipefd[0]);
    close(rw_pipefd[1]);
    check(execvp("/bin/bash", NULL), EXEC, lt_0);
  } 

  /* if successfully called, exec() doesn't return, so the code
   * below only runs in terminal thread */

  close(wr_pipefd[0]);    /* terminal writes to this pipe  */
  close(rw_pipefd[1]);    /* terminal reads from this pipe */
}

/* assume these signals are due to client termination */
void handle_signals(void) {
  check((signal(EPIPE, sighandler) == SIG_ERR), SIGNAL, is_SIG_ERR);
  check((signal(ECONNRESET, sighandler) == SIG_ERR), SIGNAL, is_SIG_ERR);
  check((signal(ETIMEDOUT, sighandler) == SIG_ERR), SIGNAL, is_SIG_ERR);
  check((signal(SIGPIPE, sighandler) == SIG_ERR), SIGNAL, is_SIG_ERR);
}

void close_pipes(void) {
  close(wr_pipefd[1]);    /* terminal writes to this pipe */
  close(rw_pipefd[0]);    /* terminal reads from this pipe */
}

void close_sockets(void) {
  if (newsockfd) close(newsockfd);  /* connected socket */
  if (sockfd) close(sockfd);        /* listening socket */
}

void sighandler(int signum) {
  if (is_server) close_pipes();
  initiate_shutdown((struct param_args*) args_ptr);
}

void initiate_shutdown(struct param_args* args) {
  int status;
  int exit_signal, exit_status;
  if (is_server) {
    check(waitpid(bash_pid, &status, 0), WAIT, lt_0);
    exit_signal = status & 0x007f;
    exit_status = (status & 0xff00) >> 8;
    fprintf(stderr,
	    "\r\nSHELL EXIT SIGNAL=%d STATUS=%d\r\n", 
	    exit_signal, exit_status);
  }
  
  if (log_fd != 0) close(log_fd);
  deinit_encryption();
  close_sockets();
  free(args->key);
  free(args->log_file);
  free(args->host_name);

  if (is_server) {
    if (WIFEXITED(status)) exit(0);
    else exit(1);
  }
  else {
    exit(0);
  }
}

void check(int code, FuncName name, comparator comp) {
  char *action; /* note: strdup not necessary because these
		   strings will never be modified */
  int error_number = errno;
  if (comp(code)) {
    switch (name) {
    case ATEXIT :
      action = "registering functions to be called at process termination";
      break;
    case PIPE :
      action = "creating pipes for interprocess communication";
      break;
    case FORK :
      action = "forking the terminal process";
      break;
    case EXEC :
      action = "creating the shell process";
      break;
    case KILL :
      action = "sending SIGINT to the shell";
      break;
    case WAIT :
      action = "waiting for the termination status of the shell process";
      break;
    case SETATTR :
      action = "setting the terminal attribute";
      break;
    case SOCK :
      action = "opening the listening socket";
      break;
    case BIND :
      action = "binding an address to socket";
      break;
    case ACCEPT :
      action = "accepting the connected socket";
      break;
    case CONNECT :
      action = "connecting to the host socket";
      break;
    case GETHOST :
      action = "trying to find the host";
      break;
    case SIGNAL :
      action = "settings up a signal handler";
      break;
    case OPEN :
      action = "opening a file";
      break;
    case KEYSIZE :
      action = "validating size of the encryption key";    
      break;
    case READ :
      action = "reading from a file or socket";
      break;
    case WRITE :
      action = "writing to a file or socket";
      break;
    case ENCRYPT :
      action = "encrypting some plain text";
      break;
    case DECRYPT :
      action = "decrypting some cipher text";
    }
    fprintf(stderr, "An error occurred while %s.\r\n%s\r\n", 
	    action, strerror(error_number));
    exit(1);
  }
}

void poll_inputs(int in1, int out1, int in2, int out2, 
		 struct param_args *args) {
  const int INITIAL_INPUT = 0;
  const int REDIRECT_INPUT = 1;
  const int NUM_INPUTS = 2;
  ssize_t num_chars;
  int i, n, digits = 0;
  char input;
  char buf[BUFSIZE];
  int block;

  char *log_file = args->log_file;
  char *key = args->key;
  bool has_log = (*log_file > '\0');
  bool has_key = (*key > '\0');

  bool exit_triggered = false;

  fds[INITIAL_INPUT].fd = in1;
  fds[INITIAL_INPUT].events = POLLIN;
  fds[REDIRECT_INPUT].fd = in2;
  fds[REDIRECT_INPUT].events = POLLIN;

  /* open log */
  if (has_log)
    check((log_fd = open(log_file, O_CREAT|O_WRONLY|O_APPEND, 777)), 
  	  OPEN, lt_0);

  memset(buf, 0, BUFSIZE);
  while (1) {
    /* let client finish one cycle before shutting down */
    if (!is_server && exit_triggered)
      initiate_shutdown(args);
    
    /* poll keyboard and shell */
    poll(fds, NUM_INPUTS, 0);
    for (n = INITIAL_INPUT; n < NUM_INPUTS;  n++) {
      /* pending input exists */
      if (fds[n].revents & POLLIN) {
        check((num_chars = read(fds[n].fd, buf, BUFSIZE-1)), READ, lt_0);

	/* log pre-decrypt message from server */
	if (has_log && n == REDIRECT_INPUT)
	  write_log(buf, num_chars, false);

	/* decrypt message between client-server */
	if (has_key && num_chars > 0) {
	  /* note: don't need strndup because buf is defined as
                   array instead of char pointer */
	  block = calculate_block(num_chars);
	  char *buf_copy = malloc(block*sizeof(char));
	  memset(buf_copy, 0, block*sizeof(char));
	  strncpy(buf_copy, buf, num_chars);
	  if ((is_server && n == INITIAL_INPUT) ||
	      (!is_server && n == REDIRECT_INPUT)) {
	    mcrypt_generic_init(decrypt_td, key, strlen(key), IV);
	    check(mdecrypt_generic(decrypt_td, buf_copy, block),
		  ENCRYPT, not_0);
	    mcrypt_generic_deinit(decrypt_td);
	    strncpy(buf, buf_copy, num_chars);
	  }
	  free(buf_copy);
	}

        /* output from terminal */
        for (i = 0; i < num_chars; i++) {
          input = buf[i];
	  if (input == '\r') {
	    /* \r\n received */
	    if (i < num_chars - 1 && buf[i+1] == '\n') i++;
	  }   
          if (input == '\r' || input == '\n') {
	    if (is_server) {
	      if (n == INITIAL_INPUT) write(out2, "\n", 1);
	      else write_to_socket(out1, "\n", key, 2, false);
	    }
	    else {
	      if (n == INITIAL_INPUT)
		write_to_socket(out2, "\n", key, 1, has_log);
	      write(out1, "\r\n", 2);
            }
          }
          else if (input == 3) { /* CTRL+C */
	    if (is_server) {
	      if (n == INITIAL_INPUT) {
	        if (!exit_triggered) {
	          exit_triggered = true;
	          check(kill(bash_pid, SIGINT), KILL, lt_0);
	          initiate_shutdown(args);
		}
	      }
	      else
		write_to_socket(out1, &input, key, 1, false);
	    }
	    else {
	      if (n == INITIAL_INPUT) {
		if (!exit_triggered) {
		  exit_triggered = true;
		  write_to_socket(out2, &input, key, 1, has_log);
		}
	      }
	      write(out1, "^C", 2);
	    }
	  }
	  else if (input == 4) { /* CTRL+D */
	    if (is_server) {
	      if (n == INITIAL_INPUT) {
		if (!exit_triggered) {
		  exit_triggered = true;
		  close_pipes();
		  initiate_shutdown(args);
		}
	      }
	      else
		write_to_socket(out1, &input, key, 1, false);
	    }
	    else {
	      if (n == INITIAL_INPUT) {
		if (!exit_triggered) {
		  exit_triggered = true;
		  write_to_socket(out2, &input, key, 1, has_log);
		}
	      }
	      write(out1, "^D", 2);
	    }
	  }
	  else if (input == 127) { /* DEL */
	    write(out1, &input, 1);
	  }
          else {
	    if (is_server) {
	      if (n == INITIAL_INPUT)
		write(out2, &input, 1);
	      else {
		write_to_socket(out1, &input, key, 1, false);
	      }
	    }
	    else {
	      write(out1, &input, 1);
	      if (n == INITIAL_INPUT)
		/* warning: this might encrypt the value of input */
		write_to_socket(out2, &input, key, 1, has_log);
	    }
          }
        }
      }
      else if (fds[n].revents & (POLLHUP | POLLERR)) {
        if (is_server) initiate_shutdown(args);
      }
    }
  }
  /* this should not be executed! */
  fprintf(stderr, "Unknown error!\r\n");
  exit(1);
}

void accept_client(struct param_args *args) {
  bool connected = false;
  check((sockfd = socket(AF_INET, SOCK_STREAM, 0)), SOCK, lt_0);
  struct sockaddr_in serv_addr;
  struct sockaddr_in cli_addr;
  bzero((char*) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(args->port_number);
  socklen_t serv_len = sizeof(serv_addr);
  socklen_t cli_len = sizeof(cli_addr);
  check(bind(sockfd, (struct sockaddr*) &serv_addr, serv_len), BIND, lt_0);
  while (!connected) {
    listen(sockfd, 5);
    newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &cli_len);
    if (newsockfd >= 0) connected = true;
  }
}

void connect_host(struct param_args *args) {
  struct sockaddr_in serv_addr;
  struct hostent *server;
  int serv_no;
  check((sockfd = socket(AF_INET, SOCK_STREAM, 0)), SOCK, lt_0);
  bzero((char*) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY; 
  server = gethostbyname(args->host_name);
  check((serv_no = (int)(long)server), GETHOST, is_NULL);
  serv_addr.sin_port = htons(args->port_number);
  socklen_t serv_len = sizeof(serv_addr);
  check(connect(sockfd, (struct sockaddr*) &serv_addr, 
		serv_len), CONNECT, lt_0);
}

void init_encryption(struct param_args *args) {
  char *key = args->key;
  encrypt_td = mcrypt_module_open("blowfish", NULL, "cfb", NULL);
  decrypt_td = mcrypt_module_open("blowfish", NULL, "cfb", NULL);
  IV = malloc(mcrypt_enc_get_iv_size(encrypt_td));
  memset(IV, 0, sizeof(char)* mcrypt_enc_get_iv_size(encrypt_td));
  check(strlen(key), KEYSIZE, gt_key_size);
  blocksize = mcrypt_enc_get_block_size(encrypt_td);
}

void deinit_encryption(void) {
  mcrypt_module_close(encrypt_td);
  mcrypt_module_close(decrypt_td);
}

int num_digits(int num) {
  int digits = 0;
  while (num > 0) {
    num /= 10;
    digits++;
  }
  return digits;
}

void write_log(void* buf, int bytes, bool is_sent) {
  /* deal with special characters */
  char *buf_copy = strndup(buf, bytes);
  if (buf_copy[0] == 3) {
    free(buf_copy);
    buf_copy = strdup("^C\0");
  }
  else if (buf_copy[0] == 4) {
    free(buf_copy);
    buf_copy = strdup("^D\0");
  }
  
  /* write header */
  int digits = num_digits(bytes);
  if (is_sent) write(log_fd, "SENT ", 5);
  else write(log_fd, "RECEIVED ", 9);
  char str_bytes[3];
  sprintf(str_bytes, "%d", bytes);
  write(log_fd, str_bytes, digits);
  write(log_fd, " bytes: ", 8);

  check(write(log_fd, buf_copy, bytes), WRITE, lt_0);
  write(log_fd, "\n", 1);
  free(buf_copy);
}

void write_to_socket(int fd, void *buf, void *key,
		     int count, bool has_log) {
  bool has_key = (strlen(key) > 0);

  /* calculate k*blocksize */
  int block = calculate_block(count);

  /* pad buffer to blocksize */
  char *buf_copy = (char*) malloc(block*sizeof(char));
  memset(buf_copy, 0, block*sizeof(char));
  strncpy(buf_copy, buf, count);
  if (has_key) {
    mcrypt_generic_init(encrypt_td, key, strlen(key), IV);
    check(mcrypt_generic(encrypt_td, buf_copy, block), DECRYPT, not_0);
    mcrypt_generic_deinit(encrypt_td);
  }
  if (has_log) {
    write_log(buf_copy, count, true);
  }
  check(write(fd, buf_copy, count), WRITE, lt_0);
  free(buf_copy);
}

int calculate_block(int size) {
  int result;
  if (size % blocksize == 0) result = size / blocksize;
  else result = (size / blocksize) + 1;
  result *= blocksize;
  return result;
}

bool lt_0(int rc) {
  return rc < 0;
}

bool not_0(int rc) {
  return rc != 0;
}

bool is_NULL(int rc) {
  return rc == (int)(long)NULL;
}

bool is_SIG_ERR(int rc) {
  return rc == (int)(long)SIG_ERR;
}

bool gt_key_size(int rc) {
  return rc >= mcrypt_enc_get_key_size(encrypt_td);
}
