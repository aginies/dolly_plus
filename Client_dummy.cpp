// Client_dummy.cpp is for tests with a config file contains 
// very many 'dummy' client nodes. 
// The client name dummy+something just send as information
// but not used as an actual node candidate.

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "net.h"
#include "Net.h"
#include "Packet.h"
#include "Disk.h"
#include "StopWatch.h"

#define MAXRETRY 3

extern bool flag_v, flag_d, flag_bind;
extern bool flag_unblock_write;

extern char * serverName;  
extern char * myName;
extern char * nextName;
extern bool isNextServer;
extern PacketIte *hostIte;
extern PacketIte   *fileIte;

extern DataOut *dataout;
extern DataIn  *datain;
extern CntlIn  *cntlin;
extern CntlOut *cntlout;
extern CntlOut *repout;

DataOut *find_next_and_newnet( PacketIte *ite ) 
  // Return: Success(Dataout), Fail(NULL)
  //
{
  DataOut *net;
  char tmpName[80];
  int ret;
  
  while(1) {
    if (!ite->pop_entry()) {
      if (isNextServer) {
	fprintf(stderr,"the next host is dolly server. so you"
		" don't have any option for the next host.\n");
	return(NULL);
      }
      isNextServer=true;
      fprintf(stderr,"select Server(%s) for the next adjacent host\n",
	      serverName);
      net = new DataOut(serverName);
      break;
    } else {
      ret = ite->get_name_len();
      if (ret>80) {
	fprintf(stderr,"The next hostname beyond the max. name length. \n");
	exit(2);
      }
      strncpy(tmpName,ite->get_name(),ret);
      tmpName[ret]='\0';
      if (strncmp("dummy",tmpName,5) == 0) {
	ite->set_flag(AVAIL_HOST);
	continue;  //while(1) again!
      } else {
	// find the next in the list
	ite->set_flag(CALL_HOST);
	if (flag_v) {
	  fprintf(stderr,"Try sending RING packet to %s\n",ite->get_name()); }
	net = new DataOut(ite->get_name());
	break;
      }
    }
  } //loop
  return net;
}
