#include <stdio.h>
#include <stdlib.h>
#include "StopWatch.h"

StopWatch::StopWatch() 
  : timeSum(0.0)
{
  startTime.tv_sec = 0; startTime.tv_usec = 0;
}

void StopWatch::start()
{
  if (gettimeofday(&startTime, NULL)==(-1)) {
    perror("StopWatch::start()");
    exit(2);
  }
}

double StopWatch::stop()
{
  struct timeval stop_time;
  long double split_time;

  if (gettimeofday(&stop_time, NULL)==(-1)) {
    perror("StopWatch::stop()");
    exit(2);
  }
  split_time = (stop_time.tv_sec*1000 + stop_time.tv_usec/1000)
              -(startTime.tv_sec*1000 + startTime.tv_usec/1000);
  split_time = split_time/1000.0;
  timeSum += split_time;
  lapTime  = 0.0L;
  
  return((double)split_time);
}

double StopWatch::lap()
{
  struct timeval stop_time;
  long double split_time;

  if (gettimeofday(&stop_time, NULL)==(-1)) {
    perror("StopWatch::stop()");
    exit(2);
  }
  split_time = (stop_time.tv_sec*1000 + stop_time.tv_usec/1000)
              -(startTime.tv_sec*1000 + startTime.tv_usec/1000);
  split_time = split_time/1000.0;

  lapTime = split_time;
  
  return((double)split_time);
}


#include <time.h>
const char * getTime() {
  char * timep;
  time_t now;

  now = time(NULL);
  timep = ctime(&now);
  *(timep+19) = 0;
  return (timep+11);
}


