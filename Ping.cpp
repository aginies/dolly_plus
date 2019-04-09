#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "common.h"
#include "Config.h"
#include "net.h"
#include "Net.h"
#include "Packet.h"
#include "List.h"

#define MAXRETRY 3

char * programName;
bool flag_v    = false;    /* verbose command switch */
bool flag_d    = false;    /* dummy sending mode */

//FILE *ty_err = NULL  /* error printout */

Config *config=0;
List *hosts=0;
ListIte  *hostIte=0;
FileList *files=0;
CntlIn  *cntlin=0;
CntlOut *cntlout=0;
DataIn  *datain=0;
DataOut *dataout=0;
void data_transfer();

//*******************
// Parse Config File
//******************
int configure(char * configfile)
{
  config = new Config();
  hosts = new List();
  files = new FileList();
  config->open(configfile);
  config->parse(hosts,files); 
  if (flag_v) {
    fprintf(stderr,"HOSTs\n");
    hosts->print();
    fprintf(stderr,"FILEs\n");
    files->print();
  }
  return 0;
}


//////////////
// usage()  //
//////////////     
void usage(char * progname)
{
  fprintf(stderr," %s  [-v] [-f <dollytab> | -h <hostname> ] \n"
          ,progname);
}

//***************
// MAIN routine *
//***************
int main(int argc, char** argv)
{
  char * pingfile;
  char * hostname;
  int c;
  bool is_pingfile=false,is_host=false;
  CntlOut *cntlout=0;
  int ret,retry;
  CmdPacket reqpack,ackpack;
  
  programName=argv[0];

  /* Parse arguments */
  while (1) {
    c = getopt(argc, argv, "f:h:v");
    if (c==(-1)) break;
    switch (c) {
    case 'f':
      /* Only the server should need this in the final version. */
      if (strlen(optarg) > 255) {
        fprintf(stderr, "Name of ping-file too long.\n");
        exit(1);
      }
      pingfile = optarg;
      is_pingfile=true;
      break;

    case 'h':
      /* Hostname */
      if (strlen(optarg) > 255) {
        fprintf(stderr, "Name of hostname too long.\n");
        exit(1);
      }
      hostname = optarg;
      is_host=true;
      break;
      
    case 'v':
      /* Verbose */
      flag_v = true;
      Net::set_verbose();
      break;
      
    default:
      fprintf(stderr, "Unknown option '%c'.\n", c);
      usage(programName);
      exit(1);
    }
  }
  
  if ( is_host) {
    if (flag_v) {
      fprintf(stderr, "Ping to host='%s'...  \n",hostname);  }    
  } else {
    usage(programName);
    exit(1);
  }

  if (is_pingfile) /*file ping mode*/ {
    FileList *filelist= new FileList();
    FilePacket *filepacket = new FilePacket();
    NetSend *repout = new NetSend(hostname,REPORT);
    Buffer *buff = new Buffer();
    DataPacket *pack = new DataPacket();
    const char * address;
    char *dummy;
    int len,ii;

    pack->attach(buff);    
    filelist->init(1);
    len = strlen(pingfile);
    dummy = new char[len+1];
    filelist->push(dummy,5,pingfile, len, (unsigned char)0);
    filelist->packetfill(filepacket);
    if (repout->open_connect()!=Net::SUCCESS) {
      fprintf(stderr,"connect to %s (port=%d) fail.\n",hostname,REPORT);
      exit(2);
    }
    ret=repout->send_packet(filepacket);
    if (ret<=0) {
      fprintf(stderr,"file packet send failure. \n");
      exit(2);
    }
    ret=repout->recv_packet(pack);
    if (ret<=0) {
      fprintf(stderr,"file contents recieve failure. \n");
      exit(2);
    }
    len = pack->get_contsize();
    address = pack->get_contaddr();
    printf("lenlen=%d address=%d \n",len,address);    
    for(ii=0; ii<len; ii++) {
      //printf("address=%d (",address+ii);
      putchar(*(address+ii));
    }

  } else /*status ping mode*/ {
    cntlout = new CntlOut(hostname);
    if ( cntlout->open_connect() != CntlOut::SUCCESS) {
      fprintf(stderr,"Fail to open connection to %s \n",hostname);
      exit(2);
    }
    reqpack.make_command(TYPE_REQBANKST,3);
    ret=cntlout->send_packet(&reqpack);
    if (ret<=0) {
      fprintf(stderr,"cannot send RequestBankStatus packet to %s \n",
	      hostname);
      exit(2);
    }
    
    ret=cntlout->recv_packet(&ackpack);
    if (ret<=0) {
      fprintf(stderr,"cannot recieve Ack for ReqBankStatus packet from %s \n",
	      hostname);
      exit(2);
    }
    ret = ackpack.get_type();
    if (ret != TYPE_ACKBANKST) {
      fprintf(stderr,"got type=%d packet from %s \n",hostname);
      exit(2);
    }
    printf("Message>%s\n",ackpack.get_message());
    
    cntlout->close_sock();
    delete cntlout;
    exit(0);
  }
}







