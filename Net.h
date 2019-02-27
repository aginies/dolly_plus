#ifndef Net_h
#define Net_h 1
#include <stdio.h>
#include <sys/time.h>
#include "net.h"
#include "List.h"
#include "Packet.h"

//////////////////////////////
//         Base Class       //
//////////////////////////////
class Net {
 public:
  Net();
  virtual ~Net();
  char * get_hostname() { return hostName; }
  int    get_portno()   { return portNo;   }
  int    get_sock()     { return sockDesc; }
  static void set_verbose() { flag_v=1; } // this flag is global to all obj.
  static int is_verbose() { return flag_v; }
  void   set_timeout(int read, int write) {
            writeTimeout=write; readTimeout=read; }
  virtual void close_sock() = 0;
  enum ret_value {SUCCESS=0, ERROR=-1, FATAL=-2}; // return value
  enum sr_retval {SR_success=1,SR_connectionlost=0,SR_error=-1}; 
    //send recieve return value
 protected:
  char *hostName;
  int  portNo;
  int  sockDesc;
  bool  closedYes;
  int writeTimeout,readTimeout;
  static int flag_v;
};

#define FROM_ANY 0
class NetRecv : public Net{

 public:
  typedef void (*T_FUNC)(NetRecv*); 
  // just for ease to read. T_FUNC is a point to a function
  // which has 'a point to NetRecv Class' as its argument. 

  NetRecv(const int port);
  virtual ~NetRecv();
  virtual ret_value open_accept();
  virtual void open();
  virtual ret_value nonblock_accept(); //don't wait for a connection. 
  virtual ret_value accept();
  virtual void close_sock();
  virtual sr_retval recv_packet(Packet *pack);
  virtual sr_retval recv_packet(SPacket *pack);
  virtual sr_retval send_packet(Packet *pack);
  virtual sr_retval send_packet(SPacket *pack);
  int get_accept_sock() { return accptSock; }
  T_FUNC 
    register_exception(NetRecv *net,void function(NetRecv*));
  // in usual 'Net instance' wait for the sending/recieving 
  // completion in send_packet()/recv_packet().
  // But if you regist 'exception port', it will respond (call 'function')
  // the registered port access as well.
 private:
  int accptSock;
  NetRecv *exceptionNet;
  void   (*exceptionFunction)(NetRecv *);
};

class NetSend : public Net {
 public:
  NetSend(const char *host, const int port);
  virtual ~NetSend();
  ret_value open_connect();
  static void set_block_write()    { flag_nonblock_write = 0; }
  static void set_nonblock_write() { flag_nonblock_write = 1; } // default
  sr_retval send_packet(Packet *pack);
  sr_retval send_packet(SPacket *pack);
  sr_retval recv_packet(Packet *pack);
  sr_retval recv_packet(SPacket *pack);
  void close_sock();
  NetRecv::T_FUNC 
    register_exception(NetRecv *net,void function(NetRecv *));
 private:
  NetRecv *exceptionNet;
  void   (*exceptionFunction)(NetRecv *);
  static int flag_nonblock_write;
};

///////////////////////////////////////
///////////////////////////////////////
class CntlIn : public NetRecv {
 public:
  CntlIn() : NetRecv(CNTLPORT) {
    if(exist) { fprintf(stderr,"CntlIn doubly exist\n"); exit(3); }
    exist=true;
  }
  ~CntlIn() { exist=false;}
 private:
  static bool exist;
};

class CntlOut: public NetSend {
 public:
  CntlOut(const char *host) : NetSend(host,CNTLPORT) {
    if(exist) { fprintf(stderr,"CntlOut doubly exist\n"); exit(3); }
    exist=true;
  }
  ~CntlOut() { exist=false; }
 private:
  static bool exist;
};

class DataIn : public NetRecv {
 public:
  DataIn() : NetRecv(DATAPORT) {
    if(exist) { fprintf(stderr,"DataIn doubly exist\n"); exit(3); }
    exist=true;
  }
  ~DataIn() { exist=false; }
 private:
  static bool exist;
};

class DataOut: public NetSend {
 public:
  DataOut(const char *host) : NetSend(host,DATAPORT) {
    if(exist) { fprintf(stderr,"DataOut doubly exist\n"); exit(3); }
    exist=true;
  }
  ~DataOut() { exist=false;}
 private:
  static bool exist;
};

namespace NetSendRecv {
  enum ret_srecving { r_ERROR=-1, r_OK=0, r_TIMEOUT=1, r_UEOF=2, r_EPIPE=3};
  ret_srecving recving(const fd_set &rset, const int &maxnr,
      char * addr, const int &bytes, int timeout,
      const int &sock, 
      const int &ex_sock, NetRecv *& ex_net,
      void (*func)(NetRecv *) );
  ret_srecving sending(const fd_set &rset, const fd_set &wset,
       const int &maxnr,
       const char *addr, const int &bytes, int timeout,
       const int &sock, 
       const int &ex_sock, NetRecv *& ex_net, void func(NetRecv *) );
}

#endif


