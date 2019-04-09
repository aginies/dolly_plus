//D is a mark for debuging
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include "Packet.h"
#include "common.h"
#include "StopWatch.h"


Packet::Packet()
  : packetBytes(0), contBytes(0), contP(NULL),
    packetBuff(NULL), headerP(NULL)
{
}

Packet::~Packet()
{
  if (packetBuff!=NULL) {
    free(packetBuff);
  }
}

//
// making a space for a packet                      
// return a pointer pointing at the top of the packet !!!

void Packet::print()
{
  PacketIte *ite = new PacketIte(this);
  int ii;

  fprintf(stderr,"---Packet contents printing  ---------------------\n");
  fprintf(stderr,"No of Bytes = %d \n", contBytes);
  fprintf(stderr,"No.    flag   name \n");
  for (ii=0;;ii++) {
    fprintf(stderr,"%3d    %d      '%s'\n",
                     ii, ite->get_flag(), ite->get_name());
    if (!ite->pop_entry()) {
      break;
    }
  }
  fprintf(stderr,"--------------------------------------------------\n");
}
    
/////////////////
// RingPacket  //
/////////////////
RingPacket::RingPacket()
{
  headerBytes=RING_INF_HEADER_BYTES;
  trailBytes=RING_INF_TRAIL_BYTES;
  headerP=aHeader;
}

//
// making a space for a RING packet                      
// return a pointer pointing at the top of the contents !!!
//                                             ^^^^^^^^
char * RingPacket::make(short int cont_bytes,short int num_entry)
{
  char *pp,*epp;
  int nbytes,nnum;

  pp  = Packet::core_make(cont_bytes,num_entry);
  epp = endP;
  strncpy(pp,MAGIC_RING_OPEN,MG_CHR_L);
  pp += MG_CHR_L;
  nbytes = htons(cont_bytes);
  nnum = htons(num_entry);
  *(short int*)pp=nbytes;
  pp +=2 ;
  *(short int*)pp=nnum;
  strncpy(epp,MAGIC_RING_CLOSE,MG_CHR_L);
  return contP;
}

//
// Check Packet header and make space for packet recieving
// Return address for the contents
//
char * RingPacket::checkheader_make()
{
  char * pp=aHeader;
  int num_entry,cont_bytes;

  if (strncmp((char *)pp,MAGIC_RING_OPEN,MG_CHR_L) != 0) {
    pp[MG_CHR_L]=0;
    fprintf(stderr,"header type mismatch('%s' != %s )\n",
                     	                 pp,MAGIC_RING_OPEN);      
    return NULL;
  }
  pp += MG_CHR_L;
  cont_bytes = ntohs(*(unsigned short int*)(pp)); 
  pp += 2;
  num_entry = ntohs(*(unsigned short int*)(pp));
  pp =  make(cont_bytes,num_entry);  
  memcpy(packetBuff,aHeader,headerBytes);
  return pp;
}

//
// Check trailer keyword 
// Return 1... OK  0... fail
int RingPacket::checktrailer()
{

  if (strncmp(endP,MAGIC_RING_CLOSE,MG_CHR_L) != 0) {
    *(endP+MG_CHR_L-1)='\0';	// print safety
    fprintf(stderr,"trailer is wrong. ('%s' != '%s')\n",
	 			endP, MAGIC_RING_CLOSE);
    return 0;
  }
  return 1;
}


/////////////////
// HostPacket  //
/////////////////
HostPacket::HostPacket()
{
  headerBytes=RING_INF_HEADER_BYTES;
  trailBytes=RING_INF_TRAIL_BYTES;
  headerP=aHeader;
}

char * HostPacket::make(short int cont_bytes,short int num_entry)
{
  char *pp,*epp;
  int nbytes,nnum;

  pp  = Packet::core_make(cont_bytes,num_entry);
  epp = endP;
  strncpy(pp,MAGIC_HOST_OPEN,MG_CHR_L);
  pp += MG_CHR_L;
  nbytes = htons(cont_bytes);
  nnum = htons(num_entry);
  *(short int*)pp=nbytes;
  pp +=2 ;
  *(short int*)pp=nnum;
  strncpy(epp,MAGIC_HOST_CLOSE,MG_CHR_L);
  
  return contP;
}

//
// Check Packet header and make space for packet recieving
// Return address for the contents
//
char * HostPacket::checkheader_make()
{
  char * pp=aHeader;
  int num_entry,cont_bytes;

  if (strncmp(pp,MAGIC_HOST_OPEN,MG_CHR_L) != 0) {
    pp[MG_CHR_L]=0;
    fprintf(stderr,"header type missmatch('%s' != %s )\n",
	    pp,MAGIC_HOST_OPEN);
    return NULL;
  }
  pp += MG_CHR_L;
  cont_bytes = ntohs(*(unsigned short int*)(pp)); 
  pp += 2;
  num_entry = ntohs(*(unsigned short int*)(pp));
  pp =  make(cont_bytes,num_entry);  
  memcpy(packetBuff,aHeader,headerBytes);
  return pp;
}

//
// Check trailer keyword 
// Return 1... OK  0... fail
int HostPacket::checktrailer()
{
  if (strncmp(endP,MAGIC_HOST_CLOSE,MG_CHR_L) != 0) {
    *(endP+MG_CHR_L-1)='\0';	// print safety
    fprintf(stderr,"trailer is wrong. ('%s' != '%s')\n",
	 endP, MAGIC_HOST_CLOSE);
    return 0;
  }
  return 1;
}

/////////////////
// FilePacket  //
/////////////////
FilePacket::FilePacket()
{
  headerBytes=FILE_INF_HEADER_BYTES;
  trailBytes=FILE_INF_TRAIL_BYTES;
  headerP=aHeader;
}

//--------------------------//
// Make header for sending  //
//--------------------------//
char * FilePacket::make(short int cont_bytes,short int num_entry)
{
  char *pp,*epp;
  int nbytes,nnum;

  pp  = Packet::core_make(cont_bytes,num_entry);
  epp = endP;
  strncpy(pp,MAGIC_FILE_BEGN,MG_CHR_L);
  pp += MG_CHR_L;
  nbytes = htons(cont_bytes);
  nnum = htons(num_entry);
  *(short int*)pp=nbytes;
  pp +=2 ;
  *(short int*)pp=nnum;
  strncpy(epp,MAGIC_FILE_END,MG_CHR_L);
  
  return contP;
}



//---------------------------//
// Make header for recieving //
//---------------------------//
//
// Check Packet header and make space for packet recieving
// Return address for the contents
//
char * FilePacket::checkheader_make()
{
  int num_entry,cont_bytes;
  char * pp=aHeader;
  if (strncmp(pp,MAGIC_FILE_BEGN,MG_CHR_L) != 0) {
    pp[MG_CHR_L]=0;
    fprintf(stderr,"header type missmatch('%s' != %s )\n",
	    pp,MAGIC_FILE_BEGN);
    return NULL;
  }
  pp += MG_CHR_L;
  cont_bytes = ntohs(*(unsigned short int*)(pp)); 
  pp += 2;
  num_entry = ntohs(*(unsigned short int*)(pp));
  pp =  make(cont_bytes,num_entry);  
  memcpy(packetBuff,aHeader,headerBytes);
  return pp;
}  

//
// Check trailer keyword 
// Return 1... OK  0... fail
int FilePacket::checktrailer()
{

  if (strncmp(endP,MAGIC_FILE_END,MG_CHR_L) != 0) {
    *(endP+MG_CHR_L-1)='\0';	// print safety
    fprintf(stderr,"trailer is wrong. ('%s' != '%s')\n",
	 endP, MAGIC_FILE_END);
    return 0;
  }
  return 1;
}

//---------------//
// FileNo Packet //
//---------------//
FileNoPacket::FileNoPacket()
{
  headerBytes=FILENO_HEADER_BYTES;
  trailBytes=FILENO_TRAIL_BYTES;
  headerP=aHeader;
}

// For sending 
char * FileNoPacket::make(short int dummy1, short int fileno)
{
  char *pp  = Packet::core_make(0,0);
  strncpy(pp,MAGIC_FILENO,MG_CHR_L);
  pp+= MG_CHR_L;
  *(short int*)pp = 0;
  pp+= 2;
  fileNo=fileno;
  *(short int*)pp = htons(fileNo);
  return contP;
}

char * FileNoPacket::checkheader_make()
{
  int cont_bytes,type;
  char * pp=aHeader;

  //D fprintf(stderr,"manabe filenopack header check address =%x \n",pp);

  if (strncmp(pp,MAGIC_FILENO,MG_CHR_L) != 0) {
    pp[MG_CHR_L]=0;
    fprintf(stderr,"header type missmatch('%s' != %s )\n",
	    pp,MAGIC_FILENO);
    return NULL;
  }
  pp += MG_CHR_L;
  contBytes = ntohs(*(unsigned short int*)(pp)); 
  pp += 2;
  fileNo = ntohs(*(unsigned short int*)(pp));
  DEBPR(printf("========= fileno packet (fileNo=%d) \n",fileNo););
  packetBytes = contBytes+headerBytes;
  
  return aHeader;
}  

int FileNoPacket::set_fileno(short int fileno)
{
  
  if (packetBuff==NULL) {
    fprintf(stderr,
 "coding error! FileNoPacket::make() first.(in FileNoPacket::set_fileno(%d) \n"
     ,fileno);
    exit(3);
  }
  *(unsigned short int*)(packetBuff+MG_CHR_L+2) = htons(fileno);
  fileNo=fileno;
  return fileno;
}


//------------//
// CMD Packet //
//------------//
CmdPacket::CmdPacket()
  :pType(numEntry)
{
  headerBytes=CMD_HEADER_BYTES;
  trailBytes=CMD_TRAIL_BYTES;
  headerP=aHeader;
}

char * CmdPacket::make(short int bytes, short int type)
{
  char *pp = Packet::core_make(bytes, type);
  // pType=type; aliased
  strncpy(pp,MAGIC_COMMAND,MG_CHR_L);
  pp+= MG_CHR_L;
  *(short int*)pp = htons(bytes);
  pp+= 2;
  *(short int*)pp = htons(type);
  return contP;
}

char * CmdPacket::make_command(int type, long int operand)
{
  make(4,(short int)type);
  *(int *)contP = htonl(operand);
  return contP;
}

char * CmdPacket::make_command(int type, char* message, int size)
{
  if (size > MAX_MESSAGE_SIZE) {
    fprintf(stderr,
     "message size is too big. (max=%d) in CmdPacket:make_command() %s\n",
                                    MAX_MESSAGE_SIZE,getTime());
    exit(1);
  }
  make(size,(short int)type);
  memcpy(contP,message,size);
  return contP;
}
  
char * CmdPacket::checkheader_make()
{
  int cont_bytes,type;
  char * pp=aHeader;
  if (strncmp(pp,MAGIC_COMMAND,MG_CHR_L) != 0) {
    pp[MG_CHR_L]=0;
    fprintf(stderr,"header type missmatch('%s' != %s )\n",
	    pp,MAGIC_COMMAND);
    return NULL;
  }
  pp += MG_CHR_L;
  cont_bytes = ntohs(*(unsigned short int*)(pp)); 
  pp += 2;
  type = ntohs(*(unsigned short int*)(pp));
  if (cont_bytes > MAX_MESSAGE_SIZE) {
    fprintf(stderr,
   "exceed message size limit(%d>%d) detect in CmdPacket::checkheader_make()\n"
	                ,cont_bytes,MAX_MESSAGE_SIZE);
    return NULL;
  }
  pp =  make(cont_bytes,type);  
  memcpy(packetBuff,aHeader,headerBytes);
  return pp;
}  

long int CmdPacket::get_operand()
{
  int ret;
  if (contP == NULL) {
    fprintf(stderr,
    "can't recieve cmdpacket before CmdPacket::get_operand() %s.\n",getTime());     exit(3);
  } else if (contBytes != 4) {
    fprintf(stderr,
     "operand in CmdPacket may not be a long integer (sz=%dBytes)."
     " detect in CmdPacket::get_operand() %s\n",
	    contBytes,getTime());
    return 0;
  }
  
  ret = ntohl(*(int*)contP);
  return ret;
}

char * CmdPacket::get_message()
{
  if (contP == NULL) {
    fprintf(stderr,
     "can't recieve cmdpacket before CmdPacket::get_operand(). %s",getTime());     exit(3);
  }
  *(char *)(contP+contBytes) = '\0';  /* 0 terminate for safety */
  return contP;
}

///////////////
// Iterator  //
///////////////
PacketIte::PacketIte(Packet *pack)
{
  packObj=pack;
  packP=packObj->contP;
} 

char * PacketIte::get_name()
{
  int ret;
  ret=name_len_check();
  return( packP+FLAG_LEN );  //+1 is flag
}

//
// get the next entry and proceed pointer
// Return 1..Success 0..End of Entry
int PacketIte::pop_entry()
{
  int ret;
  char * new_p;
  
  ret = name_len_check();
  new_p = packP + (ret+FLAG_LEN);  //+1 is for flag
  if (new_p==packObj->endP) {
    return 0;
  } else if (new_p < packObj->endP) {
    packP = new_p;
    return 1;			// success
  } else {
    fprintf(stderr,"something wrong in the packet (0x%x != 0x%x) %s\n",
	    packP,packObj->endP,getTime());
    exit(1);
  }
}

//
// search a first encounter entry which has the flag
// Return 1..Success (revised packP) 0..Cannot find (packP preserved)
int PacketIte::search_flag(unsigned char ffff)
{
  char *save_p;
  
  while (ffff != get_flag()) {
    if (!pop_entry()) {
      return 0;
    }
  }
  return 0;
}

//
// search an entry which has the name
// Return 1..Success (revised packP) 0..Cannot find (packP preserved)
int PacketIte::search_name(char * name)
{
  char *save_p;
  int len,ii;
  
  if (packObj->numEntry > MAX_PACKET_ENTRY_NO) {
    fprintf(stderr,"No of entry in the packet=%d exceed its limit-max.%d\n",
	    packObj->numEntry,MAX_PACKET_ENTRY_NO);
    return(0);
  }
  for (ii=0;ii<packObj->numEntry;ii++) {
    len = get_name_len();
    if (strncmp(get_name(),name,len) == 0) {
      return 1;
    }
    if (!pop_entry()) {
      return 0;
    }
  }
  fprintf(stderr," PacketIte::pop_entry() cannot detect packet end.\n"
	  "packetIte::search_name(%s) \n",name);
  return 0;
}

//
// retrun length of the name with '\0'
int PacketIte::name_len_check()
{
  char *pp,*sp; int ii;

  sp=pp=packP+FLAG_LEN;	// 
  for (ii=0;ii< MAX_PACKET_ENTRY_LEN ;ii++,pp++) {
    if (*pp=='\0') {
      break;
    }
  }
  if (ii==MAX_PACKET_ENTRY_LEN) {
    *(packP+10)='\0';		// print safety
    fprintf(stderr,"entry is too long. (first 10char is '%s') %s.\n",
	    sp,getTime());
    exit(1);
  }
  return ii+1; //contains the last \0
}


//////////////////////////////////////////////////////////
// SPacket           
// o Packet have continueus memory space which contains
//   header, contents and trailer.
// o SPacket has separated memory spaces for its header
//   and contents(class Buffer).
/////////////////////////////////////////////////////////
int SPacket::attach(Buffer *buff)
{
  pBuff = buff;
  contP = buff->get_addr();
  contBytes = buff->get_contsize();
  return(1); 
}


////////////////
// DataPacket //
////////////////
DataPacket::DataPacket()
{
  headerBytes=DATA_HEADER_BYTES;
  trailBytes=DATA_TRAIL_BYTES;
  headerP=aHeader; pBuff=NULL;
}

//
// make pakcket for Recieving 
//
char *DataPacket::checkheader_make()
{
  // in this case, we dont make anything...
  long int cont_bytes,num_entry;

  char * pp=aHeader;

  //D fprintf(stderr,"datapacket header addr =%x (manabe)\n",pp);


  if (strncmp((char *)pp,MAGIC_DATA_SEQ,MG_CHR_L) == 0) {
    pBuff->set_eof(B_NORMAL);
  } else if (strncmp((char *)pp,MAGIC_DATA_END,MG_CHR_L) == 0) {
    pBuff->set_eof(B_EOF);
  } else {
    pp[MG_CHR_L]=0;
    fprintf(stderr,"header type has trouble ('%s' != DATA_HEADER )\n",pp);
    return NULL;
  }
  pp += MG_CHR_L;
  contBytes = ntohl(*(unsigned long int*)(pp)); 
  pp += 4;
  numEntry = ntohl(*(unsigned long int*)(pp));
  pBuff->set_contsize(contBytes);
  pBuff->set_seqno(numEntry);
  packetBuff = 0;
  contP = pBuff->get_addr();
  endP  = contP+contBytes;
  packetBytes = contBytes+headerBytes;

  return pp;
}

//
// make packet (just packet header) for sending
//
char * DataPacket::make(short int dummy1, short int dummy2)
{
  // All arguments are dummy.
  int seqno;

  if (pBuff==NULL) {
    fprintf(stderr,
     "Do not attach buffer yet. SPacket::make_header(): coding error.\n");
    exit(3);
  }
  char *pp=headerP;
  seqno = pBuff->get_seqno();
  if (pBuff->get_eof()){
    strncpy(pp,MAGIC_DATA_END,MG_CHR_L);
  } else {
    strncpy(pp,MAGIC_DATA_SEQ,MG_CHR_L);
  }

  contBytes = pBuff->get_contsize();
  pp += MG_CHR_L;
  *(long int*)pp = htonl(contBytes);
  DEBPR(printf("contsize--%d(buff=%d) \n",
	       contBytes,pBuff->get_contsize()););
  pp += 4;
  *(long int*)pp = htonl(seqno);
  
  packetBytes=headerBytes+contBytes;
  numEntry=0;
  packetBuff=0;
  contP = pBuff->get_addr();
  endP  = contP + pBuff->get_size();
  return(contP);    // is it true?
}


///////////////////////////////////////////////////////////
//                          Inlines                      //
///////////////////////////////////////////////////////////
inline
char * Packet::core_make(short int cont_bytes, short int num_entry)
{
  if (packetBuff != NULL) {
    free(packetBuff);
    fprintf(stderr,
     "!!!!!!!!!!!!! core_make is called but already packetBuff has value."
     " It could be a coding mistake !!!!!!!!\n");
  }
  packetBytes = cont_bytes+headerBytes+trailBytes;
  contBytes   = cont_bytes;
  numEntry    = num_entry;
  if ((packetBuff=(char*)malloc(packetBytes)) == NULL) {
    perror("Packet::make() malloc()");
    fprintf(stderr,"%s\n",getTime());
    exit(2);
  }
  contP       = packetBuff+headerBytes;
  endP        = contP+cont_bytes;
  return packetBuff;
}

