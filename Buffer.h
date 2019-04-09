#ifndef Buffer_h 
#define Buffer_h

#include <semaphore.h>

enum Bstatus {B_NORMAL=0, B_EOF=1, B_FINAL_EOF=2};

class Buffer {
 public:
  Buffer();
  virtual ~Buffer();
  char *get_addr() { return buffAddr; }
  int get_seqno() {return seqNo;}
  void clear_seqno() {seqNo=0;}
  void set_seqno(int no) {seqNo=no;}
  int set_fileno(int no) {fileNo=no; return 0; }
  int get_fileno() {return fileNo;}
  int set_eof(enum Bstatus eee) { eoF=eee; return 0; }
  enum Bstatus get_eof() {return eoF;}
  int get_size() {return buffBytes;} //return Buffer space size
  int get_contsize() {return contBytes;}
  int set_contsize(int size);
  void reinit() {eoF=B_NORMAL; seqNo=0;}
 protected:
 private:
  char * buffAddr;
  const long int buffBytes; // Buffer space size
  int contBytes;
  int seqNo;
  int fileNo;
  enum Bstatus eoF;
  //very private:
  char * buff_addr;
};

class ServerBuffer : public Buffer {
 public:
  ServerBuffer();
  virtual ~ServerBuffer();
  int wait_wnet_done() {return sem_wait(&semWriteNet);}
                     //automatically reset 'done' flag.
  int set_wnet_done()   {return sem_post(&semWriteNet);}
  int wait_rdisk_done() {return sem_wait(&semReadDisk);}
  int set_rdisk_done()  {return sem_post(&semReadDisk);}
  int wait_bindtwo_go() {return sem_wait(&semBindTwo);}
  int set_bindtwo_go()  {return sem_post(&semBindTwo);}

 private:
  sem_t semWriteNet;
  sem_t semReadDisk;
  sem_t semBindTwo; 
  /* very tricky. 'semBindTwo semaphore is used not to separate  *
   * a bank writed by readdisk() to a bank read by writenet()    *
   *                  ^^^^^^^^^                    ^^^^^^^^^^    *
   * more than 1 bank apart. It is needed to gurantee recover()  *
   * scheme to work.                                             */
};

class ClientBuffer : public Buffer {
 public:
  ClientBuffer();
  virtual ~ClientBuffer();
  int wait_wnet_done() {return sem_wait(&semWriteNet);}
  int set_wnet_done() {return sem_post(&semWriteNet);}
  int wait_rnet_done() {return sem_wait(&semReadNet);}
  int set_rnet_done() {return sem_post(&semReadNet);}
  int wait_wdisk_done() {return sem_wait(&semWriteDisk);}
  int set_wdisk_done() {return sem_post(&semWriteDisk);}
  int wait_bindtwo_go() {return sem_wait(&semBindTwo);}
  int set_bindtwo_go()  {return sem_post(&semBindTwo);}
 private:
  sem_t semWriteNet;
  sem_t semReadNet;
  sem_t semWriteDisk;
  sem_t semBindTwo; 
  /* very tricky. 'semBindTwo semaphore is used not to separate *
   * a bank writed by readnet() to a bank read by writenet()    *
   *                  ^^^^^^^^                    ^^^^^^^^^^    *
   * more than 1 bank apart. It is needed to gurantee recover() *
   * scheme to work.                                            */
};

#endif


