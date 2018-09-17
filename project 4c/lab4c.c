/*
 * NAME: Jonathan Chang
 * EMAIL: j.a.chang820@gmail.com
 * ID: 104853981
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include "PreciseTimer.h"
#include "EdisonSensors.h"


/* constants */
const char CLEAR_CONSOLE_LINE[5] = "\33[2K\r";
const int BUFSIZE = 128;
const float RESPONSIVENESS = 0.75; /* decrease to increase 
				      responsiveness */
/* console */
char new_buf[128];
char line_buf[128];
char proc_buf[128];
int line_marker = 0;
int num_chars;
struct PreciseTimer period_timer, input_timer;
int interrupt = 0;
int generate_reports = 1;
struct termios orig_attr, new_attr;

/* i/o */
struct pollfd fds[1];
int log_fd, format_fd, sock_fd;

/* ssl/tls */
int is_secure;
SSL_CTX *ctx;
SSL *ssl;


void process_args(int, char**);
void current_time(char*);
void report(void);
void init_vars(void);
void noncanonical_mode(void);
void restore_console(void);
void deinit_everything(void);
void store_buffer(void);
void process_command(void);
void write_report(void);
void write_input(void);
int connect_host(char*, int);
int read_from_host(char*);
void write_to_host(char*, int);
SSL_CTX* init_context(void);


int main(int argc, char* argv[]) {
  /* detect whether filename is lab4c_tcp or lab4c_tls */
  size_t proc_length = strlen(argv[0]);
  is_secure = (argv[0][proc_length - 1] == 's') ? 1 : 0; 
  
  /* init */
  process_args(argc, argv);
  init_temp_sensor();
  init_button();
  init_vars();
  noncanonical_mode();
  atexit(deinit_everything);
  
  /* sense/report */
  format_fd = dup(STDERR_FILENO);
  write_report();
  while (1) {
    poll(fds, 1, 0);
    if (fds[0].revents & POLLIN) {
      num_chars = read_from_host(new_buf);
      write(STDOUT_FILENO, new_buf, num_chars);
      if (new_buf[0] == 0) num_chars = 0;
    }
    if (num_chars > 0) {
      store_buffer();
      write_input();
      num_chars = 0;
    }
    if (read_button() == 1) {
      interrupt = 1;
    }
    if (PreciseTimer_elapsedFloat(&input_timer) >= RESPONSIVENESS) {
      write_input();
      PreciseTimer_start(&input_timer);
    }
    if (PreciseTimer_elapsedFloat(&period_timer) >= get_period()) {
      write_report();
      write_input();
      PreciseTimer_start(&period_timer);
    }
  }

  /* should not be reached */
  exit(2);
}

void process_args(int argc, char* argv[]) {
  int opt, longindex;
  char *host_name, *student_id;
  int port_number;
  char correct_usage[284] =
    "Correct usage:\r\n"
    "/lab4b port_number --period=# --scale=F|C --log=filename\r\n"
    "--period     : sampling interval in seconds\r\n"
    "--scale      : temperature in Fahrenheit or Celsius\r\n"
    "--log        : name of file to log report\r\n"
    "--id         : id of student\r\n"
    "--host       : host name of server\r\n\0";

  /* default values */
  set_period(1.000);
  set_scale('F');
  log_fd = -1;
  host_name = NULL;
  port_number = 0;
  student_id = NULL;

  while(1) {
    longindex = 0;
    static struct option longopt[] = {
      {"log"       , required_argument, 0, 'l' },
      {"id"        , required_argument, 0, 'i' },
      {"host"      , required_argument, 0, 'h' },
      {0           , 0                , 0,  0  }
    };
    opt = getopt_long(argc, argv, "", longopt, &longindex);

    if (opt == -1) break;

    switch(opt) {
    case 'l':
      log_fd = open(optarg,
		    O_CREAT | O_WRONLY | O_APPEND | O_NONBLOCK, 666);
      break;
    case 'i':
      student_id = strdup(optarg);
      break;
    case 'h':
      host_name = strdup(optarg);
      break;
    default:
      fprintf(stderr, "%s", correct_usage);
      exit(1);
    }
  }
  if (optind < argc - 1) {
    fprintf(stderr, "Invalid arguments: ");
    while (optind < argc)
      fprintf(stderr, "%s ", argv[optind++]);
    fprintf(stderr, ". %s", correct_usage);
    exit(1);
  }
  else {
    port_number = atoi(argv[optind]);
  }

  if (strlen(student_id) != 9) {
    fprintf(stderr, "ID must be 9 digits long.\r\n");
    exit(1);
  }
  
  if (port_number == 0 || host_name == NULL || student_id == NULL) {
    fprintf(stderr, "%s", correct_usage);
    exit(1);
  }
  else {
    sock_fd = connect_host(host_name, port_number);
    if (is_secure == 1) {
      ctx = init_context();
    }
    char write_id[13];
    sprintf(write_id, "ID=%s\n", student_id);
    write_to_host(write_id, 13);
    free(student_id);
    free(host_name);
  }
}

void current_time(char* time_str) {
  const int INT_TO_ASCII = 48;
  time_t now = time(NULL);
  struct tm time;
  localtime_r(&now, &time);
  sprintf(time_str, "%02d:%02d:%02d",
	  time.tm_hour,
	  time.tm_min,
	  time.tm_sec);
}

void report(void) {
  char time_str[9];
  current_time(time_str);
  time_str[8] = '\0';
  read_temp_sensor();
  float temperature = get_temperature();
  char temp_str[8];
  int temp_length;
  temp_length = sprintf(temp_str, "%2.1f", temperature);
  temp_str[temp_length] = '\0';
  char write_str[20];
  int write_length;

  if (interrupt || generate_reports) {
    if (interrupt) {
      write_length = sprintf(write_str, "%s SHUTDOWN", time_str);
    }
    else if (generate_reports) {
      write_length = sprintf(write_str, "%s %s", time_str, temp_str);
    } 
    write(STDOUT_FILENO, write_str, write_length);
    write(format_fd, "\r", 1);
    write(STDOUT_FILENO, "\n", 1);
    write_str[write_length] = '\n';
    write_str[write_length + 1] = '\0';
    write_to_host(write_str, write_length + 1);
  }
  
  if (interrupt) exit(0);
}

void init_vars(void) {
  memset(new_buf, 0, BUFSIZE);
  memset(line_buf, 0, BUFSIZE);
  memset(proc_buf, 0, BUFSIZE);
  fds[0].fd = sock_fd;
  fds[0].events = POLLIN;
  PreciseTimer_start(&period_timer);
  PreciseTimer_start(&input_timer);
}

void noncanonical_mode(void) {
  tcgetattr(STDIN_FILENO, &orig_attr);
  new_attr = orig_attr;
  new_attr.c_oflag = 0;
  new_attr.c_lflag &= ~(ECHO | ICANON | ISIG);
  tcsetattr(STDIN_FILENO, TCSANOW, &new_attr);
}

void restore_console(void) {
  tcflush(STDIN_FILENO, TCIFLUSH);
  tcsetattr(STDIN_FILENO, TCSANOW, &orig_attr);
}

void deinit_everything(void) {
  restore_console();
  deinit_button();
  deinit_temp_sensor();
  if (log_fd != -1) close(log_fd);
  close(format_fd);
  close(sock_fd);
  if (is_secure == 1) {
    SSL_free(ssl);
    SSL_CTX_free(ctx);
  }
}

void store_buffer(void) {
  int i;
  if (line_marker > BUFSIZE - 1) {
    fprintf(stderr, "Input line is too long.\r\n");
    memset(line_buf, 0, BUFSIZE);
    line_marker = 0;
  }
  for (i = 0; i < num_chars; i++) {
    if (new_buf[i] == 127) { /* DEL */
      if (line_marker > 0) {
	line_marker--;
	line_buf[line_marker] = 0;
      }
    }
    else if (new_buf[i] == 10) { /* NL */
      if (line_marker > 0) {
	/* don't echo newline */
	write(format_fd, "\r", 1);
	process_command();
	memset(line_buf, 0, line_marker);
	line_marker = 0;
      }
    }
    else if (new_buf[i] == 3) { /* CTRL+C */
      write(format_fd, "\r", 1);
      exit(2);
    }
    else if (new_buf[i] >= 32 && new_buf[i] <= 126) {
      /* visible chars */
      line_buf[line_marker] = new_buf[i];
      line_marker++;
    }
  }
  memset(new_buf, 0, num_chars);
}

void process_command(void) {
  int i;
  char period[10];
  memset(period, 0, 10);
  strncpy(proc_buf, line_buf, line_marker);
  if (strncmp(proc_buf, "SCALE=", 6) == 0) {
    set_scale(proc_buf[6]);
  }
  else if (strncmp(proc_buf, "PERIOD=", 7) == 0) {
    for (i = 7; i < line_marker; i++) {
      period[i - 7] = proc_buf[i];
    }
    set_period(atof(period));
  }
  else if (strncmp(proc_buf, "STOP", 4) == 0) {
    generate_reports = 0;
  }
  else if (strncmp(proc_buf, "START", 5) == 0) {
    generate_reports = 1;
  }
  else if (strncmp(proc_buf, "OFF", 3) == 0) {
    interrupt = 1;
  }
 
  if (interrupt) {
    report();
  }
  memset(proc_buf, 0, line_marker);
}

void write_input(void) {
  write(format_fd, CLEAR_CONSOLE_LINE, 5);
  write(format_fd, &line_buf, line_marker);
}

void write_report(void) {
  write(format_fd, CLEAR_CONSOLE_LINE, 5);
  report();
}

int connect_host(char *host_name, int port_number) {
  struct sockaddr_in serv_addr;
  struct hostent *server;
  int serv_no;
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    fprintf(stderr, strerror(errno));
    fprintf(stderr, ". Unable to obtain socket descriptor.\r\n");
    exit(2);
  }
  memset((char*) &serv_addr, 0, sizeof(serv_addr));
  server = gethostbyname(host_name);
  if (server == NULL) {
    fprintf(stderr, strerror(errno));
    fprintf(stderr, ". Host does not exist.\r\n");
    exit(1);
  }
  serv_addr.sin_family = AF_INET;
  memcpy((char*)&serv_addr.sin_addr.s_addr,
	 (char*)server->h_addr, server->h_length);
  serv_no = (int)(long)server;
  serv_addr.sin_port = htons(port_number);
  socklen_t serv_len = sizeof(serv_addr);
  if (connect(sockfd, (struct sockaddr*) &serv_addr, serv_len) == -1) {
    fprintf(stderr, strerror(errno));
    fprintf(stderr, ". Failed to connect to the host.\r\n");
    exit(2);
  }

  return sockfd;
}

int read_from_host(char *buf) {
  int size = (is_secure == 1) ?
    SSL_read(ssl, buf, BUFSIZE) :
    read(fds[0].fd, buf, BUFSIZE);
  if (log_fd != -1) {
    write(log_fd, buf, size);
  }
  return size;
}

void write_to_host(char *buf, int size) {
  if (log_fd != -1) {
    write(log_fd, buf, size);
  }
  int result;
  result = (is_secure == 1) ?
    SSL_write(ssl, buf, size) :
    write(sock_fd, buf, size);

  if (result < 0) {
    fprintf(stderr, "Unable to write to host.\r\n");
    exit(2);
  }
}

SSL_CTX* init_context(void) {
  const SSL_METHOD *method;
  SSL_CTX *ctx;
  BIO *web = NULL, *out = NULL;
  
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  SSL_library_init();
  
  method = SSLv23_client_method();
  ctx = SSL_CTX_new(method);
  if (ctx == NULL) {
    ERR_print_errors_fp(stderr);
    exit(2);
  }

  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
  SSL_CTX_set_verify_depth(ctx, 4);
  const long flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;
  SSL_CTX_set_options(ctx, flags);
  int res = SSL_CTX_load_verify_locations(ctx, "lab4c_server.crt", NULL);
   
  ssl = SSL_new(ctx);
  SSL_set_fd(ssl, sock_fd);
  if (SSL_connect(ssl) == -1) {
    ERR_print_errors_fp(stderr);
    exit(2);
  }

  X509* cert = SSL_get_peer_certificate(ssl);
  X509_free(cert);

  return ctx;
}
