/*
 * NAME: Jonathan Chang
 * EMAIL: j.a.chang820@gmail.com
 * ID: 104853981
 */ 

struct PreciseTimer {
  struct timespec start_time;
  struct timespec end_time;
  long long diff;
};

void PreciseTimer_start(struct PreciseTimer *timer);
void PreciseTimer_end(struct PreciseTimer *timer);
