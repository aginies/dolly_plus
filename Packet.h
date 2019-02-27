#ifndef Packet_h 
#define Packet_h 1

#include <stdlib.h>
#include "packet.h"
#include "Buffer.h"

#define MAX_PACKET_ENTRY_LEN 128  //max no. of char. of an entry in the packet
#define MAX_PACKET_ENTRY_NO 2048  //max no. of entrys in the packet

class Packet {
 public:
  Packet();
  virtual ~Packet();
  int get_size() { return packetBytes; }
  int get_contsize() { return contBytes; }
  int get_headersize() { return headerBytes; }
  int get_trailersize() {return trailBytes; }
  const char * get_addr() { return packetBuff; }
  char * get_haddr() {return headerP;}
  const char * get_contaddr() { return contP; }
  virtual char *make(short int bytes, short int num) = 0;  
                // make memory space for a packet to send
  virtual char *checkheader_make() = 0;
                // make memory space for a packet to recieve
  virtual int  checktrailer()=0;
  void print();
 protected:
  char * core_make(short int bytes, short int num);  
  void buff_free() { free(packetBuff); }
  char * packetBuff;    /* Buffer address created by malloc() */
  char * contP;		/* Hasei?: contentsPointer */
  char * endP;		/* Contents End pointer (top of trailer) */
  char * headerP;	/* Header Buffer for recieving only */
  int packetBytes;
  int contBytes;
  int numEntry;		/* optional */
  int headerBytes;
  int trailBytes;

  friend class PacketIte;
};
//flag value   (caution!!: in ascent_entry()(not yet implemented,
//               I assume flag is not 0.)
#define AVAIL_HOST  1   /* available host */
#define CALL_HOST   2   /* the next destination host(called host) */
                        /* Purpose: to know who am I in the host ring. */
                        /* hostname() cannot be used in the case where */
			/* node has more than 2 NIC */
#define NOAVIL_HOST 3   /* the host find to be not available */

//RingPacket: [path=data]
//     MAGIC_RING_OPEN="ROPN"(4),no_of_bytes(2),no_of_hosts(2),
//     1(1),server_host_name(Variable. terminated by '\0'),
//     available_flag, client_host_name(V),
//               :
//     MAGIC_RING_CLOSE="RCLS(4)

class RingPacket : public Packet {
 public:
  RingPacket();
  virtual ~RingPacket() {}
  char *make(short int cont_bytes, short int num_entry);
  char *checkheader_make();
  int checktrailer();
 private:
  char aHeader[HEADER_MAXLEN]; //buffer only for recieving 
};

//HostPacket: [path=data]
//     MAGIC_HOST_OPEN="HOPN"(4),no_of_bytes(2),no_of_hosts(2),
//     1(1),server_host_name(Variable. terminated by '\0'),
//     available_flag, client_host_name(V),
//               :
//     MAGIC_HOST_CLOSE="HCLS(4)

class HostPacket : public Packet {
 public:
  HostPacket();
  virtual ~HostPacket() {}
  char *make(short int cont_bytes, short int num_entry);
  char *checkheader_make();
  int checktrailer();
 private:
  char aHeader[HEADER_MAXLEN]; //buffer used only in recieving
};

//flag value
#define NORMAL     0 
#define COMPRESSED 1

//FilePacket:  [path=data]
//     MAGIC_FILE_BEGN="FBEG"(4),no_of_bytes(2),no_of_files(2),
//     compress_flag, file_name(V),
//               :
//     MAGIC_RING_CLOSE="FFEND"(4)

class FilePacket : public Packet {
 public:
  FilePacket();
  virtual ~FilePacket() {}
  char *make(short int cont_bytes, short int num_entry);
  char *checkheader_make();
  int checktrailer();
 private:
  char aHeader[HEADER_MAXLEN]; //buffer used only in recieving
};

//FileNoPacket:  [path=data]
//     MAGIC_FILENO="FLNO"(4),no_of_bytes=0(2),file_No.(2)
//

class FileNoPacket : public Packet {
 public:
  FileNoPacket();
  virtual ~FileNoPacket() {}
  char *make(short int dummy1, short int fileno);
  char *checkheader_make();
  int checktrailer() {}
  int set_fileno(short int fileno) ;
  short int get_fileno() { return fileNo; }
 private:
  short int fileNo;
  char aHeader[HEADER_MAXLEN]; //used only in recieving
};
  
//CmdPacket:   [path=control]
//    MAGIC_COMMAND="CMND",no_of_bytes=4(2),type(2),operand(V)
//

#define MAX_MESSAGE_SIZE 128

class CmdPacket : public Packet {
 public:
  CmdPacket();
  virtual ~CmdPacket() {}
  char *make_command(int type, long int operand);
  char *make_command(int type, char *message, int size);
  char *checkheader_make();
  int checktrailer() {}
  int get_type() { return pType; }
  long int get_operand();
  char *get_message();
  int get_message_size() { return contBytes; }
 private:
  char *make(short int bytes, short int type);
  char aHeader[HEADER_MAXLEN]; //used only in recieving
  int& pType;
};
  
/////////////////
// Iterator    //
/////////////////
#define FLAG_LEN 1
class PacketIte {
 public:
  PacketIte(Packet *pack);
  virtual ~PacketIte() {};
  int pop_entry();
  int search_flag(unsigned char fl);
  int search_name(char *name);
  unsigned char get_flag() {return *(unsigned char *)packP;  }
  void set_flag(unsigned char flag) { *(unsigned char*)packP=flag; }
  char * get_name();
  int get_name_len() { name_len_check()-1; } // length without '\0'
  int get_noentry() { return packObj->numEntry; }
  void all_print() { packObj->print(); }
 private:
  int name_len_check(); // length with '\0'
  char * packP;
  Packet *packObj;
};


////////////////////////////////////////////////////////
// spacket (data buffer and header are separated)     //
////////////////////////////////////////////////////////
class SPacket: public Packet {
 public:
  SPacket() {}
  virtual ~SPacket() {}
  int attach(Buffer *buff);
  virtual char *checkheader_make()=0;
  virtual char *make(short int contbytes, short int num_entry)=0;
  virtual int  checktrailer() =0;
 protected:
  // Useless valiables in Packet
  //  char * packetBytes;
  //  int numEntry;
  Buffer *pBuff;
  char aHeader[HEADER_MAXLEN];
};


class DataPacket: public SPacket {
 public:
  DataPacket();
  ~DataPacket() {}
  char * checkheader_make();
  char * make(short int dummy1, short int dummy2);
  int  checktrailer() {}
};

#endif
