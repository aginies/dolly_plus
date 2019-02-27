#ifndef StopWatch_h
#define StopWatch_h
#include <sys/time.h>
class StopWatch {
 public:
  StopWatch();
  virtual ~StopWatch() {}
  virtual void start();   // start also means restart
  virtual double lap();
  virtual double stop();
  double get_sum() { return (double)(timeSum+lapTime); }
  void clear_sum() { timeSum = 0.0L; }
 private:
  struct timeval startTime;
  long double lapTime;
  long double timeSum;
};

const char * getTime();
#endif
