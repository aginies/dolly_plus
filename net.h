#define REPORT   9996
#define CNTLPORT 9997
#define DATAPORT 9998

/* SENDTIMEOUT must be much smaller value than RECVTIMEOUT */
//#define SENDTIMEOUT    60		        /* sec */
#define SENDTIMEOUT    200		        /* sec */
#define MAX_XOFF_WAIT  10                       /* times */

//#define RECVTIMEOUT 10+SENDTIMEOUT		/* sec */
#define RECVTIMEOUT 100+SENDTIMEOUT		/* sec */

#define MAX_CONNECT_TRY 3
#define MAX_HOST_NAME_LEN 32

#define YIELD_BANKNO 3
