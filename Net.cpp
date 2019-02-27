//D is a mark for debugging
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <fcntl.h>
#include "common.h"
#include "Net.h"
#include "StopWatch.h"

/////////////////////////
/////////////////////////
bool  CntlIn::exist = false;
bool CntlOut::exist = false;
bool  DataIn::exist = false;
bool DataOut::exist = false;

//////////////////////////////////
// just a writter's memo      
//
//  struct sockaddr {
//  	u_short sa_family;	:
//	char sa_data[14];	: depends on the protocol	
//   }
//
//  IPv4's case !!
//  struct sockaddr_in {
//   	short 	sin_family;	: AF_INET 
//	u_short	sin_port;	: 16bit port number with network byte order
//	struct in_addr sin_addr; :32bin netid ex.192.168.197.11 
//                               : with network byte order
//      char sin_zero[8];       : just a pad 8=14-2-4
//  }
//  struct in_addr {
//  	u_long s_addr;		: 32bint netid(IP address), networkbyte order
//  }
//////////////////////////////////
//         Net                  //
//////////////////////////////////
Net::Net() 
  :  sockDesc(-1), closedYes(false), 
     writeTimeout(SENDTIMEOUT), readTimeout(RECVTIMEOUT) {
}

Net::~Net()
{
  if (!closedYes) {
    close(sockDesc);
  }
}

int Net::flag_v = 0;

///////////////////////////////////////////////
//         Net for Active connection         //
///////////////////////////////////////////////

NetSend::NetSend(const char *host, const int port)
  : exceptionNet(NULL), exceptionFunction(NULL)
{
  int ret;

  portNo=port;
  if (host == NULL) { 
    hostName = NULL;
  }else{
    ret=strlen(host)+1;
    if (ret>MAX_HOST_NAME_LEN) {
    fprintf(stderr,"hostname is too long. (%s) NetSend::new \n",
	    host);     exit(1);   }
    hostName = new char[ret];
    strncpy(hostName,host,ret);

    //D fprintf(stderr,"Netsend hostname=%s (manabe)\n",hostName);
  }
  // Net();  //manabe test
}

NetSend::~NetSend()
{
  delete hostName;
}

int NetSend::flag_nonblock_write=1;

//**********************************************//
// Active open a network for sending to a host  //
// Return: Success(0) Error(-1) Fatal(-2)       //
//**********************************************//
Net::ret_value NetSend::open_connect()
{
  struct hostent *hent;
  struct sockaddr_in addrdata, addrctrl;
  int ret,dataok,no_try;
  unsigned long inaddr;
  int option,optlen;

  //D fprintf(stderr,"start Connection %d (manabe)\n",flag_v);
  //hostName is IP address or a real name ?
  if ( (inaddr = inet_addr(hostName)) != INADDR_NONE) {
    // addrdata for the later connect() call 
    if (flag_v) fprintf(stderr,"use IP number address %s\n",hostName);
    bcopy((char*)&inaddr, (char *)&addrdata.sin_addr,sizeof(inaddr));
  } else {
    if (flag_v) fprintf(stderr,"use hostname '%s'\n",hostName); 
    if ((hent=gethostbyname(hostName))==NULL) {
      fprintf(stderr,"hostname=%s..\n",hostName);
      herror("gethostbyname@Net.cpp");
      return(FATAL);
    }
    if (hent->h_addrtype != AF_INET) {
      fprintf(stderr, "Expected h_addrtype of AF_INET, got %d\n",
	      hent->h_addrtype);
      return(FATAL); }
    bcopy(hent->h_addr, &addrdata.sin_addr, hent->h_length);
  }
  addrdata.sin_family = AF_INET;
  addrdata.sin_port = htons(portNo);

  /* Setup data port */

  if (flag_v) {
    fprintf(stderr, "Connecting to %s...",hostName);
    fflush(stderr);
  }
  
  /* Wait until we connected to everything... */
  no_try = 0; dataok=0;
  while (no_try<=MAX_CONNECT_TRY) {
    if ((sockDesc = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
      perror("Opening output data socket");      return(FATAL);    }
    
    
    ret=connect(sockDesc,(struct sockaddr *)&addrdata,
		sizeof(addrdata));
    if ((ret == -1) && 
	( errno==ECONNREFUSED || errno==ENETUNREACH || 
	  errno==EHOSTUNREACH ) ) {
      close(sockDesc);
    } else if(ret == -1) {
      fprintf(stderr,"errno=%d \n",errno);
      perror("connect at NetSend::open_connect() ");   return(FATAL);
    } else {
      if (flag_v) {
	fprintf(stderr, "..(host=%s::%d)",hostName,portNo);
	fprintf(stderr, "\n");
      }                       
      ///manabe added for test, manabetest
      //      optlen = sizeof(option); 
      //      option = 1;
      //      if ( setsockopt(sockDesc, SOL_IP, IP_RECVERR, &option,
      //		      (socklen_t)optlen)   < 0 ) {
      //	perror("setsockopt");
      //	exit(1);
      //      }
      //      if ( getsockopt(sockDesc, SOL_IP, IP_RECVERR, &option,
      //		      (socklen_t*)&optlen)   < 0 ) {
      //	perror("getsockopt");
      //	exit(1);
      //      }
      //      printf("==========================\n");
      //      printf("option=%d length=%d \n",option,optlen);
      //      printf("==========================\n");

      if (flag_nonblock_write) {
	if (fcntl(sockDesc,F_SETFL,O_NONBLOCK)<0) {
	  fprintf(stderr,"fail to set NOBLOCK mode in open_connect()\n");
	  perror("fcntl");
	}
	if (flag_v) {
	  fprintf(stderr,"setting NOBLOCK mode in open_connect()\n");
	}
      }

                              ////////////////////////
      return(SUCCESS);               // <--- normal return //
                              ////////////////////////
    }
    sleep(1);
    no_try++;
  }  // connect loop
  fprintf(stderr,"Connection to %s is faild."
	  "(#_try>%d)\n",hostName,no_try);
  return(ERROR);
}

void NetSend::close_sock()
{
  close(sockDesc);
  closedYes=true;
}

//***************************************************//
// Send a packet                                     //
// return: normal(1), timeout or EPIPE(0), error(-1) //
//***************************************************//
Net::sr_retval NetSend::send_packet(Packet *pack)
{
  using namespace NetSendRecv;
  fd_set save_rset, save_wset;
  int maxsetnr; int rest_bytes;
  int exp_sock;
  const char *pp;

  if (sockDesc == (-1)) {
    fprintf(stderr,"coding error! call NetSend::open_connect() first"
	    " NetSend::send_packet(pack) \n");   exit(3);
  }
  FD_ZERO(&save_wset); FD_ZERO(&save_rset);
  FD_SET(sockDesc,&save_wset);

  if (exceptionFunction != NULL ) {
    exp_sock = exceptionNet->get_accept_sock();
    if (exp_sock==(-1)) {
      exp_sock = exceptionNet->get_sock();
    }
    FD_SET(exp_sock,&save_rset);
    maxsetnr= (exp_sock>sockDesc)?exp_sock:sockDesc;
  } else {
    maxsetnr=sockDesc;
  }

  maxsetnr++;
  pp = pack->get_addr();
  rest_bytes=pack->get_size();

  int ret=NetSendRecv::sending(save_rset,save_wset,maxsetnr,
			       pp,rest_bytes, writeTimeout, sockDesc,
			       exp_sock,exceptionNet,exceptionFunction);
  if (ret==r_OK) {
    return(SR_success);
  } else if (ret==r_TIMEOUT || ret==r_EPIPE) {
    return(SR_connectionlost);
  } else {
    fprintf(stderr,"fail to send packet NetSend::send_packet()\n");
    return(SR_error);
  }
}     

////////////////////////////////////////////////////////
// Send a Spacket                                     //
// return: normal(1), timeout or EPIPE(0), error(-1)  //
////////////////////////////////////////////////////////
Net::sr_retval NetSend::send_packet(SPacket *pack)
{
  using namespace NetSendRecv;
  fd_set save_rset, save_wset;
  int maxsetnr; int rest_bytes;
  int exp_sock;
  const char *pp;
  
  if (sockDesc==(-1)){
    fprintf(stderr,"coding error! call NetSend::open_connect() first"
	    " in NetSend::send_packet(SPacket)\n");   exit(3);
  }
  FD_ZERO(&save_wset); FD_ZERO(&save_rset);
  FD_SET(sockDesc,&save_wset);
  
  if (exceptionFunction != NULL) {
    exp_sock = exceptionNet->get_accept_sock();
    if (exp_sock==(-1)) {
      exp_sock = exceptionNet->get_sock();
    }
    FD_SET(exp_sock,&save_rset);
    maxsetnr= (exp_sock>sockDesc)?exp_sock:sockDesc;
  } else {
    maxsetnr=sockDesc;
  }
  maxsetnr++;
  
  pp = pack->get_haddr();
  rest_bytes=pack->get_headersize();

  int ret=NetSendRecv::sending(save_rset,save_wset,maxsetnr,
			       pp,rest_bytes, writeTimeout, sockDesc,
			       exp_sock,exceptionNet,exceptionFunction);
  if (ret==r_OK) {
  } else if (ret==r_TIMEOUT || ret==r_EPIPE) {
    return(SR_connectionlost);
  } else {
    fprintf(stderr,"fail to send packet header "
	    "NetSend::send_packet(spacket)\n");
    return(SR_error);
  }
  pp=pack->get_contaddr();
  rest_bytes=pack->get_contsize();
  ret=NetSendRecv::sending(save_rset,save_wset,maxsetnr,
			   pp,rest_bytes, writeTimeout, sockDesc,
			   exp_sock,exceptionNet,exceptionFunction);
  if (ret==r_OK) {
  } else if (ret==r_TIMEOUT || ret==r_EPIPE) {
    return(SR_connectionlost);
  } else{
    fprintf(stderr,"fail to send data packet(contents)"
	    "NetSend::send_packet(spacket)\n");
    return(SR_error);
  } 
  return(SR_success);
}     

//********************************************//
// recieving a packet                         //
// return:  normal(1), timeout(0), error(-1)  //
//********************************************//
Net::sr_retval NetSend::recv_packet(Packet *pack)
{
  using namespace NetSendRecv;
  char *pp;
  fd_set save_rset;
  int exp_sock, maxsetnr, rest_bytes;

  if (sockDesc==(-1)) {
    fprintf(stderr,"coding error! Call open() first."
	" in NetSend::recv_packet() \n");    exit(3);
  }
  FD_ZERO(&save_rset); 
  FD_SET(sockDesc,&save_rset);
  if (exceptionFunction != NULL) {
    exp_sock = exceptionNet->get_accept_sock();
    if (exp_sock==(-1)) {
      exp_sock = exceptionNet->get_sock();
    }
    FD_SET(exp_sock,&save_rset);
    maxsetnr= (exp_sock>sockDesc)?exp_sock:sockDesc;
  } else {
    maxsetnr= sockDesc;
  }
  maxsetnr++;
  pp=pack->get_haddr();
  rest_bytes = pack->get_headersize();
  if (rest_bytes>HEADER_MAXLEN) {
    fprintf(stderr," exceed header size limit."
      " (NetSend::recv_packet(packet) coding error). %s\n", getTime());
    exit(3);
  }

  int ret=NetSendRecv::recving(save_rset,maxsetnr,pp,rest_bytes,
         readTimeout,sockDesc,exp_sock,exceptionNet,exceptionFunction);
  if (ret == r_OK) {
    // do continue
  } else if (ret == r_TIMEOUT) {
    return(SR_connectionlost);
  } else {
    fprintf(stderr,"fail to reciev packet NetSend::recv_packet()(1)\n");
    return(SR_error);
  }

  if ((pp=pack->checkheader_make())==NULL) {
    fprintf(stderr,"recived data header mismatch "
	    "NetSend::recv_packet(pack) %s\n",getTime()); 
    exit(3);  }
  rest_bytes=pack->get_contsize()+pack->get_trailersize();

  ret=NetSendRecv::recving(save_rset,maxsetnr,pp,rest_bytes,
	readTimeout,sockDesc,exp_sock,exceptionNet,exceptionFunction);
  if (ret == r_OK) {
    // do continue
  } else if (ret == r_TIMEOUT ) {
    return(SR_connectionlost);
  } else {
    fprintf(stderr,"fail to reciev packet NetSend::recv_packet()(2)\n");
    return(SR_error);
  }

  if (!pack->checktrailer()) {
    fprintf(stderr,"checktrailer() fail NetSend::recv_packet()\n");
    pack->print();
    return(SR_error);
  }
  return(SR_success);
}

////////////////////////////////////////////////
// recieving a Spacket                        //
// return:  normal(1), timeout(0), error(-1)  //
////////////////////////////////////////////////
Net::sr_retval NetSend::recv_packet(SPacket *pack)
{
  using namespace NetSendRecv;
  char *pp;
  fd_set save_rset;
  int maxsetnr; int rest_bytes;
  int exp_sock;
  
  if (sockDesc==(-1)) {
    fprintf(stderr,"coding error! Call open() first"
	    " in NetSend::recv_packet(SPacket) \n");
    exit(3);
  }
  FD_ZERO(&save_rset); 
  FD_SET(sockDesc,&save_rset);
  if (exceptionFunction != NULL) {
    exp_sock = exceptionNet->get_accept_sock();
    if (exp_sock==(-1)) {
      exp_sock = exceptionNet->get_sock();
    }
    FD_SET(exp_sock,&save_rset);
    maxsetnr= (exp_sock>sockDesc)?exp_sock:sockDesc;
  } else {
    maxsetnr= sockDesc;
  }
  maxsetnr++;
  pp=pack->get_haddr();
  rest_bytes = pack->get_headersize();
  if (rest_bytes>HEADER_MAXLEN) {
    fprintf(stderr," exceed header size limit."
      " (NetRecv::recv_packet(SPacket) coding error) \n"); exit(3);
  }
  int ret=NetSendRecv::recving(save_rset,maxsetnr,pp,rest_bytes,
       readTimeout,sockDesc,exp_sock,exceptionNet,exceptionFunction);
  if (ret==r_OK) {
    //do continue
  } else if (ret==r_TIMEOUT) {
    return(SR_connectionlost);
  } else {
    fprintf(stderr,"fail to reciev packet NetSend::recv_packet(spacket)(3)\n");
    return(SR_error);
  }

  if (pack->checkheader_make()==NULL) {
    fprintf(stderr,"recieved data header mismatch "
	    "NetSend::recv_packet(SPacket) \n");
    return(SR_error);
  }
  rest_bytes = pack->get_contsize();
  pp=(char *)pack->get_contaddr();

  ret=NetSendRecv::recving(save_rset,maxsetnr,pp,rest_bytes,readTimeout,
	      sockDesc,exp_sock,exceptionNet,exceptionFunction);
  if (ret==r_OK) {
    
  } else if (ret==r_TIMEOUT) {
    return(SR_connectionlost);
  } else {
    fprintf(stderr,"fail to recieve SPacket NetSend::recv_packet(SP)\n");
    return(SR_error);
  }
  return(SR_success);
}

//
// Registrate a function which will be called when
// a packet is sent from control port
// If function is NULL, catching exceptional access is disabled.
NetRecv::T_FUNC
    NetSend::register_exception(NetRecv *net, void function(NetRecv *))
{
  exceptionFunction = function;
  exceptionNet=net;
  return(exceptionFunction);
}


/////////////////////////////////////////////
////        Net for Passive Connection     //
/////////////////////////////////////////////

NetRecv::NetRecv(const int port) 
  : exceptionNet(NULL), exceptionFunction(NULL),
    accptSock(-1)
{
  int ret;
  hostName = FROM_ANY; //from any interface. hostName is not a right name
  portNo=port;
}

NetRecv::~NetRecv()
{
  if (!closedYes) {
    close(sockDesc);
    if (accptSock!=(-1)) 
      close(accptSock);
  }
  delete hostName;
}

void NetRecv::close_sock()
{
  closedYes = true;
  close(sockDesc);
  if(accptSock != (-1)) {
     close(accptSock);
  }
}

////////////////////////////////////////////////////////
//  Passive Open network for recieving from any hosts //
////////////////////////////////////////////////////////
Net::ret_value NetRecv::open_accept()
{
  struct sockaddr_in addr;
  int optval,size=0;
  struct hostent *hent;
  int acptret;

  sockDesc = socket(PF_INET, SOCK_STREAM, 0);
  if (sockDesc==-1) {
    perror("Opening input data socket");    return(FATAL);   }
  optval = 1;
  if (setsockopt(sockDesc,SOL_SOCKET,SO_REUSEADDR,
                &optval, sizeof(int)) == -1) {
    perror("setsockopt on dataSock");    return(FATAL);   }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(portNo);
  addr.sin_addr.s_addr = htonl(INADDR_ANY); // from any interface
    
  if (bind(sockDesc,(struct sockaddr *)&addr,sizeof(addr))==-1) {
    perror("binding input data socket");     return(FATAL);  }

  if (listen(sockDesc,2)==-1) { /* queue is 2...Manabe */
    perror("listen input data socket");    return(FATAL);  }

  if (exceptionFunction == NULL) {
    fprintf(stderr,"Accepting port(%d).....",portNo); 
    fflush(stderr);
    accptSock = ::accept(sockDesc,NULL,(socklen_t*)&size);
    if (accptSock<0) {
      perror("Acception error");
      return(FATAL);
    }
  } else {
    fd_set use_rset,rset; struct timeval timeout;
    timeout.tv_sec=timeout.tv_usec=0;
    int selret;
    int exp_sock,maxsetnr;
    
    fprintf(stderr,"Accepting port(%d).....",portNo); 
    fflush(stderr);
    FD_ZERO(&rset); FD_SET(sockDesc,&rset);
    exp_sock= exceptionNet->get_accept_sock();
    if (exp_sock==(-1)) {
      exp_sock = exceptionNet->get_sock();
    }
    FD_SET(exp_sock,&rset);
    maxsetnr = (exp_sock>sockDesc)?exp_sock:sockDesc;
    maxsetnr++;
    
    while(1) {
      use_rset = rset;
      selret=select(sockDesc+1,&use_rset,NULL,NULL,NULL);

      if(selret>0) {
	if (FD_ISSET(sockDesc,&use_rset)) {
	  break;
	} else {		// must be exception access
	  exceptionFunction(exceptionNet);
	}
      } else /*select error*/ { 
	perror("Net::open_accept() select()");
	return(FATAL);
      }
    } // while(1) loop
    accptSock = ::accept(sockDesc,NULL,(socklen_t*)&size);
    if (accptSock<0) {
      perror("Acception error");
      return(FATAL);
    }
  }   // is exceptionFunction set ?
  return(SUCCESS);
}

//
// Separated routines
//
void NetRecv::open()
{
  struct sockaddr_in addr;
  int optval;
  struct hostent *hent;

  sockDesc = socket(PF_INET, SOCK_STREAM, 0);
  if (sockDesc==(-1)) {
    perror("Opening input data socket");
    fprintf(stderr,"%s\n",getTime());
    exit(2);   }
  optval = 1;
  if (setsockopt(sockDesc,SOL_SOCKET,SO_REUSEADDR,
                   &optval,sizeof(int))==(-1)) {
    perror("setsockopt on dataSock"); 
    fprintf(stderr,"%s\n",getTime());exit(2);   }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(portNo);
  addr.sin_addr.s_addr = htonl(INADDR_ANY); // from any interface
    
  if (bind(sockDesc,(struct sockaddr *)&addr,sizeof(addr))==-1) {
    perror("binding input data socket");
    fprintf(stderr,"%s\n",getTime());exit(2);  }

  if (listen(sockDesc,2)==-1) { /* queue is 2...Manabe */
    perror("listen input data socket"); 
    fprintf(stderr,"%s\n",getTime());exit(2);  }
}

//
// Return ... Fail(0)  .. Accepted(1)
Net::ret_value NetRecv::nonblock_accept()
{
  socklen_t size=0;
  struct sockaddr_in peer_sin;
  struct hostent *peer_host;
  
  fcntl(sockDesc,F_SETFL,O_NONBLOCK);
  fprintf(stderr,"Nonblock Accept from port(%d) ...\n",portNo); 
  if ((accptSock = ::accept(sockDesc,NULL,(socklen_t*)&size)) < 0 ){
    if (errno==EAGAIN) {
      return(ERROR);
    } else {
      perror("Net::noblock_accept()");
      return(FATAL);
    }
  }

  if (flag_v) {
    fprintf(stderr," peer host is ..");
    size = sizeof peer_sin;
    if (getpeername(accptSock,(struct sockaddr*)&peer_sin,&size) == -1) {
       perror("fail getpeername()");
       return(ERROR);
    }
    if ((peer_host=gethostbyaddr((char*)&peer_sin.sin_addr,
            sizeof peer_sin.sin_addr, AF_INET)) == NULL) {
       perror("fail gethostbyaddr()");
       return(ERROR);
    }
    fprintf(stderr,"%s\n",peer_host->h_name);
  }
  return(SUCCESS);
}


// accept wait without exception handling
// Return ... Fail(0)  .. Accepted(1)
Net::ret_value NetRecv::accept()
{
  socklen_t size=0;
  struct sockaddr_in peer_sin;
  struct hostent *peer_host;
  
  fprintf(stderr,"Wait for some one access at port(%d) ...\n",portNo); 
  if ((accptSock = ::accept(sockDesc,NULL,(socklen_t*)&size)) < 0 ){
    if (errno==EAGAIN) {
      return(ERROR);
    } else {
      perror("Net::noblock_accept()");
      return(FATAL);
    }
  }

  if (flag_v) {
    fprintf(stderr," peer host is ..");
    size = sizeof peer_sin;
    if (getpeername(accptSock,(struct sockaddr*)&peer_sin,&size) == -1) {
       perror("fail getpeername()");
       return(ERROR);
    }
    if ((peer_host=gethostbyaddr((char*)&peer_sin.sin_addr,
            sizeof peer_sin.sin_addr, AF_INET)) == NULL) {
       perror("fail gethostbyaddr()");
       return(ERROR);
    }
    fprintf(stderr,"%s\n",peer_host->h_name);
  }
  return(SUCCESS);
}

//
// Registrate a function which will be called when
// a packet is sent from control port
//
NetRecv::T_FUNC 
   NetRecv::register_exception(NetRecv *net, void function(NetRecv *))
{
  exceptionFunction = function;
  exceptionNet = net;
  return(exceptionFunction);
}

//********************************************//
// recieving a packet                         //
// return:  normal(1), timeout(0), error(-1)  //
//********************************************//
Net::sr_retval NetRecv::recv_packet(Packet *pack)
{
  using namespace NetSendRecv;
  char *pp;
  fd_set save_rset;
  int exp_sock, maxsetnr, rest_bytes;

  //D fprintf(stderr,"NetRecv::Recv_pacekt(Packet) (manabe) \n");  
  if (accptSock==(-1)) {
    fprintf(stderr,"coding error! you try recv_packet() but did not call "
	    "open() first in NetRecv::recv_packet() (port=%d)\n",portNo);
    //    return(-1); for debug
    exit(3);
  }
  FD_ZERO(&save_rset); 
  FD_SET(accptSock,&save_rset);
  if (exceptionFunction != NULL) {
    exp_sock = exceptionNet->get_accept_sock();
    if (exp_sock==(-1)) {
      exp_sock = exceptionNet->get_sock();
    }
    FD_SET(exp_sock,&save_rset);
    maxsetnr= (exp_sock>accptSock)?exp_sock:accptSock;
  } else {
    maxsetnr= accptSock;
  }
  maxsetnr++;
  //D char * mm;
  pp=pack->get_haddr();
  //D fprintf(stderr,"NetRecv::Recv_pacekt(Packet) header add=%x(manabe) \n",pp);  
  //Dmm=pp;
  rest_bytes = pack->get_headersize();
  if( rest_bytes > HEADER_MAXLEN ) {
    fprintf(stderr," exceed header size limit."
      " (NetRecv::recv_packet(packet) coding error) \n"); exit(3);
  }

  int ret=NetSendRecv::recving(save_rset,maxsetnr,pp,rest_bytes,
	  readTimeout,accptSock,exp_sock,exceptionNet,exceptionFunction);
  if (ret == r_OK) {
    //do continue
  } else if (ret==r_TIMEOUT) {
    return(SR_connectionlost);
  } else {
    fprintf(stderr,"fail to reciev packet NetRecv::recv_packet()(1)\n");
    return(SR_error);
  }
  //D fprintf(stderr,"packet header is %c %c %c (manabe)\n",*mm,*(mm+1),*(mm+2));

  if ((pp=pack->checkheader_make())==NULL) {
    fprintf(stderr,"recived data header mismatch Net::recv_packet() %s\n",
	    getTime());    exit(3);  }
  rest_bytes = pack->get_contsize() + pack->get_trailersize();

  ret=NetSendRecv::recving(save_rset,maxsetnr,pp,rest_bytes,readTimeout,
	      accptSock,exp_sock,exceptionNet,exceptionFunction);
  if (ret==r_OK) {
    //do continue
  } else if (ret==r_TIMEOUT) {
    return(SR_connectionlost);
  } else {
    fprintf(stderr,"fail to reciev packet NetRecv::recv_packet()(2)\n");
    return(SR_error);
  }
  //D fprintf(stderr,"pack->checkheder_make addr=%x(manabe)\n",pp);

  if (!pack->checktrailer()) {
    fprintf(stderr,"checktrailer() fail in NetRecv::recv_packet()\n");
    pack->print();
    return(SR_error);
  }
  return(SR_success);
}


////////////////////////////////////////////////
// recieving a Spacket                        //
// return:  normal(1), timeout(0), error(-1)  //
////////////////////////////////////////////////
Net::sr_retval NetRecv::recv_packet(SPacket *pack)
{
  using namespace NetSendRecv;
  char *pp;
  fd_set save_rset;
  int maxsetnr; int rest_bytes;
  int exp_sock;
  
  if (accptSock==(-1)) {
    fprintf(stderr,"coding error! Call open() first"
	    " in NetRecv::recv_packet(SPacket) \n");
    exit(3);
  }
  FD_ZERO(&save_rset); 
  FD_SET(accptSock,&save_rset);
  if (exceptionFunction != NULL) {
    exp_sock = exceptionNet->get_accept_sock();
    if (exp_sock==(-1)) {
      exp_sock = exceptionNet->get_sock();
    }
    FD_SET(exp_sock,&save_rset);
    maxsetnr= (exp_sock>accptSock)?exp_sock:accptSock;
  } else {
    maxsetnr= accptSock;
  }
  maxsetnr++;
  pp=pack->get_haddr();

  rest_bytes = pack->get_headersize();
  if (rest_bytes>HEADER_MAXLEN) {
    fprintf(stderr," exceed header size limit."
      " (NetRecv::recv_packet(SPacket) coding error) \n"); exit(3);
  }
  int ret=NetSendRecv::recving(save_rset,maxsetnr,pp,rest_bytes,
       readTimeout,accptSock,exp_sock,exceptionNet,exceptionFunction);
  if (ret==r_OK) {
    //do continue
  } else if (ret==r_TIMEOUT) {
    return(SR_connectionlost);
  } else {
    fprintf(stderr,"fail to reciev packet NetRecv::recv_packet()(3)\n");
    return(SR_error);
  }

  if (pack->checkheader_make()==NULL) {
    fprintf(stderr,"recieved data header mismatch "
	    "NetRecv::recv_packet(SPacket) \n");   
    return(SR_error);
  }
  rest_bytes = pack->get_contsize();
  pp=(char *)pack->get_contaddr();

  ret=NetSendRecv::recving(save_rset,maxsetnr,pp,rest_bytes,readTimeout,
	      accptSock,exp_sock,exceptionNet,exceptionFunction);
  if (ret==r_OK) {

  } else if (ret==r_TIMEOUT) {
    return(SR_connectionlost);
  } else {
    fprintf(stderr,"fail to recieve SPacket NetRecv::recv_packet(SP)\n");
    return(SR_error);
  }
  return(SR_success);
}

//***************************************************//
// Send a packet                                     //
// return: normal(1), timeout or EPIPE(0), error(-1) //
//***************************************************//
Net::sr_retval NetRecv::send_packet(Packet *pack)
{
  using namespace NetSendRecv;
  fd_set save_rset, save_wset;
  int maxsetnr; int rest_bytes;
  int exp_sock;
  const char *pp;

  if (accptSock==(-1)) {
    fprintf(stderr,"coding error! call NetRecv::open_accpt() first in"
	    " NetRecv::send_packet(pack)   \n");   exit(3);
  }
  FD_ZERO(&save_wset); FD_ZERO(&save_rset);
  FD_SET(accptSock,&save_wset);
  if (exceptionFunction != NULL) {
    exp_sock = exceptionNet->get_accept_sock();
    if (exp_sock==(-1)) {
      exp_sock = exceptionNet->get_sock();
    }
    FD_SET(exp_sock,&save_rset);
    maxsetnr= (exp_sock>accptSock)?exp_sock:accptSock;
  } else {
    maxsetnr= accptSock;
  }
  maxsetnr++;
  pp = pack->get_addr();
  rest_bytes=pack->get_size();

  int ret=NetSendRecv::sending(save_rset,save_wset,maxsetnr,
			       pp,rest_bytes, writeTimeout, accptSock,
			       exp_sock,exceptionNet,exceptionFunction);
  if (ret==r_OK) {

  } else if (ret==r_TIMEOUT || ret==r_EPIPE) {
    return(SR_connectionlost);
  } else {
    fprintf(stderr,"fail to send packet NetRecv::send_packet()\n");
    return(SR_error);
  }
  return(SR_success);
}     
////////////////////////////////////////////////////////
// Send a Spacket                                     //
// return: normal(1), timeout or EPIPE(0), error(-1)  //
////////////////////////////////////////////////////////
Net::sr_retval NetRecv::send_packet(SPacket *pack)
{
  using namespace NetSendRecv;
  fd_set save_rset, save_wset;
  int maxsetnr; int rest_bytes;
  int exp_sock;
  const char *pp;

  if (sockDesc==(-1)){
    fprintf(stderr,"coding error! call NetRecv::open_accpt() first"
	    " in NetRecv::send_packet(SPacket)\n");   exit(3);
  }
  FD_ZERO(&save_wset); FD_ZERO(&save_rset);
  FD_SET(accptSock,&save_wset);
  
  if (exceptionFunction != NULL) {
    exp_sock = exceptionNet->get_accept_sock();
    if (exp_sock==(-1)) {
      exp_sock = exceptionNet->get_sock();
    }
    FD_SET(exp_sock,&save_rset);
    maxsetnr= (exp_sock>accptSock)?exp_sock:accptSock;
  } else {
    maxsetnr=accptSock;
  }
  maxsetnr++;
  
  pp = pack->get_haddr();
  rest_bytes=pack->get_headersize();

  int ret=NetSendRecv::sending(save_rset,save_wset,maxsetnr,
			       pp,rest_bytes, writeTimeout, accptSock,
			       exp_sock,exceptionNet,exceptionFunction);
  if (ret==r_OK) {
    //do continue;
  } else if (ret==r_TIMEOUT || ret==r_EPIPE) {
    return(SR_connectionlost);
  } else {
    fprintf(stderr,"fail to send packet header "
	    "NetRecv::send_packet(spacket)\n");
    return(SR_error);
  }
  pp=pack->get_contaddr();
  rest_bytes=pack->get_contsize();
  ret=NetSendRecv::sending(save_rset,save_wset,maxsetnr,
			   pp,rest_bytes, writeTimeout, accptSock,
			   exp_sock,exceptionNet,exceptionFunction);
  if (ret==r_OK) {

  } else if (ret==r_TIMEOUT || ret==r_EPIPE) {
    return(SR_connectionlost);
  } else{
    fprintf(stderr,"fail to send data packet(contents)"
	    "NetRecv::send_packet(spacket)\n");
    return(SR_error);
  } 
  return(SR_success);
}     

//****************************************************//
//                  Inline's                          //
//****************************************************//
namespace NetSendRecv {
  /// return   -1=(error) 0=(timeout or pipebroken) 1=(normal)
  /// return   r_ERROR(-1),r_OK(0),r_TIMEOUT(1),r_UEOF(2)
  inline ret_srecving sending(
	const fd_set &rset, const fd_set &wset,
	const int &maxnr,
	const char *addr, const int &bytes, int timelimit,
	const int &sock, 
	const int &ex_sock, NetRecv *& ex_net, void (*func)(NetRecv *))
  {
    const char *pp=addr;          
    int r_bytes=bytes,ret,selret;
    fd_set use_rset,use_wset;          struct timeval timeout;

    for (;r_bytes>0;r_bytes-=ret,pp+=ret) {
      use_wset=wset;
      timeout.tv_sec=timelimit; timeout.tv_usec=0; 
      ret=0;
      // Linux rule! see select(2)

      if (func!=NULL) {
	use_rset=rset;
	selret=select(maxnr,&use_rset,&use_wset,NULL,&timeout);
      } else {
	selret=select(maxnr,NULL,&use_wset,NULL,&timeout);
      }

      if (selret>0) {		// normal result
	if (FD_ISSET(sock,&use_wset)) {
	write_again:
	  DEBPR(printf("write..%dbytes add=%d>>\n",r_bytes,pp););
	  ret = write(sock,(char *)pp,r_bytes);
	  DEBPR(printf("done..ret=%dbytes<<\n",ret););
	  if (ret<=0) {
	    // 2001.8.2 modified
	    //	    if (errno==EPIPE) /* pipe broken*/ {
	    //return(r_TIMEOUT);
	    //	    }else{
	    //	      perror("::write() in Net::sending()");
	    //	      return(r_ERROR);
	    //	    }
	    if (errno==EBADF || errno==EINVAL || errno==EFAULT ||
		errno==ENOSPC || errno==EIO ) {
	      perror("unrecoverable error ::write() in Net::sending()");
	      return(r_ERROR);
	    } else if (errno==EAGAIN) {
	      sleep(1);
	      DEBPR(printf("EAGAIN\n"););
	      goto write_again;
	    }else{
	      DEBPR(printf("TIMEOUT\n"););
	      if (Net::is_verbose()) {
		printf("write TIMEOUT happned (%d)\n",errno);
		perror("reason");
	      }
	      return(r_TIMEOUT);
	    }
	  } /* write error */
	  DEBPR(printf("writeend\n"););
	}
	if (func!=NULL && FD_ISSET(ex_sock,&use_rset)) {
	  func(ex_net);
	}
      } else if (selret==0) {		// timeout
	return(r_TIMEOUT);
      }else{
	perror("Net::send_packet select()");
	return(r_ERROR);
      }
      DEBPR(printf("next\n"););
    }//write loop
    return(r_OK);
  }
  
  /// return   r_ERROR(-1),r_OK(0),r_TIMEOUT(1),r_UEOF(2)
  inline ret_srecving recving(
         const fd_set &rset, const int &maxnr,
	 char * addr, const int &bytes,  int timelimit, 
	 const int &sock, 
         const int &ex_sock, NetRecv *&ex_net, void (*func)(NetRecv *))
  {
    char *pp=addr;
    int ret,selret;
    fd_set use_rset,save_rset;
    struct timeval timeout;
    int r_bytes=bytes;
    
    save_rset=rset;
    for (; r_bytes>0; r_bytes-=ret,pp+=ret) {
      use_rset=save_rset;
      timeout.tv_sec=timelimit; timeout.tv_usec=0; 
      ret=0;
      DEBPR(printf("recving select=%o \n",use_rset););
      selret=select(maxnr,&use_rset,NULL,NULL,&timeout);
      DEBPR(printf("recving select ret=%d \n",selret););
      if (selret>0) {		// normal result
	if (FD_ISSET(sock,&use_rset)) {
	  if ((ret=read(sock,pp,r_bytes))<0){
	    perror("Net(h)::recving read(header)");
	    return(r_ERROR);
	  } else if (ret==0) /* EOF read*/ {
	    struct hostent *peer_host; 
	    struct sockaddr_in peer_sin;
	    socklen_t size=sizeof peer_sin;
	    int ret_r;
	    ret_r=getpeername(sock,(struct sockaddr*)&peer_sin,&size);
	    fprintf(stderr,"unexpected EOF in Net::recving read() %s"
		    "(restbytes=%d)",getTime(),r_bytes);
	    if(ret_r != -1) {
	      peer_host=gethostbyaddr((char*)&peer_sin.sin_addr,
		sizeof peer_sin.sin_addr, AF_INET);
	      if (peer_host != NULL) {
		fprintf(stderr,"from host=%s(port=%d)",peer_host->h_name,
			ntohs(peer_sin.sin_port));
	      }
	    }
	    fprintf(stderr,"\n");
	    // return(-1); 2001.5.1.15
	    if (r_bytes == bytes) {
	      fprintf(stderr,"detect 'exception' port closing.\n");
	      FD_CLR(sock,&save_rset);
	      continue;
	    } else {
	      fprintf(stderr,"Stop data reading!! Only wait for exception port"
		      " access. %s\n",getTime());
	      FD_CLR(sock,&save_rset);
	      continue; 
	    }
	  } /* read/EOF */
	} /* sock ISSET? */
	if (func!=NULL && FD_ISSET(ex_sock,&use_rset)) {
	  func(ex_net);
	}
      } else if (selret==0) {		// timeout
	return(r_TIMEOUT);
      } else {
	perror("Net::recv_packet select()");
	fprintf(stderr,"%d = select() \n",selret);
	return(r_ERROR);
      } //select result if
    }   //read loop
    return(r_OK);
  }
}
/*
int CntlOut::send_packet(Packet *pack) {
    struct sockaddr_in peer_sin;
    socklen_t size = sizeof peer_sin;
    struct hostent *peer_host;
    getpeername(sockDesc,(struct sockaddr*)&peer_sin,&size);
    peer_host=gethostbyaddr((char*)&peer_sin.sin_addr,sizeof peer_sin.sin_addr,AF_INET);
    printf("ctlout------ sock=%d port=%d host=%s(%s) \n",sockDesc,portNo,hostName,peer_host->h_name);
    return NetSend::send_packet(pack);
  }

int DataOut::send_packet(SPacket *pack) {
    struct sockaddr_in peer_sin;
    socklen_t size = sizeof peer_sin;
    struct hostent *peer_host;
    getpeername(sockDesc,(struct sockaddr*)&peer_sin,&size);
    peer_host=gethostbyaddr((char*)&peer_sin.sin_addr,sizeof peer_sin.sin_addr,AF_INET);
    printf("dataout-S------------- sock=%d port=%d host=%s(%s) \n",sockDesc,portNo,hostName,peer_host->h_name);
    return NetSend::send_packet(pack);
  }
*/
