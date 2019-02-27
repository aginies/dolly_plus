#define HEADER_MAXLEN 64	/* bytes */

/* PACKET FORMAT  */
#define MG_CHR_L  4		/* length of magic char. */

/* INFO PACKET */
#define MAX_INFO_LENGTH  64*1024  /* 64kBytes */
				/* we dont have 16bit limit anywhere */
#define RING_INF_HEADER_BYTES 8   /* MAGIC_$,bytes(2),entry(2) */
#define RING_INF_TRAIL_BYTES  4   /* MAGIC_$ */
#define MAGIC_RING_OPEN "ROPN"
#define MAGIC_RING_CLOSE "RCLS"
#define MAGIC_HOST_OPEN  "HOPN"
#define MAGIC_HOST_CLOSE "HCLS"

#define FILE_INF_HEADER_BYTES  8 /* MAGIC_$,bytes(2),#_of_file(2) */
#define FILE_INF_TRAIL_BYTES   4 
#define MAGIC_FILE_BEGN  "FBEG"
#define MAGIC_FILE_END   "FEND"
#define MAXFILEBYTES  64000

  /*File flag*/
  #define MASK_COMPRESS 03
  #define FLAG_GZ       01
  #define FLAG_Z        02
  #define MASK_ARCHIVE 014
  #define FLAG_CPIO     04
  #define FLAG_TAR     010
  #define FLAG_TMP    0100

/* File No. Packet */
#define FILENO_HEADER_BYTES   8 /* MAGIC_$,bytes(2),fileNo(2) */
#define MAGIC_FILENO    "FLNO"
#define FILENO_TRAIL_BYTES 0

/* DATA PACKET */
#define DATA_HEADER_BYTES  12  /* MAGIC_$,bytes(4),entry(4) */
#define MAGIC_DATA_SEQ   "SEQN"
#define MAGIC_DATA_END   "SEND"
#define DATA_TRAIL_BYTES  0

/* Command Packet */
#define CMD_HEADER_BYTES  8
#define CMD_TRAIL_BYTES  0
#define MAGIC_COMMAND    "CMND"
                                /* operand                  */
#define TYPE_RECONNECT     1    /* no operand               */
#define TYPE_RECONNECT_ACK 2    /* (long int)seqno          */
#define TYPE_XOFF          3    /* no operand               */
#define TYPE_XON           4    /* not implemented          */
#define TYPE_REPORT        5    /* with message(char)       */
                                /*  should be 0 terminated  */
#define TYPE_REQBANKST     6    /* request to nodes for reporting bankstatus */
#define TYPE_ACKBANKST     7    /* request to nodes for reporting bankstatus */
				/* message should be 0 terminated */
#define TYPE_STOP          8    /* The last node reports its termination.*/



