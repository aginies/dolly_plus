#define MAXINLINE 256		/* chars in a line of a configuration file */
#define MAXHOSTS  1024		/* MAX number of hosts in the ring  */
#define MAXFILES  32		/* MAX number of opening file */
/* Keyword */

#define KEY_COMPRESSED  "compressed "
#define N_KEY_COMPRESSED 11
#define KEY_INFILE "infile "
#define N_KEY_INFILE 7
#define KEY_OUTFILE "outfile "
#define N_KEY_OUTFILE 8
#define KEY_CLIENTNO "clients"

#define KEY_IOFILES "iofiles "
#define N_KEY_IOFILES 8
#define IN_OUT_DELM  ">"
#define N_IN_OUT_DELM 1
#define IN_OUT_CDELM ">>"
#define N_IN_OUT_CDELM 2

#define N_KEY_SERVER 7
#define KEY_SERVER "server "

#define N_KEY_FIRSTCLIENT 12
#define KEY_FIRSTCLIENT "firstclient "

#define N_KEY_LASTCLIENT  11
#define KEY_LASTCLIENT "lastclient "

#define N_KEY_CLIENTS  8
#define KEY_CLIENTS "clients "

#define N_KEY_ENDCONFIG 9
#define KEY_ENDCONFIG "endconfig"
