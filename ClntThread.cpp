//D is a keyward for debug printing
#define _REENTRANT
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sched.h>
#include "net.h"
#include "Net.h"
#include "Packet.h"
#include "Disk.h"

#include "StopWatch.h"   // just for getTime()
#include "common.h"


extern bool flag_v,flag_d,flag_bind;
extern char * serverName;
extern char * myName;
extern char * nextName;
extern bool isNextServer;
extern PacketIte   *hostIte;
extern PacketIte   *fileIte;

extern DataOut *dataout;
extern DataIn *datain;
extern CntlOut *cntlout;
extern CntlIn  *cntlin;
extern CntlOut *repout;

bool writeNetXoff=false;
int  readSeqNo=0;   /* for REC_ACK. */

//********************************
//*- Data Transfer      ---------*
//********************************
//#define NO_BANK 21
//#define NO_BIND 10 /* Max. seqno distance from readnet() to writenet()   *
#define NO_BANK 21
#define NO_BIND 10 /* Max. seqno distance from readnet() to writenet()   *
		   * NO_BIND must less than NO_BANK and follow the rule *
		   * NO_BANK >= 2*NO_BIND +1                            */

ClientBuffer *dataBuffer[NO_BANK];
DataPacket   *dataPacket[NO_BANK];

// These bank no was originally local to each subroutine
// But became global to report bank status.
int readnet_bankno;
int writnet_bankno;
int writdsk_bankno;
int diff_rwnet=-1;
int diff_wnet_dsk=-1;


//**********************
//**  readnet  Thread **
//**********************
class ReconnectRequest {
};

void exception_reciever(NetRecv *net );

//Name:			[ readnet() ]
void *readnet(void *)
{
  int seqno,prev_bankno;
  bool is_1st_bankround=true;  // the bank 1st access=0, 2nd> =1
  int fileno,no_file;
  ClientBuffer *buff;   DataPacket *pack;
  FileNoPacket *filenopack;
  Net::sr_retval srret;
  extern int readnet_bankno;

  //* cntlin is newed in Client.cpp --> changed 20011128
  //* cntlin is now created writenet()
  //* if the following line is active. sometimes
  //* cntlin is not created yet.
  //  datain->register_exception(cntlin,exception_reciever);
  
  if (isNextServer) {  
    // if next is Server, writenet doesnot exit
    // So exception catch is done here.
    cntlin = new CntlIn();
    cntlin->open();
    datain->register_exception(cntlin,exception_reciever);
    fprintf(stderr,"exception is registrated in readnet() \n");
  }

  no_file = fileIte->get_noentry();
  if (flag_v) {
    fprintf(stderr,"start readnet process %d files. %s\n",
	                             no_file,getTime());  }
  filenopack = new FileNoPacket();
  
  readnet_bankno=0; fileno=0;
  while (fileno<no_file) { //fileno++ is done later

    srret=datain->recv_packet(filenopack);
    if (srret != Net::SR_success) {
      fprintf(stderr,
  "cannot recieve filenopack in Client.cpp::readnet() (%d=recv_packet()) %s\n"
	,srret,getTime());
      exit(1);}
    // data integrity check
    if (fileno != filenopack->get_fileno()) {
      fprintf(stderr,
 "fileno(%d) in 'filenopacket' is not matched to the expected fileno=%d.!! %s\n"	,filenopack->get_fileno(),                          fileno,getTime());
      exit(1);}
    if (flag_v) {
      fprintf(stderr,"file No.=%d processing...%s\n",fileno,getTime());    } 
    
    // Sometimes 'writedisk openfile' takes more than 1min.
    // I guess it happens because readnet monopolyze CPU and writedisk()
    // cannot get CPU for open the file.
    // So I put sleep() for giving a time for file opening to writedisk()
    sched_yield();

    for (seqno=0;;seqno++) {   
      readSeqNo=seqno;  /* for REC_ACK */
      buff = dataBuffer[readnet_bankno];  pack = dataPacket[readnet_bankno];
      

      // very tricky code
      if (!readnet_bankno%YIELD_BANKNO) {
	sched_yield();
      }



      buff->wait_wdisk_done();
      //      if ((readnet_bankno!=prev_bankno) && flag_bind ) {
      if (flag_bind && !is_1st_bankround && !isNextServer) {
	int check_writenet_bank;
	check_writenet_bank=(readnet_bankno-NO_BIND+NO_BANK)%NO_BANK;
	                                  //= readnet_bankno-NO_BIND
	dataBuffer[check_writenet_bank]->wait_bindtwo_go();
      }
     
      buff->set_fileno(fileno);    //2001.0514
      try {   // reconnect event capture try. (happen in recv_packet())
	DEBPR(printf("reading seqno=%d\n",seqno););
	srret = datain->recv_packet(pack);
	DEBPR(printf("done! (seqno=%d)\n",seqno););
      }	catch (ReconnectRequest) {  //reconnect event captured.
	datain->close_sock();
	delete datain;
	datain = new DataIn();
	if (datain->open_accept() != Net::SUCCESS) {
	  fprintf(stderr,"fail to accept connection in recovery process. %s\n"
		  ,getTime());	  exit(2);
	}
	if(flag_v) fprintf(stderr,"reconnected .. \n");
	srret=datain->recv_packet(pack);
      }                               //  with same fileno,seqno & bankno.
      if (srret!=Net::SR_success)  {  //0=timeout will have exception throw
	fprintf(stderr,"DataIn recieve Packet failed. "
		"datain->recv_packet(SPack) in ClntThread::readnet(). %s\n",
		getTime());	exit(1);	}
      
      if (seqno != buff->get_seqno()) {
	fprintf(stderr,
 	"buffer seq no(%d) in packet and expected seqno(%d) was not same! %s\n",
		buff->get_seqno(),seqno,getTime());
	exit(1);   } 

      //Dfprintf(stderr,"manabe recv seqno=%d..\n",seqno);

      
      if (buff->get_eof()) {
	if (flag_v) {
	  fprintf(stderr,"readnet() detect EOF packet for"
		  " fileno=%d/#_of_files=%d (fileno=0,1..)\n"
		  ,fileno,no_file);	}
	
	if ((fileno+1) == no_file) { // if the last file to recv.
	  fprintf(stderr,"readnet() detect Final EOF packet\n");
	  buff->set_eof(B_FINAL_EOF);	
	}
	buff->set_rnet_done();
	readnet_bankno=(readnet_bankno+1)%NO_BANK; //<------- bankno modified
	if (is_1st_bankround && readnet_bankno==0) is_1st_bankround=false;
	prev_bankno=readnet_bankno;
	break;                     //<--------- go to next fileno
      }else /* =not eof */{
	buff->set_rnet_done();
	readnet_bankno=(readnet_bankno+1)%NO_BANK; //<------- bankno modified
	if (is_1st_bankround && readnet_bankno==0) is_1st_bankround=false;
	prev_bankno=readnet_bankno;
      } // eof IF
    
    } // seqno loop (bankno loop)

    fileno++;                       //<--------- fieno modified
  } // file loop
  
  if (flag_v) {
    fprintf(stderr,"readnet() ended ....\n");  }
  delete filenopack;

  return NULL;
}

//**********************
//**  writenet Thread **
//**********************


//Name:   		[ exception_reciever ]
//Function: manage exceptional command packet
void exception_reciever(NetRecv *net )
{
  CmdPacket cmdpack,reqack_pack;
  int ret;
  void (*save_function)(NetRecv*);
  char message[256];
  Net::sr_retval srret;

  save_function = net->register_exception(net,NULL);

  if (flag_v) {
    fprintf(stderr,"!!!! get control packet !!!  \n"); }


  if (net->get_accept_sock() == (-1)) {
    if (net->nonblock_accept()!=Net::SUCCESS) {
      fprintf(stderr,
        "cannot accept at the port in ClntThread::exception_reciever(). %s\n"
	,getTime());
      exit(1); }
  }

  srret=net->recv_packet(&cmdpack);
  if (srret!=Net::SR_success) {
    fprintf(stderr,
 "cannot recieve any command packet from the host which made expt. access.%s\n"
     ,getTime());
    exit(1);
  }
  
  ret=cmdpack.get_type();
  switch (ret) {
  case TYPE_XOFF:
    writeNetXoff = true;
    if(flag_v) fprintf(stderr,"XOFF exception packet recv.\n");
    break;
  case TYPE_RECONNECT :
    if(flag_v) fprintf(stderr,"RECONNECT exception packet recv.\n");
    reqack_pack.make_command(TYPE_RECONNECT_ACK,readSeqNo);
    srret=net->send_packet(&reqack_pack);
    if (srret != Net::SR_success) {
      fprintf(stderr,
       "Cannot send REconnectAck to %s in ClntThread::exception_recver(). %s\n"
        ,net->get_hostname(),getTime());    exit(1);
    }
    throw ReconnectRequest();
    break;
  case TYPE_REQBANKST:
    if(flag_v) fprintf(stderr,"REPORT REQ exception packet recv.\n");
    fprintf(stderr,"Bank information; [%d-%d-%d] read-writenet(%d),"
	   "write net-disk(%d)\n",readnet_bankno,writnet_bankno,writdsk_bankno,
	   diff_rwnet,diff_wnet_dsk);
    if (diff_rwnet == -1) {
      sprintf(message,"I am ready to connect.[%d-%d-%d] (%d-%d)",
	      readnet_bankno,writnet_bankno,writdsk_bankno,diff_rwnet,
	      diff_rwnet,diff_wnet_dsk);
    }else {
      if (flag_bind) {
	sprintf(message,"BankSt: Bank(%d),BIND(%d),[%d-%d-%d],"
		"read-writenet(%d),write net-disk(%d)",	NO_BANK,NO_BIND,
		readnet_bankno,writnet_bankno,writdsk_bankno,
		diff_rwnet,diff_wnet_dsk);
      } else {
	sprintf(message,"BankSt: Bank(%d),[%d-%d-%d],"
		"read-writenet(%d),write net-disk(%d)\n",
		NO_BANK,readnet_bankno,writnet_bankno,writdsk_bankno,
		diff_rwnet,diff_wnet_dsk);
      }
    }
    reqack_pack.make_command(TYPE_ACKBANKST,message,strlen(message));
    srret=net->send_packet(&reqack_pack);
    if (srret != Net::SR_success) {
      fprintf(stderr,
 "Cannot send Ack against BankStatusRequest to %s in ClntThread::exception_recver(). %s\n"
       ,net->get_hostname(),getTime());
      exit(1);
    }
    break;
  default:
    fprintf(stderr,"Get unknown command packet(type=%d) (see packet.h). %s\n",
	    ret,getTime());
    exit(1);
    break;
  }
  net->register_exception(net,save_function);
  cntlin->close_sock();
  delete cntlin;
  cntlin = new CntlIn();
  cntlin->open();
}

// Name  : set_noav_and_find_nexthost() 
// Return: success(char *), fail(NULL)
char * set_noav_and_find_nexthost(PacketIte *hostite)
{
  int len;

  if (isNextServer) {
    fprintf(stderr,"Now the next host is the server, so there are no "
          " alternatives for the next host.\n");
    return(NULL);  }

  hostIte->set_flag(NOAVIL_HOST);
  delete nextName;
  if (hostIte->search_flag(AVAIL_HOST)!=0) {
    isNextServer=false;  // not needed. but for easy to read
    len = hostite->get_name_len();
    nextName = new char(len+1);
    strncpy(nextName,hostite->get_name(),len);
    nextName[len]='\0';
  } else {   // reached to the bottom of the host list
    isNextServer=true;
    len = strlen(serverName); // strlen is without '\0'
    nextName = new char(len+1);
    strncpy(nextName,serverName,len);
    nextName[len]='\0';
  }
  return(nextName);
}  

// Name:  [ recover_sending() ]
//   
void recover_sending(int bankno)
{
  CmdPacket rec_pack,ack_pack,rep_pack;
  DataPacket *pack;
  int ii,jj,ret;   char *next;
  char message[256];
  Net::ret_value retn;
  Net::sr_retval srret;

  fprintf(stderr,"Starting recovery process ...\n");
  if (flag_v) {
    fprintf(stderr,"Reporting the situation to the server.\n"); }

  if (!isNextServer) {
    if (repout == NULL) {
      repout = new CntlOut(serverName);
      retn = repout->open_connect();
      if (retn != Net::SUCCESS) {
	fprintf(stderr,"cannot connect to the server in reporting"
		"reconnection (ClntThread.cpp::recover_sending()). %s\n",
		getTime());	exit(1);  }
    }
    sprintf(message,"(<%s): Stop to sending to %s.",myName,nextName);
    ret = strlen(message);
    rep_pack.make_command(TYPE_REPORT,message,ret);
    if (repout->send_packet(&rep_pack) != Net::SR_success) {
      fprintf(stderr, "cannot send report packet(%s) to the server."
	      "(ClntThread.cpp::recover_sending(). %s\n",
	      message,getTime());      exit(1);  }
  }

  dataout->close_sock();
  delete dataout;

  next=set_noav_and_find_nexthost(hostIte);
  if (next==NULL) {
    fprintf(stderr,"Cannot find the next available host. "
	 "clntThread.cpp::recover_sending(). %s\n",getTime());
    hostIte->all_print(); exit(1);
  }

  if (flag_v) {
    fprintf(stderr,"start to connect %s(manabe=%s)\n",nextName,next);  }

  if (isNextServer) {
    fprintf(stderr,"Reconnecting... but the next node find to be server."
	    " Do nothing.\n");
    return;
  } else {  /* next node is not server */
    cntlout = new CntlOut(next);
    if (cntlout->open_connect()!=Net::SUCCESS) {
      fprintf(stderr,"the next host candidate %s is not available. %s\n",
	      nextName,getTime());
      exit(1);  }
  }

  if (flag_v) {
    fprintf(stderr,"Send RECONNECT command packet.. \n");  }
  rec_pack.make_command(TYPE_RECONNECT,0);
  srret=cntlout->send_packet(&rec_pack);
  if (srret!=Net::SR_success) {
    fprintf(stderr,"Cannot send REconnect packet to %s "
	    "in ClntThread.cpp::recover_sending(). %s\n",
	    nextName,getTime()); 
    exit(1);  }

  if (flag_v) {
    fprintf(stderr,"Waiting RECONNECT ACK ....\n"); }

  int req_seqno, now_seqno, lowest_seqno, resend_count, re_bankno;
  while(1) { // waiting for RECONNECT Ack
    srret=cntlout->recv_packet(&ack_pack);
    if (srret!=Net::SR_success) {
      fprintf(stderr,
     "ClntThread.cpp::recover_sending() reconnect Ack cannotbe recieved. %s\n"
        ,getTime());
      exit(1);    }
    if (ack_pack.get_type() == TYPE_RECONNECT_ACK){
      req_seqno=ack_pack.get_operand();
      break;           // <------- break from RECONNECT wait
    }else{    // other packet than RECONNECT
      fprintf(stderr,
       "type=%d is recieved, when RECONNECT_ACK is expected. %s\n"
        ,ack_pack.get_type(),getTime());
      exit(1);
    } // wait for Ack packet loop
  }// wait for RECONNECT ACK
  
  if (flag_v) {
    fprintf(stderr,"RECONNECT Ack(seqno=%d) is recieved from %s.\n"
	    ,req_seqno, nextName);  }

  now_seqno=dataBuffer[bankno]->get_seqno();
  //  lowest_seqno=(now_seqno-NO_BANK-1); //-1 is by readdisk()
  lowest_seqno=(now_seqno-NO_BIND); //is guranteed by BIND mech.
  if (req_seqno<lowest_seqno ) {
    fprintf(stderr,"I don't have the requested seq_no's bank "
	    "request seqno(%d), I have (%d-%d)\n", req_seqno,
	    lowest_seqno,now_seqno);
    fprintf(stderr,"writenet() recovery was failed."
	    "ClntThread::recover_sending(). %s \n",getTime());
    exit(1);
  } else if (now_seqno<req_seqno) {
    fprintf(stderr,"Requested seqno(%d) is larger than my present processing "
	    "seqno(%d).\n It means your requested file chunk is the chunk"
	    " which is in the previous file. \n"
	    "For now, the program doesnot support such situation. sorry!\n",
	    req_seqno,now_seqno);
    fprintf(stderr,"writenet() recovery was failed."
	    "ClntThread::recover_sending(). %s\n",getTime());
    exit(1);
  }
  resend_count = (now_seqno-req_seqno) + 1; // +1..include myself 
  
  if (flag_v) 
    fprintf(stderr,
	    "resend data buffer from seqno=%d(req) to seqno=%d(now).\n",
	    req_seqno,now_seqno);


  dataout = new DataOut(nextName);
  sleep(1);  // wait for reciever node becomes ready 20010806
  if (dataout->open_connect()!=Net::SUCCESS) {
    fprintf(stderr,"fail to reconnect in recorver sending. %s\n",getTime());
    exit(1);
  }

  re_bankno    = (bankno+NO_BANK-(resend_count-1)) % NO_BANK;  

  int rr_bankno;
  for (ii=resend_count,jj=0;ii>0;ii--,jj++) {
    rr_bankno = (re_bankno+jj) % NO_BANK;
    pack      = dataPacket[rr_bankno]; 
    if (dataBuffer[rr_bankno]->get_seqno() != (req_seqno+jj)) {
      fprintf(stderr,
"Requested filechunk(seqno=%d+%d) is not matched to buffer packet filechunk(seqno=%d,bankno=%d). %s\n",
         req_seqno,jj, dataBuffer[re_bankno]->get_seqno(),rr_bankno, getTime());
      exit(1);
    }
    pack->make(0,0);
    srret=dataout->send_packet(pack);
    if (srret != Net::SR_success) {
      fprintf(stderr,"Double send error is not recoverable(1). %s\n",
	      getTime());
      exit(1); }
    if (flag_v) {
      fprintf(stderr,"sending seqno=%d(bank=%d) is succeeded\n",
	      dataBuffer[rr_bankno]->get_seqno(),rr_bankno); }
  } /* resend_count loop */
  if (flag_v)
    fprintf(stderr,"Return from recover_sending.\n");
  cntlout->close_sock();
  delete cntlout;
}

// Name:		  [ writenet() ]
//
void *writenet(void *)
{
  DataPacket *pack;  ClientBuffer *buff; 
  int ret;
  int thread_return_code;
  FileNoPacket filenopack;
  extern int writnet_bankno,diff_rwnet;
  Net::sr_retval srret;

  if (isNextServer) {  // if next is Server, this routine is useless.
    if(flag_v) 
      fprintf(stderr,"Next is server, writenet() exit.\n");
    thread_return_code =1;
    pthread_exit(&thread_return_code);
  } else {
    cntlin = new CntlIn();
    cntlin->open();
    dataout->register_exception(cntlin,exception_reciever);
  }

  filenopack.make(0,0);

  for (writnet_bankno=0;;writnet_bankno=(writnet_bankno+1)%NO_BANK) {
    buff=dataBuffer[writnet_bankno];    pack=dataPacket[writnet_bankno];
    //D printf("\trnet wait\n");


    //very tricky code
    if (!writnet_bankno%5) {
      sched_yield();
    }



    buff->wait_rnet_done();
    //D printf("\trnet wait done\n");
    diff_rwnet = readnet_bankno - writnet_bankno ;
    if (diff_rwnet < 0) diff_rwnet += NO_BANK;

    //Before sending the 1st chunk of file, send 'fileno' packet
    ////////////////////////////
    // file no packet sending //
    ////////////////////////////
    if (buff->get_seqno() == 0) {
      if (flag_v) {
        fprintf(stderr,"filenopacket(fileno=%d) sending.\n",
		buff->get_fileno());
      }
      filenopack.set_fileno((short int)buff->get_fileno());
      int retry_count;
      for (retry_count=0;retry_count<MAX_XOFF_WAIT;retry_count++) {
	srret=dataout->send_packet(&filenopack);
	if (srret==Net::SR_success) {                            // Success 
	  writeNetXoff=false;                   // stop Xoff (reset)          
	  break;                                //<----retry break;
	}else if (srret==Net::SR_connectionlost) { //timeout or EPIPE
          if (flag_v) {
            fprintf(stderr,"filenopack(%d) send timeout.\n",
		    buff->get_fileno());
          }
	  if (writeNetXoff) {                   //in XOFF waiting. 
	    continue;                           //<----retry (XOFF waiting)!
	  }else{                                //first recovery 
	    if (flag_v) {
	      fprintf(stderr,"Fail to send 'filenopacket' to %s\n",
		      nextName);
	    }
	    recover_sending(writnet_bankno);
	    writeNetXoff=false;                   // stop Xoff (reset)  
	    break;                                //<----retry break;
	  }
	}else{
	  fprintf(stderr,
 "send fileno packet fail. (fileno=%d) (ClntTread::writenet). %s\n"
            ,buff->get_fileno(),getTime());
	  exit(1);
	}
      }//send filenopack retry loop;
    }// send ? filenopack

    ///////////////////////////
    // datapacket sending    //
    ///////////////////////////
    int retry_count;
    for (retry_count=0;retry_count<MAX_XOFF_WAIT;retry_count++) {
      //D printf("\tsend_packet (bankno=%d)\n",writnet_bankno);
      srret=dataout->send_packet(pack);
      //D printf("\tsend_packet done\n");
      if (srret==Net::SR_success) {                 //Success
	writeNetXoff=false;        //<----- reset XOFF
	break;                     //<----- break retry!;
      } else if(srret==Net::SR_connectionlost) /*timeout or EPIPE*/{
        if (flag_v) {
          fprintf(stderr,"datapacket send timeout.\n");
        }
	if (writeNetXoff) {        //in XOFF waiting. 
	  continue;                //<----- retry!;
	} else {                   //First recovery process
	  if (flag_v) {
	    fprintf(stderr,"fail to send 'datapacket' to %s\n",
		   nextName);
	  }
	  recover_sending(writnet_bankno);
	  writeNetXoff=false;      // stop Xoff (reset)  
	  break;                   //<----retry break;
	}
      } else /* send_packet error */ {
	fprintf(stderr,
         "send data packet fail. (seqno=%d) (ClntTread::writenet). %s\n"
         ,buff->get_seqno(),getTime());
	exit(1);
      }
    } // datapacket sending retry loop 


    int eof;
    eof = buff->get_eof();

    //D printf("\tmanabe set wnet done sem\n");    
    buff->set_wnet_done();
    if (flag_bind)   buff->set_bindtwo_go();

    if (isNextServer) {  // if next becomes Server, this routine should stop.
      if(flag_v) 
	fprintf(stderr,"Next node becomes server, writenet() exit.\n");
      thread_return_code =1;
      pthread_exit(&thread_return_code);
    }
    
    if (eof == B_FINAL_EOF) {
      break;                       //<--------- break to the end!!
    }
  }// bank loop

  if (flag_v) {
    fprintf(stderr,"writnet() stop ... \n");  }

  return NULL;
}

//************************
//**  writedisk  Thread **
//************************
// Name:  writedisk
// Func:  Main routine in writedisk thread
void writedisk(void)
{
  ToDisk *afile;  ClientBuffer *buff;
  int eof;
  const char *filename;
  int flag;
  extern int writdsk_bankno,diff_wnet_dsk;

  writdsk_bankno=0;
  while (1) /* fileno loop */{
    afile = new ToDisk();
    filename = fileIte->get_name();
    flag = fileIte->get_flag();
    if (flag_v) fprintf(stderr,"open before(%s)-",getTime());    
    if (!afile->open(filename,flag)) {
      fprintf(stderr,"\nfail to open output file in "
	      "ClntThread.cpp::writedisk(). %s\n",getTime());
      exit(1);    }
    if (flag_v) fprintf(stderr,"(%s)after \n",getTime());    
    if(flag_v) {
      fprintf(stderr,"writing to '%s' \n",fileIte->get_name());  }
    while(1) /* bankno loop*/ {

      buff = dataBuffer[writdsk_bankno];


      // very tricky code
      if (!readnet_bankno%YIELD_BANKNO) {
	sched_yield();
      }



      if (isNextServer)    buff->wait_rnet_done();
      else                 buff->wait_wnet_done();

      diff_wnet_dsk = writnet_bankno - writdsk_bankno;
      if (diff_wnet_dsk < 0) diff_wnet_dsk += NO_BANK;

      if (!afile->write_frombuff(buff)) {
	fprintf(stderr,"Disk write failed. %s\n",getTime());
	exit(2);
      }
      eof=buff->get_eof();
      buff->set_wdisk_done();
      writdsk_bankno=(writdsk_bankno+1)%NO_BANK;
      if (eof) {
	if(flag_v) {
	  fprintf(stderr,"writedisk(%s) packet seq=EOF is detected.\n"
                  ,filename);
	}
	break;             //<------ break from bankno loop;
      }
    }// bank loop
    afile->close();
    if (flag_v) {
      fprintf(stderr,"%s is closed.\n",filename);
    }
    delete afile;
    if( eof == B_FINAL_EOF) {
      if(flag_v) {
	fprintf(stderr,"All files are written \n");      }
      break;               //<------ break from fileno loop;
    }
    if(!fileIte->pop_entry()) {
      fprintf(stderr,"file list becomes empty. Client::writedisk() \n"
	      "but cannot detect B_FINAL_EOF in buffer. %s\n",getTime());
      exit(1);  }
  }// file loop
  if(flag_v){
    fprintf(stderr,"writedisk() ended ... \n");
  }
}
 
//************************************************
// read from net, write to net and write to disk *
//************************************************
void data_transfer()
{
  int ii;
  ClientBuffer *buff; DataPacket *pack;
  
  for (ii=0;ii<NO_BANK;ii++) {
    buff=dataBuffer[ii]=new ClientBuffer();
    buff->set_wdisk_done(); // initial 'ready'
    //    buff->set_bindtwo_go(); // initial 'go'
    pack=dataPacket[ii]=new DataPacket();
    pack->attach(buff);
  }

  void *writenet(void *);
  void *readnet(void *);
  pthread_t th_readnet,th_writenet;

  if (pthread_create(&th_readnet,NULL,readnet,NULL)){
    perror("Client pthread readnet() creation failed");
    exit(2);
  }
  if (flag_v) 
    fprintf(stderr,"readnet() thread started \n");


  if (pthread_create(&th_writenet,NULL,writenet,NULL)){
    perror("Client pthread writenet() creation failed");
    exit(2);
  }
  if (flag_v) 
    fprintf(stderr,"writenet() thread started. ALL are ready ! \n");

  writedisk();
  
  //Final ending report to server. I am the last node.
  if (isNextServer) {
    CmdPacket rep_pack;
    Net::ret_value retn;
    char message[100];
    int ret;
    
    if (repout == NULL) {
      repout = new CntlOut(serverName);
      retn = repout->open_connect();
      if (retn != Net::SUCCESS) {
	fprintf(stderr,"cannot connect to the server for ending report. %s\n",
		getTime());	exit(1);  }
      sprintf(message,"END:(<%s): The last node finish recieving.",myName);
      ret = strlen(message);
      rep_pack.make_command(TYPE_STOP,message,ret);
      if (repout->send_packet(&rep_pack) != Net::SR_success) {
	fprintf(stderr, "cannot send end report packet(%s) to the server. %s",
		message,getTime());      exit(1);  }
    }
  }

  if (repout!=NULL) {
    repout->close_sock();
    delete repout;
  }
}
