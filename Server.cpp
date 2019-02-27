#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "common.h"
#include "Config.h"
#include "List.h"
#include "net.h"
#include "Net.h"
#include "Packet.h"

#define MAXRETRY 3

char * programName;
bool flag_v    = false;    // verbose command switch 
bool flag_d    = false;    // dummy sending mode 
bool flag_bind  = true;    // readdisk(), writenet() bind mode
bool last_stop = false;    // a flag show that the last node is finished.

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

//*********************
// Building A host ring
//*********************
int buildring()
{
  int retry;
  Net::sr_retval srret;
  
  hostIte =new ListIte(hosts);
  hostIte->pop_entry(); // first entry was servername (it's me!)
  datain  =new DataIn();
  // connect cannot wait for long time. So server have to issue
  // connect command last
  dataout  =new DataOut(hostIte->get_name());
 
  //***  1st Step:  Sending a Ring host list Packet

  RingPacket *ring_packet;

  if (flag_v) printf("Start sending RING Packet \n");

  ring_packet = new RingPacket();  
  hosts->packetfill(ring_packet);
  // CALL_HOST flag set to the first candidate of host is done
  // in Config.cpp
  
  // connection retry loop; upto MAXRETRY hosts are tried.
  for (retry=0;retry<MAXRETRY;retry++) {

    if (dataout->open_connect()!=Net::SUCCESS) {
      hostIte->set_flag(NOAVIL_HOST);
      dataout->close_sock();
      delete dataout;

      if (!hostIte->pop_entry()) {
	fprintf(stderr,"host list is reached to the end(1)\n");
	hosts->print();
	exit(1);
      }
      hostIte->set_flag(CALL_HOST); 
      delete ring_packet;
      ring_packet = new RingPacket();
      hosts->packetfill(ring_packet);
      dataout=new DataOut(hostIte->get_name());
      continue;
    }
    
    srret=dataout->send_packet(ring_packet);
    if (srret==Net::SR_success)      /* success */   {
      break;           //            <------------ retry break!
    } else if (srret==Net::SR_connectionlost) /*send timeout*/{
      hostIte->set_flag(NOAVIL_HOST);
      delete dataout;
      if (!hostIte->pop_entry()) {
	fprintf(stderr,"host list is reached to the end(2)\n");
	hosts->print();	exit(1);      
      } else {
	hostIte->set_flag(CALL_HOST);
	delete ring_packet;
	ring_packet = new RingPacket();
	hosts->packetfill(ring_packet);
	dataout = new DataOut(hostIte->get_name());
	continue;      //            <------------ retry !!
      }
    } else              /*send error*/ {
      fprintf(stderr,"Server.cpp::building(1) \n");
      exit(1);
    }
  }
  if (retry==MAXRETRY) {
    fprintf(stderr,"send retry exceed a limit(<%d times)\n",retry);
    exit(1);  }
  delete ring_packet;

  if (flag_v) {
    fprintf(stderr,"Sending Ring packet succeeded. try to recieve the "
	    "return.\n");
  }
 

  //*** 2nd Step: Recieving the Ring packet

  if (flag_v) printf("Start recieving RING Packet back \n");

  if (datain->open_accept()!=Net::SUCCESS) {
    fprintf(stderr,"fail to accept connection in recieving RING packet.\n");
    exit(2);
  }
  RingPacket *recv_ring_packet;

  recv_ring_packet = new RingPacket();

  srret=datain->recv_packet(recv_ring_packet);
  if (srret!=Net::SR_success) /* error or timeout*/ {
    fprintf(stderr,"Ring packet cannot be recieved (ret=%d) \n",srret);
    exit(1);  }
  
  if (flag_v) {
    fprintf(stderr,"Sent/Recieved Ring Packet\n");  }
  
  if (!hosts->flag_sync(recv_ring_packet)) {
    fprintf(stderr,"RING information isnot agree. (send!=recv)");
    exit(1);  }
  delete recv_ring_packet;

  //*** 3rd Step: Sending a Host list packet.

  HostPacket *host_packet;

  if (flag_v) printf("Start sending Host Packet\n");

  host_packet = new HostPacket();
  hosts->packetfill(host_packet);
  srret=dataout->send_packet(host_packet);
  if (srret!=Net::SR_success) {
    fprintf(stderr,"fail to send HOST packet. "
	    "buildring()@Server.cpp \n");    exit(1);  }
  delete host_packet;

  //***  4th Step: Recieving the Host list packet.

  HostPacket *recv_host_packet;

  if (flag_v) printf("Start recieving  Host Packet back\n");

  recv_host_packet = new HostPacket();
  srret=datain->recv_packet(recv_host_packet);
  if (srret!=Net::SR_success) /*error or timeout*/ {
    fprintf(stderr,"Host Packet cannot be recieved (ret=%d).\n",srret);
    exit(1);  }
  
  if(!hosts->flag_sync(recv_host_packet)) {
    fprintf(stderr,"HOST information isnot agree. (send!=recv)");
    exit(1);  }

  if(flag_v) {
    fprintf(stderr,"sent/recieved HOST Packet. \n");  }

  hostIte->all_print();
  fprintf(stderr,"******************************************\n");
  fprintf(stderr,"* Host marked with flag=3 has a trouble. *\n");
  fprintf(stderr,"******************************************\n");
  delete recv_host_packet;
  return 0;
}

//**************************
// Sending Files information
//**************************
int file_info()
{
  Net::sr_retval srret;

  FilePacket *file_packet = new FilePacket();  
  files->packetfill(file_packet);
  srret=dataout->send_packet(file_packet);
  if (srret!=Net::SR_success) /*error or timeout*/ { 
    fprintf(stderr,"File Packet cannot be recieved (timeout) \n");
    exit(1);  }
  delete file_packet;
  
  FilePacket *recv_file_packet = new FilePacket();
  srret=datain->recv_packet(recv_file_packet);
  if (srret!=Net::SR_success) /*error or timeout*/ {
    fprintf(stderr,"File Packet cannot be recieved (ret=%d).\n",srret);
    exit(1);  }

  if (!files->flag_sync(recv_file_packet)) {
    fprintf(stderr,"RING information isnot agree. (send!=recv)");
    exit(1);  }
  delete recv_file_packet;
  return 0;
}

///////////////////
// err_ttyopen() //
///////////////////
/* 
void err_ttyopen(char* dev) {
  if ( dev == NULL ) {
     ty_err = stderr;
  } else {
    if ( (ty_err = fopen(dev)) == NULL ) {
      perror("error output stream(%s) open",dev);
      exit(1);
    } 
  }
}
*/      

//////////////
// usage()  //
//////////////     
void usage(char * progname)
{
  fprintf(stderr," %s    [-v] [-d] [-B] -f dollytab ] \n"
          ,progname);
  fprintf(stderr,"            :-v verbose    -d dummy \n");
  fprintf(stderr,"            :-f dollytab  \n");
  fprintf(stderr,
    "            :-B switch off binding of writenet() and readnet().\n"
    "                With this option, you may get faster transfer "
                                 "and less ability\n"
    "                 for recovery \n");
  fprintf(stderr,
    "            :-L switch off unblocking write mode in sending network"
    " packet.\n"
    "                With this option, you may get faster transfer "
                                 "and less ability \n"
    "                 for recovery \n");

}

//***************
// MAIN routine *
//***************
main(int argc, char** argv)
{
  char * configfile="dolly.tab";
  int c;
  
  programName=argv[0];

  /* Parse arguments */
  while (1) {
    c = getopt(argc, argv, "f:vdbBL");
    if (c==(-1)) break;
    switch (c) {
    case 'f':
      /* Only the server should need this in the final version. */
      if (strlen(optarg) > 255) {
        fprintf(stderr, "Name of config-file too long.\n");
        exit(1);
      }
      configfile = optarg;
      break;
      
    case 'v':
      /* Verbose */
      flag_v = true;
      break;
      
    case 'd':
      /* Dummy disk read/write for debugging */
      flag_d = true;
      break;

    case 'B':
      /* Stop readdisk(), writenet() binding */
      fprintf(stderr,"unbinde mode . \n");
      flag_bind = false;
      break;

    case 'L':
      /* stop unblocking write in Net.cpp:send_packet() */
      fprintf(stderr,"block write mode.(does not work connection recorvery\n");
      NetSend::set_block_write();
      break;

    default:
      fprintf(stderr, "Unknown option '%c'.\n", c);
      usage(programName);
      exit(1);
    }
  }
  
  if (flag_d) {
    fprintf(stderr,"Dolly will send dummy data and never stop \n");
  }

  signal(SIGPIPE, SIG_IGN);
  //  err_ttyopen(NULL);

  if (flag_v) {
    if (strcmp(configfile,"-") == 0) {
      fprintf(stderr, "Read configuration from STDIN...  \n",configfile); 
    } else {
      fprintf(stderr, "Read config file(%s)...  \n",configfile); 
    }

    Net::set_verbose();
  }    

  configure(configfile);

  if (flag_v) {
    fprintf(stderr,"Trying to build ring... \n");  }

  buildring();
  file_info();
  data_transfer();
}

