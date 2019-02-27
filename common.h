//#define DEBUG

extern char *programName;
extern void mprintf();

#ifdef DEBUG
  #define DEBPR(x) {x};
#else
  #define DEBPR(x)  
#endif

