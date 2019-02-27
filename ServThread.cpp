//D is a keyword for detail debuging print
#define _REENTRANT
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include "common.h"
#include "List.h"
#include "net.h"
#include "Net.h"
#include "Packet.h"
#include "Disk.h"
#include "Buffer.h"
#include "StopWatch.h"

extern bool flag_v,flag_d,flag_bind;
extern bool last_stop;

extern FileList *files;
extern ListIte  *hostIte;
extern CntlIn  *cntlin;
extern CntlOut *cntlout;
extern DataIn  *datain;
extern DataOut *dataout;

long long int readDiskBytes=0;

//---------------
//*  Buffer's  *- 
//---------------
//#define NO_BANK 21 //No of buffer banks
//#define NO_BIND 10 /* Max. seqno distance from readnet() to writenet()   *

#define NO_BANK 21  //No of buffer banks
#define NO_BIND 10 /* Max. seqno distance from readnet() to writenet()   *
		   * NO_BIND must less than NO_BANK and follow the rule *
		   * NO_BANK >= 2*NO_BIND +1                            */

ServerBuffer *dataBuffer[NO_BANK];
DataPacket   *dataPacket[NO_BANK];

/*----------------------------*
 * Very ugly grobal common %-)*
 *----------------------------*/
bool writeNetXoff=false;

/*-----------------------*
 *   Exception recieve   *
 *-----------------------*/
//Name:			[ exception_reciever()  ]
//Func: manage control command.
void exception_reciever(NetRecv *net)
{
  int ret;
  CmdPacket cmdpack;
  Net::sr_retval srret;
  void (*save_function)(NetRecv*);

  if (flag_v) {
    fprintf(stderr,"!!!! get control packet. !!!!\n"); }
  save_function = net->register_exception(net,NULL);

  if (net->get_accept_sock() == (-1)) {
    if (net->nonblock_accept()!=Net::SUCCESS) {
      fprintf(stderr,"select() got access but cannot accept()."
	     " in ServThread::exception_reciever()");
      exit(1); }
  }
   
  srret=net->recv_packet(&cmdpack);
  if (srret!=Net::SR_success) /*error or timeout*/ {
    fprintf(stderr,"cannot recieve command packet from the host which "
	    "made an exceptional access.\n");    exit(1);   }

  ret=cmdpack.get_type();
  switch (ret) {
  case TYPE_XOFF:
    writeNetXoff = true;
    if(flag_v) fprintf(stderr,"XOFF exception packet \n");
    break;
  case TYPE_REPORT:
    fprintf(stderr,"CmdMess:'%s'\n",cmdpack.get_message());
    break;
  case TYPE_RECONNECT :
    fprintf(stderr,"I am server. But recieved RECONNECT request."
	    " in ServThread::exception_reciever() \n");
    exit(1);
    break;
  case TYPE_STOP :
    fprintf(stderr,"Got the last node's termination");
    break;
  default:
    fprintf(stderr,"Get unknown command packet(type=%d)\n"
                   " (see type in packet.h)\n",ret);
    last_stop = true;
    break;
  }
  net->register_exception(net,save_function);
  cntlin->close_sock();
  delete cntlin;


  cntlin = new CntlIn();
  cntlin->open();
}

/*---------------------*
 *   readdisk Thread   *
 *---------------------*/
//Name:			[  readdisk() ]
void *readdisk(void *)
{
  FileListIte *fileite = new FileListIte(files);
  FromDisk *afile;   ServerBuffer *buff;
  int bankno,prev_bankno,fileno;
  bool is_1st_bankround=true;  // the bank 1st access=0, 2nd> =1
  StopWatch wait_watch, read_watch, all_watch;
  int ret;
  int ten_mbytes,prev_ten_mbytes;
  double now_time_sum=0, prev_time_sum=0;

  all_watch.start();

  bankno=0; prev_bankno=0;
  prev_ten_mbytes=0;
  for (fileno=0;;fileno++) {
    afile = new FromDisk();
    if (!afile->open(fileite->get_inputname())) {
      fprintf(stderr,"in ServThread.cpp::readdisk()\n");
      exit(1);    }
    if (flag_v) {
      fprintf(stderr,"fileno=%d '%s' opened.\n",fileno,
	      fileite->get_inputname());    }

    int eof; eof=NOT_EOF;
    while (eof == NOT_EOF) {
      buff = dataBuffer[bankno];



      // very tricky code
      if (!bankno%YIELD_BANKNO) {
	sched_yield();
      }




      wait_watch.start(); buff->wait_wnet_done(); wait_watch.stop();
      //     if ((bankno!=prev_bankno) && flag_bind ) 
      if (flag_bind && !is_1st_bankround) {
	int check_writenet_bank;
	check_writenet_bank=(bankno-NO_BIND+NO_BANK)%NO_BANK;
	                                  //= bankno-NO_BIND
	DEBPR(printf("readdisk() waiting for bank=%d. I am in %d (seqno=%d+BANKNO(%d))\n",check_writenet_bank,bankno,dataBuffer[bankno]->get_seqno(),NO_BANK););
	/*
	  printf("readdisk() waiting for bank=%d. I am in %d (seqno=%d+BANKNO(%d))\n",check_writenet_bank,bankno,dataBuffer[bankno]->get_seqno(),NO_BANK);
	  ** I saw twice that twobind semaphore did not work. **
	  ** in this case please activate this line
	  */
	dataBuffer[check_writenet_bank]->wait_bindtwo_go();
	DEBPR(printf("wait done!\n"););
      }

      read_watch.start();
      buff->set_fileno(fileno);
      eof = afile->read_tobuff(buff);
      readDiskBytes += buff->get_contsize();
      read_watch.stop();

      if (eof == EOF_DETECT)     {
	DEBPR(printf("+++++++++ readdisk() seqno=%d bankno=%d "
	     "fileno=%d ..EOF\n", buff->get_seqno(),bankno,fileno););
	                         //<--------- buffer unlock defered
      } else if (eof == NOT_EOF)  {
	DEBPR(printf("+++++++++ readdisk() seqno=%d bankno=%d "
		   "fileno=%d \n",buff->get_seqno(),bankno,fileno););
	buff->set_rdisk_done();  //<--------- buffer unlock
      } else    /* error */       { 
	fprintf(stderr,
		"fail to read disk in ServThread.cpp::readdisk()\n");
	exit(1);
      }

      prev_bankno=bankno;
      bankno=(bankno+1)%NO_BANK; //<--------- bankno modified
      if (is_1st_bankround && bankno==0) is_1st_bankround=false;

      ten_mbytes = readDiskBytes/10000000LL/*10MB*/;
      all_watch.lap();
      if (ten_mbytes && (prev_ten_mbytes!=ten_mbytes)) {
	now_time_sum = all_watch.get_sum();
	printf("%d0MB  %.2fMB/s 10MB=%.1fMB/s (%.1fs 10MB=%.1fs)\n",
	       ten_mbytes,(double)(ten_mbytes*10)/now_time_sum,
	       10.0/(now_time_sum-prev_time_sum),
	       now_time_sum, now_time_sum-prev_time_sum);
	prev_time_sum = now_time_sum;
      }
      prev_ten_mbytes = ten_mbytes;
    } // until eof, bank loop
    
    afile->close();    delete afile;
    if(flag_v)  fprintf(stderr,"fileno=%d closed\n",fileno); 

    if (! fileite->pop_entry() ) {
      buff->set_eof(B_FINAL_EOF);
      buff->set_rdisk_done();   //<-----------defered unlock
      break;                    //<-----------finish fileloop
    } else {
      buff->set_eof(B_EOF);
      buff->set_rdisk_done();   //<-----------defered unlock
    }
  } // file loop
  all_watch.stop();
  if (flag_v) {
    fprintf(stderr,"TotalTime=%.2f(sec) DiskRead=%.2f(sec) "
	 "SemaphoreWait=%.2f(sec)\n",all_watch.get_sum(),
	    read_watch.get_sum(),  wait_watch.get_sum());
    fprintf(stderr,"readdisk() ended.. \n");
  }
  last_stop = true;
  return NULL;
}

/*---------------------------*
 *      writenet  Thread    **
 *---------------------------*/

//Name:			 [ recover_sending() ]
//Func: recover from sending error by redirecting alternate host.
void recover_sending(int bankno)
{
  CmdPacket rec_pack,ack_pack;   DataPacket *pack;
  int ii,jj,ret;
  Net::ret_value retn;
  Net::sr_retval srret;

  if (flag_v) {
    fprintf(stderr,"Starting recovery process ...\n"); }

  dataout->close_sock();
  delete dataout;

  hostIte->set_flag(NOAVIL_HOST);
  if (!hostIte->search_flag(AVAIL_HOST)) {
    fprintf(stderr,"Cannot find the next available host. "
	    "Server::writenet() \n");
    hostIte->all_print();	exit(1);  }

  if (flag_v) {
    fprintf(stderr,"start to connect to %s...\n",hostIte->get_name());}

  cntlout = new CntlOut(hostIte->get_name());
  if (cntlout->open_connect()!=Net::SUCCESS) {
    fprintf(stderr,"the next host candidate %s is not available\n",
	    hostIte->get_name());    exit(1);  }

  if (flag_v) {
    fprintf(stderr,"Send RECONNECT command packet.. \n");  }

  rec_pack.make_command(TYPE_RECONNECT,0);
  srret = cntlout->send_packet(&rec_pack);
  if ( srret != Net::SR_success) {
    fprintf(stderr,"Cannot send REconnect packet to %s in Server::"
	    "recover_sending() (ret=%d)\n", hostIte->get_name(),srret);
    exit(1);  }

  if (flag_v) {
    fprintf(stderr,"Waiting RECONNECT ACK ...\n"); }

  int req_seqno, now_seqno, lowest_seqno, resend_count, re_bankno;
  while (1) { // waiting for RECONNECT Ack
    if (cntlout->recv_packet(&ack_pack) != Net::SR_success ) {
      fprintf(stderr,"Server::recover_sending() reconnect Ack cannot"
	      "be recieved\n");      exit(1);    }
    if (ack_pack.get_type() == TYPE_RECONNECT_ACK) {
      req_seqno=ack_pack.get_operand();
      break;                     //<--- break from RECONNECT wait
    } else {
      fprintf(stderr,"type=%d is recieved, when RECONNECT_ACK is"
	      " expected \n",ack_pack.get_type());  exit(1);
    } // wait for Ack packet loop
  }// wait for RECONNECT ACK

  if (flag_v) {
    fprintf(stderr,"RECONNECT Ack(seqno=%d) is recieved from %s.\n"
	    , req_seqno, hostIte->get_name());  }

  now_seqno    = dataBuffer[bankno]->get_seqno();
  //  lowest_seqno =(now_seqno-NO_BANK-1); //-1 is by readnet()
  lowest_seqno = (now_seqno-NO_BIND); //is guranteed by BIND mech.
  if (req_seqno<lowest_seqno) {
    fprintf(stderr,"I don't have the requested seq_no's bank data."
	    "request seqno(%d), I have (%d-%d)\n", req_seqno,
	    lowest_seqno,now_seqno);
    fprintf(stderr,"writenet() recovery was failed."
	    "Server::recover_sending() \n");
    exit(1);
  } else if (now_seqno<req_seqno) {
    fprintf(stderr,"Requested seqno(%d) is larger than my present processing "
	    "seqno(%d).\n It means your requested file chunk is the chunk"
	    " which is in the previous file. \n"
	    "For now, the program doesnot support such situation. sorry!\n",
	    req_seqno,now_seqno);
    fprintf(stderr,"writenet() recovery was failed."
	    "ClntThread::recover_sending() \n");
    exit(1);
  }

  if (flag_v) 
    fprintf(stderr,
	    "resend databuffer from seqno=%d(req) to seqno=%d(now)\n",
	    req_seqno,now_seqno);

  dataout = new DataOut(hostIte->get_name());
  sleep(1);   // wait for reciever node ready 20010806
  if (dataout->open_connect()!=Net::SUCCESS) {
    fprintf(stderr,"fail to connect to %s(%d) in connection recovery.\n",
	    dataout->get_hostname(), dataout->get_portno());
    exit(1);
  }
	    
  resend_count= (now_seqno-req_seqno) +1; //+1: include myself

  // print for verbose
  ii = bankno;
  fprintf(stderr,"now bankno=%d,seqno=%d \n",ii,
	  dataBuffer[ii]->get_seqno());
  ii = (ii-1+NO_BANK) % NO_BANK;
  fprintf(stderr,"-1 bankno=%d,seqno=%d \n",ii,
	  dataBuffer[ii]->get_seqno());
  ii = (ii-1+NO_BANK) % NO_BANK;
  fprintf(stderr,"-2 bankno=%d,seqno=%d \n",ii,
	  dataBuffer[ii]->get_seqno());
  ii = (ii-1+NO_BANK) % NO_BANK;
  fprintf(stderr,"-3 bankno=%d,seqno=%d \n",ii,
	  dataBuffer[ii]->get_seqno());
  // end printing

  re_bankno = (bankno + NO_BANK - (resend_count-1)) % NO_BANK;

  int rr_bankno;
  for (ii=resend_count,jj=0; ii>0; ii--,jj++) {
    rr_bankno = (re_bankno + jj)%NO_BANK;

    pack      = dataPacket[rr_bankno];
    if (dataBuffer[rr_bankno]->get_seqno() != (req_seqno+jj)) {
      fprintf(stderr,"Requested filechunk(seqno=%d+%d) is not matched to "
	      "buffer packet filechunk(seqno=%d,bankno=%d)\n",
	      req_seqno,jj,dataBuffer[rr_bankno]->get_seqno(),rr_bankno);
      exit(1);
    }
    pack->make(0,0);
    srret = dataout->send_packet(pack);
    if (srret==Net::SR_error) {
      fprintf(stderr,"Double send error is not recoverable(1)\n");
      exit(1);
    }else if (srret==Net::SR_connectionlost) {
      fprintf(stderr,"Double send error is not recoverable(TIMEOUT)\n");
      exit(1);
    }
    if (flag_v) 
      fprintf(stderr,"sending seqno=%d(bank=%d) is succeeded\n",
	      dataBuffer[rr_bankno]->get_seqno(),rr_bankno);
  } /* resend_count loop */
  if (flag_v)
      fprintf(stderr,"Return from recover_sending.\n");
}

/*--------------------*
 * writenet() Thread  *
 *--------------------*/
//Name:			[   writenet()   ]
void *writenet(void *)
{
  int ii,bankno;
  DataPacket *pack;  ServerBuffer *buff;
  StopWatch all_watch,write_watch,wait_watch,misc_watch;
  Net::sr_retval srret;
  FileNoPacket filenopack;

  all_watch.start();
  cntlin = new CntlIn();
  cntlin->open();
  dataout->register_exception(cntlin,exception_reciever);

  filenopack.make(0,0);

  for (bankno=0;;bankno=(bankno+1)%NO_BANK) {
    misc_watch.start();
    buff = dataBuffer[bankno];    pack = dataPacket[bankno];


    // very tricky code
    if (!bankno%YIELD_BANKNO) {
      sched_yield();
    }



    wait_watch.start(); buff->wait_rdisk_done();  wait_watch.stop();
    //printf("\twait rdisk done! \n");

    // At the first chunk of file sending, fileno packet is sent
    //////////////////////////
    // filenopacket sending //
    //////////////////////////
    if (buff->get_seqno()==0) { 
      if (flag_v) {
	fprintf(stderr,"filenopacket(fileno=%d) sending \n",
		                 buff->get_fileno()); }

      filenopack.set_fileno((short int)buff->get_fileno());
      int retry_count;
      for (retry_count=0;retry_count<MAX_XOFF_WAIT;retry_count++) {
	//printf("\tfile_packet(bankno=%d)\n",bankno);
	srret=dataout->send_packet(&filenopack);
	//printf("\tfile_packet(bankno=%d) done\n",bankno);
	if (srret==Net::SR_success)          /*send success*/     {
	  writeNetXoff=false;	// reset XOFF
	  break;
	} else if (srret==Net::SR_connectionlost)  /*timeout or EPIPE*/ {
	  if (writeNetXoff)             {
	    continue;           // retry sending
	  } else  /* not Xoff status */ {
	    if (flag_v) {
	      fprintf(stderr,"fail to send 'filenopack' to %s\n",
		      hostIte->get_name());}
	    recover_sending(bankno);
	    writeNetXoff=false;
	    break;
	  }
	} else         /* send_packet error         */  {
	  fprintf(stderr,"sned filno packet fail(fileno=%d)"
		  "in Server.cpp::writenet()\n",buff->get_fileno());
	  exit(1);
	} // send_packet error ?
      }   // send filenopack retry loop;
    }     // do I send filenopack ?

    ////////////////////////
    // datapacket sending //
    ////////////////////////
    pack->make(0,0);
    int retry_count;
    for (retry_count=0;retry_count<MAX_XOFF_WAIT;retry_count++) {
      write_watch.start();
      //D printf("send_packet seq=%d bank=%d\n",buff->get_seqno(),bankno);
      srret=dataout->send_packet(pack);
      //D printf("send_packet done !(size=%d)\n",ret);
      write_watch.stop();
      if (srret==Net::SR_success)         /*success*/                {
	writeNetXoff=false;          //<----  reset XOFF
	break;                       //<----  break retry!
      } else if (srret==Net::SR_connectionlost) /*timeout or pipe broken*/ {
	if (writeNetXoff) {
          if (flag_v) {
	    fprintf(stderr,"sending retry in writenet()\n");
          }
	  continue;                   //<----  retry !
	} else            {
	  if (flag_v) {
	    fprintf(stderr,"fail to send 'datapacket'(seqno=%d) to %s\n",
		    buff->get_seqno(),hostIte->get_name());	  }
	  recover_sending(bankno);
	  writeNetXoff=false;
	  break;
	}
      } else             /* send_packet error */          {
	fprintf(stderr,"send data packet fail in "
		"Server.cpp::writenet()\n");    exit(1); 
      }
    } // send data packet retry loop;

    int eof;
    eof = buff->get_eof();  // refer must be before semaphore set;
    
    DEBPR(printf("********write net  done bank=%d seqno=%d\n",bankno,buff->get_seqno()););
    /* I saw twice that twobind sempahore did not work.in the case,
       please activate the line.
       printf("********write net  done bank=%d seqno=%d\n",bankno,buff->get_seqno());
    */
    buff->set_wnet_done();
    if (flag_bind)  buff->set_bindtwo_go();

    if (eof == B_FINAL_EOF ){  
      DEBPR(printf("--------- final bankno=%d, seqno=%d eof=%d\n",
	  bankno,buff->get_seqno(),buff->get_eof()););
      break;
    }else{
      DEBPR(printf("...... writenet() bank=%d seqno=%d fileno=%d"
	"eof=%d\n",bankno,buff->get_seqno(),buff->get_fileno(),
         buff->get_eof()););
      misc_watch.stop();
    }
  }// bankno loop

  all_watch.stop();
  if (flag_v) {
    fprintf(stderr,"Totaltime=%.2f(sec) WriteNetTime=%.2f(sec)"
       " WaitSemaphoreTime=%.2f(sec) miscTime=%.2f(sec)\n",
       all_watch.get_sum(),write_watch.get_sum(), wait_watch.get_sum(),
       misc_watch.get_sum());
    fprintf(stderr,"writenet() ended..\n");
  }
  return NULL;
}

//****************************//
// read disk and write to net //
//****************************//
void data_transfer()
{
  int ii;
  StopWatch elapse_watch;
  double elapse_time;

  for (ii=0;ii<NO_BANK;ii++) {
    dataBuffer[ii]=new ServerBuffer();
    dataBuffer[ii]->set_wnet_done();  // initially all 'ready'
    // dataBuffer[ii]->set_bindtwo_go(); // initially 'go'
    dataPacket[ii]=new DataPacket();
    dataPacket[ii]->attach(dataBuffer[ii]);
  }

  elapse_watch.start();
  if (flag_v) 
    fprintf(stderr,"starting threads \n");

  void *readdisk(void *);
  pthread_t th_readdisk;
  if (pthread_create(&th_readdisk,NULL,readdisk,NULL)) {
    perror("Server pthread readdisk() creation failed");
    exit(2);
  }
  if (flag_v) 
    fprintf(stderr,"readdisk() thread started.\n");

  void *writenet(void *);
  pthread_t th_writenet;
  if (pthread_create(&th_writenet,NULL,writenet,NULL)) {
    perror("Server pthread writenet() creation failed");
    exit(2);
  }
  if (flag_v) 
    fprintf(stderr,"writenet() thread started. ALL are ready !\n");

  
  if (pthread_join(th_readdisk,NULL)) {
    perror("readdisk() cannot join.\n");
  }
  if (pthread_join(th_writenet,NULL)) {
    perror("writenet() cannot join.\n");
  }
  elapse_time=elapse_watch.stop();

  printf("Transfer Bytes=%lldMB. Transfer Speed=%.2fMB/s \n",
	 readDiskBytes/1000000LL,
	 ((double)(readDiskBytes/1000000LL))/elapse_time);

  while(1) {
    sleep(1);
    if ( last_stop ) break;
  }
  elapse_time=elapse_watch.stop();

  printf("Final Transfer Speed=%.2fMB/s \n",
	 ((double)(readDiskBytes/1000000LL))/elapse_time);
}

