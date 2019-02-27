#ifndef Disk_h
#define Disk_h
#include "Buffer.h"

#define NOT_EOF    1
#define EOF_DETECT 2

enum FileStat {S_FREE,S_BEGIN,S_MIDDLE,S_EOF};
class Disk {
 public:
  Disk() {aFD=(-1);fStat=S_FREE;}
  virtual ~Disk() {}
  void close() { ::close(aFD); }
 protected:
  int aFD;
  enum FileStat fStat; // for the moment, it is only 
                       //used in FromDisk@server side
};

class ToDisk : public Disk {
 public:
  ToDisk() { Disk(); }
  ~ToDisk() {}
  int open(const char *path, const int flag);
  int write_frombuff(Buffer *buff);
 private: 
  int pipes_forks(const int fd, const int flag, const int level);
};

class FromDisk : public Disk {
 public:
  FromDisk() { Disk(); }
  ~FromDisk() {}
  int open(const char *filename);
  int read_tobuff(Buffer *buff);
 private:
  int seqNo;   // Count file chunk seq no.
};
#endif


