#include "buffer.h"
#include "Buffer.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

/////////////
/// Buffer //
/////////////

Buffer::Buffer()
  : buffBytes(BUFFSIZE),seqNo(0),eoF(B_NORMAL) {
  
  buff_addr = (char *)malloc(BUFFSIZE+2*PAGESIZE);
  if( buff_addr == NULL) {
    perror("Buffer::Buffer() malloc");
    exit(2);
  }
  buffAddr=(char*)((long)(buff_addr+PAGESIZE) & (~(PAGESIZE-1)));
  contBytes=0;
}

Buffer::~Buffer() {
  free(buff_addr);
}

// Return  0..fail       1..Success
int Buffer::set_contsize(int size) {
  if( size > buffBytes)  {
    fprintf(stderr,"recieved data exceed buffer size (%d > %d)"
	    "Buffer::set_contsize()\n", size,buffBytes);
    return 0;
  }
  contBytes=size;
  return 1;
}

//////////////////
// ServerBuffer //
//////////////////

ServerBuffer::ServerBuffer() {
  if( sem_init(&semWriteNet,0,0) ) {
    perror("semWriteNet init fail sem_init()");
    exit(2);
  }
  if( sem_init(&semReadDisk,0,0) ) {
    perror("semReadDisk init fail sem_init()");
    exit(2);
  }
  if( sem_init(&semBindTwo,0,0) ) {
    perror("semBindTwo init fail sem_init()");
    exit(2);
  }
}

ServerBuffer::~ServerBuffer() {
  sem_destroy(&semWriteNet);
  sem_destroy(&semReadDisk);
  sem_destroy(&semBindTwo);
}

//////////////////
// ClientBuffer //
//////////////////

ClientBuffer::ClientBuffer() {
  if( sem_init(&semWriteNet,0,0) ) {
    perror("semWriteNet init fail sem_init()");
    exit(2);
  }
  if( sem_init(&semReadNet,0,0) ) {
    perror("semReadNet init fail sem_init()");
    exit(2);
  }
  if( sem_init(&semWriteDisk,0,0) ) {
    perror("semWriteDisk init fail sem_init()");
    exit(2);
  }
  if( sem_init(&semBindTwo,0,0) ) {
    perror("semBindTwo init fail sem_init()");
    exit(2);
  }
}

ClientBuffer::~ClientBuffer() {
  sem_destroy(&semWriteNet);
  sem_destroy(&semReadNet);
  sem_destroy(&semWriteDisk);
  sem_destroy(&semBindTwo);
}
