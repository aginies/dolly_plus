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

bool flag_v=false, flag_d=false, flag_bind=true;
bool flag_unblock_write = true;
//these names contain '/0'
char * serverName;  
char * myName;
char * nextName;
bool isNextServer;
PacketIte *hostIte;
PacketIte   *fileIte;

DataOut *dataout=NULL;
DataIn  *datain=NULL;
CntlIn  *cntlin=NULL;
CntlOut *cntlout=NULL;
CntlOut *repout=NULL;

void data_transfer();
void exception_reciever(NetRecv *net );
//FILE *ty_err=NULL;

/*************************
 *      BuildRing        *
 *************************/
DataOut *find_next_and_newnet( PacketIte *ite ) 
  // Return: Success(Dataout), Fail(NULL)
  //
{
  DataOut *net;
  
  if (!ite->pop_entry()) {
    if (isNextServer) {
      fprintf(stderr,"the next host is dolly server. so you"
	 " don't have any option for the next host.\n");
      return(NULL);    }

    isNextServer=true;
    fprintf(stderr,"Server(%s) was selected for the next adjacent host.\n",
	    serverName);
    net = new DataOut(serverName);

  }else{			// find the next in the list
    ite->set_flag(CALL_HOST);
    if (flag_v) {
      fprintf(stderr,"Try sending RING packet to %s\n",ite->get_name()); }
    net = new DataOut(ite->get_name());
  }
  return net;
}

/////////////////
// buildring() //
/////////////////
void buildring() 
{
  int ret,retry;
  Net::ret_value retn;
  Net::sr_retval srret;
  
  //--------------------------------------------------
  //******  1st Step: recieve a RING packet **********
  //--------------------------------------------------

  datain = new DataIn();
  cntlin = new CntlIn();
  cntlin->open();

  datain->register_exception(cntlin,exception_reciever);
  if(datain->open_accept()!=Net::SUCCESS){
    fprintf(stderr,"fail to accept connection in reciving RING packet.\n");
    exit(2);
  }


  RingPacket *ring_packet=new RingPacket();
  srret=datain->recv_packet(ring_packet);
  if (srret!=Net::SR_success) {
    fprintf(stderr,"Ring packet cannot be recieved (timeout)\n");
    exit(1);  }

  hostIte=new PacketIte(ring_packet);
  ret =  hostIte->get_name_len();
  serverName = new char[ret+1];
  strncpy(serverName,hostIte->get_name(),ret);
  serverName[ret]='\0';
  if (!hostIte->search_flag(CALL_HOST)) {
    fprintf(stderr,"cannot find myhost name in RING packet, trying to continue..."
	    "(Client.cpp)\n"); } //exit(1);  }
  ret= hostIte->get_name_len();
  myName = new char [ret+1];
  strncpy(myName,hostIte->get_name(),ret);
  myName[ret]='\0';

  hostIte->set_flag(AVAIL_HOST);
  if (flag_v) {
    fprintf(stderr," Server name is %s, my name is %s.(%s)\n",
	    serverName,myName,getTime());  }

  //-------------------------------------------------------
  //******  2nd Step:  Send the RingPacket to the nexthost;
  //-------------------------------------------------------

  isNextServer=false; //initial value
  dataout = find_next_and_newnet(hostIte);
  if (dataout == NULL) {
    fprintf(stderr,"cannot find the next host in 1st RingPacket"
	    " sending.(1)\n");     exit(1);
  }

  // connection to the adjacent host. upto MAXRETRY hosts are tried if fail.
  for (retry=0;retry<MAXRETRY;retry++) {
    retn = dataout->open_connect();
    if (retn != Net::SUCCESS) {
      if (!isNextServer) {
	hostIte->set_flag(NOAVIL_HOST);
      }else{ // next is Server
	fprintf(stderr,
         "cannot connect to server(%s) in the ring creation step.\n"
           ,serverName);	exit(1);
      }
      dataout->close_sock();
      delete dataout;
      dataout=find_next_and_newnet(hostIte);
      if(dataout == NULL) {
	fprintf(stderr,"cannot fine the next host in 1st "
		"RingPacket sending.(2)\n");    exit(1);  }
      
      continue;  //<-----------continue to retry open_connect
    } // if open fail

    srret=dataout->send_packet(ring_packet);
    if (srret == Net::SR_success) /* send packet success */ { 
      break;     //<----------- success then break retry loop
    }else if (srret==Net::SR_connectionlost)
                          /* packet sending timeout or EPIPE */ {
      fprintf(stderr," fail to send to %s,",hostIte->get_name());
      if (!isNextServer) {
	hostIte->set_flag(NOAVIL_HOST);
      }
      dataout=find_next_and_newnet(hostIte);
      if (dataout==NULL) {
	fprintf(stderr,"cannot fine the next host in 1st "
		"RingPacket sending.(2)\n");    exit(2);  }
    }else /* send packet error*/ {
      fprintf(stderr,"Client.cpp:1st ring packet sending.\n");
      exit(1);
    } /* send packet result if */
  } /* ring retry loop */
  if (retry==MAXRETRY) {
    int retn;
    char message[100];
    CmdPacket rep_pack;

    fprintf(stderr,"send retry failed. (%d times tried)\n",retry);
    repout = new CntlOut(serverName);
    retn =repout->open_connect();
    if (retn != Net::SUCCESS) {
      fprintf(stderr,"cannot report RETRY overrun to %s(1).\n",serverName);
      exit(2);
    }
    sprintf(message,"(<%s): Stop to sending RingPacket(retry=%d).",
	    myName,retry);
    retn = strlen(message);
    rep_pack.make_command(TYPE_REPORT,message,retn);
    if (repout->send_packet(&rep_pack)  != Net::SR_success ) {
      fprintf(stderr,"cannot report RETRY overrun to %s(2).\n",serverName);
      exit(2);
    }
    fprintf(stderr,"Report to server(%s) is succeeded.\n",serverName);
    exit(1);   }

  if (flag_v) {
    fprintf(stderr,"RING packet recieved/sent.\n");  }

  ret = hostIte->get_name_len();
  nextName = new char[ret+1];  // +1 = '\0'
  strncpy(nextName,hostIte->get_name(),ret);
  nextName[ret]='\0';
  delete hostIte;

  //-------------------------------------------------
  //******  3rd Step: Recieving a Host list Packet  *
  //-------------------------------------------------

  HostPacket *host_packet=new HostPacket();
  srret=datain->recv_packet(host_packet);
  if ( srret != Net::SR_success ) {
    fprintf(stderr,"Host packet cannot be recieved (ret=%d)\n",srret);
    exit(1);  }

  //------------------------------------------------------------  
  //******  4th Step: Host List Packet sending to the nexthost *
  //------------------------------------------------------------  

  srret=dataout->send_packet(host_packet);
  if (srret != Net::SR_success) {
    fprintf(stderr,"fail to send Hostpacket to '%s'(ret=%d). \n",
	    nextName,srret);
    exit(1);  }

  if (flag_v) {
    fprintf(stderr,"HOST packet recieved/sent \n");
    host_packet->print();  }

  /* Setting Host iterator again   */
  hostIte=new PacketIte(host_packet);
  if (!hostIte->search_name(nextName)){  // reconfirmation 
    fprintf(stderr,"cannot find the next adjacent host name in HOST "
	    "packet(Client.cpp).\n"); exit(1);  }


  datain->register_exception(0,NULL);
  cntlin->close_sock();
  delete cntlin;
}

void file_info()
{
  Net::sr_retval srret;

  //----------------------------------------------
  //******  5th Step: recieve a File List Packet *
  //----------------------------------------------

  FilePacket *file_packet=new FilePacket();
  srret=datain->recv_packet(file_packet);
  if (srret != Net::SR_success) {
    fprintf(stderr,"File packet cannot be recieved. (ret=%d)\n",srret);
    exit(1);  }

  //-----------------------------------------------------------
  //******  6th Step:  the FilePacket sending to the nexthost *
  //-----------------------------------------------------------
  srret=dataout->send_packet(file_packet);
  if (srret!=Net::SR_success) {
    fprintf(stderr,"fail to send file packet to '%s'.", nextName);
    exit(1);  }

  if (flag_v) {
    fprintf(stderr,"File packet recieved/sent.\n");
    file_packet->print();  }


  fileIte = new PacketIte(file_packet); // set file list iterator
}

//**********************
//*   Main ROUTINE    **
//**********************
void usage(char * progname)
{
  fprintf(stderr,
    " %s    [-v] [-d] [-b] [-t device]\n"          ,progname);
  fprintf(stderr,
	  "            :-v verbose    -d dummy  \n"
	  "            :-p pingfile    ping mode \n");
  fprintf(stderr,
    "            :-B switch off binding of writenet() and readnet().\n"
    "                With this option, you may get faster transfer "
                                 "and less avility \n"
    "                for recovery \n");
  fprintf(stderr,
    "            :-L switch off unblocking write mode in sending network"
    " packet.\n"
    "                With this option, you may get faster transfer "
                                 "and less avility\n"
    "                for recovery \n");
}

int main(int argc, char** argv)
{
  int c;
  bool flag_p=false;
  
  /* Parse arguments */
  while(1) {
    c = getopt(argc, argv, "vdpbBL");
    if (c == -1) break;

    switch(c) {
    case 'v':
      /* Verbose */
      flag_v = true;
      break;

    case 'p':
      /* ping mode */
      flag_p = true;
      break;
      
    case 'd':
      /* Dummy disk read/write for debugging */
      flag_d = true;
      break;
      
    case 'B':
      /* netread() and netwrite() off-binding */
      fprintf(stderr,"unbind mode. \n");
      flag_bind = false;
      break;

    case 'L':
      /* stop unblocking write in Net.cpp:send_packet() */
      fprintf(stderr,"nonblock write mode. \n");
      flag_unblock_write = false;
      break;

    default:
      fprintf(stderr, "Unknown option '%c'.\n", c);
      usage(argv[0]);
      exit(1);
    }
  }
  
  if (flag_d) {
    fprintf(stderr,
            "Dolly will send dummy data and never stop \n");  }

  if (!flag_p) /* normal mode */ {
    signal(SIGPIPE, SIG_IGN);
    
    if (flag_v) {
      fprintf(stderr, "Trying to build ring... %s.\n",getTime());  }

    buildring();
    file_info();
    data_transfer();

    if (flag_v) {
      fprintf(stderr, "Going into multiThread mode... %s\n",getTime());  }

  } else /* ping mode .... this is actually not  dolly */ {

    const char *filename;
    Net::sr_retval srret;
    FilePacket *filepacket = new FilePacket();
    NetRecv *repin = new NetRecv(REPORT);
    FromDisk *afile = new FromDisk();
    Buffer *buff = new Buffer();
    DataPacket *pack = new DataPacket();

    pack->attach(buff);
    while (1) {
      if ( repin->open_accept()!=Net::SUCCESS ) {
	fprintf(stderr,"fail to accept connection in ping mode.\n");
	exit(2);
      }
      if ((srret=repin->recv_packet(filepacket)) != Net::SR_success ) {
	fprintf(stderr,"File name packet recieve failure.(ret=%d)\n",srret);
	exit(2);
      }
      fileIte = new PacketIte(filepacket);
      filename = fileIte->get_name();

      if (!afile->open(filename)) {
	fprintf(stderr,"file open error in ping mode.\n");
	exit(2);
      }
      afile->read_tobuff(buff);
      pack->make(0,0);
      srret=repin->send_packet(pack);
      if (srret !=Net::SR_success) {
	fprintf(stderr,"data send error \n");
      }
      repin->close_sock();
    } /* infinete loop */
  } /* ping mode */
}





