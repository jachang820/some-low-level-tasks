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
struct pollfd fds[1];
struct PreciseTimer period_timer, input_timer;
int interrupt = 0;
int generate_reports = 1;
int log_fd, format_fd;
struct termios orig_attr, new_attr;

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

/* stubs */
/*
float pd;
float tmptr;
void set_period(float period) { pd = period; }
void set_scale(char scale) { tmptr = (scale=='F') ? 85.6 : 30.3; }
float get_period(void) { return pd; }
float get_temperature(void) { return tmptr; }
void init_temp_sensor(void) {}
void init_button(void) {}
void read_temp_sensor(void) {}
int read_button(void) { return 0; }
void deinit_temp_sensor(void) {}
void deinit_button(void) {}
*/

int main(int argc, char* argv[]) {
  /* init */
  process_args(argc, argv);
  init_temp_sensor();
  init_button();
  init_vars();
  noncanonical_mode();
  atexit(deinit_everything);
  
  /* sample */
  /* NOTE:
   * Formatting is written to new fd to prevent polluting
   * output redirection to a file.
   * Input is written to new fd to show it while typing
   * when output is redirected to a file.
   */
  format_fd = dup(STDERR_FILENO);
  write_report();
  while (1) {
    poll(fds, 1, 0);
    if (fds[0].revents & POLLIN) {
      num_chars = read(fds[0].fd, &new_buf, 128);
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

  exit(2);
}

void process_args(int argc, char* argv[]) {
  int opt, longindex;
  char correct_usage[206] =
    "Correct usage:\r\n"
    "/lab4b --period=# --scale=F|C --log=filename\r\n"
    "--period     : sampling interval in seconds\r\n"
    "--scale      : temperature in Fahrenheit or Celsius\r\n"
    "--log        : name of file to log report\r\n\0";

  /* default values */
  set_period(1.000);
  set_scale('F');
  log_fd = -1;

  while(1) {
    longindex = 0;
    static struct option longopt[] = {
      {"period"    , required_argument, 0, 'p' },
      {"scale"     , required_argument, 0, 's' },
      {"log"       , required_argument, 0, 'l' },
      {0           , 0                , 0,  0  }
    };
    opt = getopt_long(argc, argv, "", longopt, &longindex);

    if (opt == -1) break;

    switch(opt) {
    case 'p':
      set_period(atof(optarg));
      break;
    case 's':
      set_scale(*optarg);
      break;
    case 'l':
      log_fd = open(optarg,
		    O_CREAT | O_WRONLY | O_APPEND | O_NONBLOCK, 666);
      break;
    default:
      fprintf(stderr, "%s", correct_usage);
      exit(1);
    }
  }
  if (optind < argc) {
    fprintf(stderr, "Invalid arguments: ");
    while (optind < argc)
      fprintf(stderr, "%s ", argv[optind++]);
    fprintf(stderr, ". %s", correct_usage);
    exit(1);
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
  char time_str[8];
  current_time(time_str);
  read_temp_sensor();
  float temperature = get_temperature();
  char temp_str[8];
  int temp_length;
  temp_length = sprintf(temp_str, " %2.1f", temperature);

  if (interrupt || generate_reports) {
    write(STDOUT_FILENO, time_str, 8);
    if (log_fd != -1) write(log_fd, time_str, 8);

    if (interrupt) {
      write(STDOUT_FILENO, " SHUTDOWN", 9);
      if (log_fd != -1) write(log_fd, " SHUTDOWN", 9);
    }
    else if (generate_reports) {
      write(STDOUT_FILENO, temp_str, temp_length);
      if (log_fd != -1) write(log_fd, temp_str, temp_length);
    }
  
    write(format_fd, "\r", 1);
    write(STDOUT_FILENO, "\n", 1);
    if (log_fd != -1) write(log_fd, "\n", 1);
  }
  
  if (interrupt) exit(0);
}

void init_vars(void) {
  memset(new_buf, 0, BUFSIZE);
  memset(line_buf, 0, BUFSIZE);
  memset(proc_buf, 0, BUFSIZE);
  fds[0].fd = STDIN_FILENO;
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
  int valid_command = 1;
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
  else {
    valid_command = 0;
  }
  if (valid_command && log_fd != -1) {
    write(log_fd, proc_buf, line_marker);
    write(log_fd, "\n", 1);
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
