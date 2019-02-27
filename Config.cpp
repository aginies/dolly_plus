#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "Config.h"
#include "configfile.h"
#include "packet.h"
extern bool flag_v;

Config::Config() {
  stream = (FILE*)0;
}

Config::~Config() {
}

int Config::open(char * name) {
  if (strcmp(name,"-") == 0) {
    stream = stdin;
  } else {
    if((stream = fopen(name,"r"))==NULL) {
      perror("config file open");
      fprintf(stderr,"filename = %s \n",filename);
      exit(2);
    }
  }
  return 0;
}

// call open() before calling parse()
int Config::parse(List *hostlist, FileList *filelist) {
  char str[MAXINLINE+1];	/* +1 is for \0 */
  char *p0, *p1, *p2, *p3;
  int len,len1,len2,ret;
  char *infile,*outfile;
  int ii;
  int cooked = 0;
  unsigned char cook_flag=0;
  int filenr,hostnr;

  if(stream==NULL) {
    fprintf(stderr,"call open(); first before parse(); \n");
    exit(3);
  }
  /* Read the parameters... */
  /********************************************/
  /* First we want to know the input filename */
  /********************************************/
  if(fgets(str, MAXINLINE, stream) == NULL) {
    fprintf(stderr, "errno = %d\n", errno);
    perror("fgets for infile");
    exit(1);
  }
  if(strncmp(KEY_COMPRESSED, str, N_KEY_COMPRESSED) == 0) {
    cooked = 1;
  }
  p1 = str+N_KEY_COMPRESSED*cooked;
  if(strncmp(KEY_INFILE, p1, N_KEY_INFILE) == 0) {
    if(str[strlen(str)-1] == '\n') {
      str[strlen(str)-1] = '\0';    }
    p0 = strrchr(str, ' ');
    if(p0 == NULL) {
      fprintf(stderr, "Error in infile line.\n");   exit(1);     }
    p0++;
    len1=strcspn(p0," \t\n\0");
    infile = new char[len1+1];
    strncpy(infile,p0,len1);
    
    if (cooked) {
      cook_flag = check_filename(infile,len1);
    }
   /* Then we want to know the output filename */
    if(fgets(str, MAXINLINE, stream) == NULL) {
      perror("fgets for outfile");  exit(1);    }
    if(strncmp(KEY_OUTFILE, str, N_KEY_OUTFILE) != 0) {
      fprintf(stderr, "Missing 'outfile ' in config-file.\n");
      exit(1);  }
    if(str[strlen(str)-1] == '\n') {
      str[strlen(str)-1] = '\0';    }
    p0 = strchr(str, ' ');
    if(p0 == NULL) {
      fprintf(stderr, "Error in outfile line.\n");
      exit(1);     }
    outfile=p0+1;	   /*	+1 is ' ' */
    len2=strcspn(outfile," \t\n\0");
    if(flag_v) {
      printf(" infile is %s, compress flag=%d\n",infile,cook_flag);
      printf(" outfile is %s \n",outfile);
    }
    filelist->init(1);
    filelist->push(infile,len1,outfile,len2,cook_flag);
  }else if( strncmp(KEY_IOFILES,str,N_KEY_IOFILES)==0 ){
    /***************************/
    /* infile > outfile format */
    /***************************/
    filenr=atoi(str+N_KEY_IOFILES);
    if(filenr < 1 || MAXFILES <filenr) {
      fprintf(stderr," No of file=%d isnot acceptable(MAX=%d)\n",
	      filenr,MAXFILES);    exit(1);    }
    filelist->init(filenr);

    for(ii=0;ii<filenr;ii++) {
      if(fgets(str,MAXINLINE,stream) == NULL){
	fprintf(stderr,"fgets for file %d ",ii);
	perror("config.c"); exit(2);      }
      if(str[strlen(str)-1] == '\n') {
	str[strlen(str)-1]='\0';      }
      if((len=strspn(str," \t"))==0) {
	p0=str;			// p0 is inputname
      }else{
	p0=str+len;	        // p0 is inputname
      }
      if((p1=strstr(str,IN_OUT_CDELM))!=NULL) {
	cooked = 1;
	p2=p1+N_IN_OUT_CDELM;
      }else if((p1=strstr(str,IN_OUT_DELM))!=NULL){
	cooked = 0;
	p2=p1+N_IN_OUT_DELM;
      }else{
	fprintf(stderr,"syntax error: no '>' or '>>' in file lines");
	exit(1);      }
      if(p1==p0) {
	fprintf(stderr,"syntax error: doesnot have input filename\n");
	exit(1);      }
      len1=strcspn(p0," \t");
      len1=(len1<(p1-p0))? len1:(p1-p0);
      infile=p0;
      if (cooked) {
	cook_flag = check_filename(infile,len1);
	if (cook_flag == 0) {
	  infile[len1]='\0';
	  fprintf(stderr,"********************************"
		  "****************************************\n");
	  fprintf(stderr,"* WARNING: Cooked infile '%s' "
		  "doesnot end with '.gz or other extentions.'!\n", p0);
	  fprintf(stderr,"********************************"
		"******************************************\n");
	}
      }
      if((len=strspn(p2," \t"))==0) {
      }else{
	p2+=len;		// p2 is outputfilename
      }
      len2=strcspn(p2," \t");
      if(len2==0) {
	fprintf(stderr,"syntax error: doesnot have output filename\n");
	exit(1);
      }
      outfile=p2;
      filelist->push(infile,len1,outfile,len2,cook_flag);
    } /* fileinput loop */
  }else{
    fprintf(stderr, "Missing '%s' or '%s' in config-file.\n"
	    ,KEY_INFILE,KEY_IOFILES);
    exit(1);
  }
  /**************************************/
  /* Get our own hostname -- SERVERNAME */
  /**************************************/
  /*  if(gethostname(myhostname, 63) == -1) {
   *    perror("gethostname");
   *  }
  /* Get the server's name. */
  if(fgets(str, MAXINLINE, stream) == NULL) {
     perror("fgets for server");
    exit(2);
  }
  if(strncmp(KEY_SERVER, str, N_KEY_SERVER) != 0) {
    fprintf(stderr, "Missing 'server' in config-file.\n");
    exit(1);
  }
  if(str[strlen(str)-1] == '\n') {
    str[strlen(str)-1] = '\0';
  }
  p0 = strchr(str, ' ');
  if(p0 == NULL) {
    fprintf(stderr, "Error in firstclient line.\n");
    exit(1);
  }
  len1 = strspn(p0," \t");
  p0 += len1;
  len1 = strcspn(p0," \t\0\n");
  set_servername(p0,len1);
  if(flag_v) {
    fprintf(stderr,"server name is '%s' (%d) \n",p0,len1);
  }
  
  /* We need to know the FIRST host of the ring. */
  /* (Do we still need the firstclient?)         */
  if(fgets(str, MAXINLINE, stream) == NULL) {
    perror("fgets for firstclient");
    exit(1);
  }
  if(strncmp(KEY_FIRSTCLIENT, str, N_KEY_FIRSTCLIENT) != 0) {
    fprintf(stderr, "Missing 'firstclient ' in config-file.\n");
    exit(1);
  }
  if(str[strlen(str)-1] == '\n') {
    str[strlen(str)-1] = '\0';
  }
  p0 = strchr(str, ' ');
  if(p0 == NULL) {
    fprintf(stderr, "Error in firstclient line.\n");
    exit(1);
  }

  /* We need to know the LAST host of the ring. */
  if(fgets(str, MAXINLINE, stream) == NULL) {
    perror("fgets for lasthost");
    exit(2);
  }
  if(strncmp(KEY_LASTCLIENT, str,N_KEY_LASTCLIENT) != 0) {
    fprintf(stderr, "Missing 'lastclient ' in config-file.\n");
    exit(1);
  }
  if(str[strlen(str)-1] == '\n') {
    str[strlen(str)-1] = '\0';
  }
  p0 = strchr(str, ' ');
  if(p0 == NULL) {
    fprintf(stderr, "Error in lastclient line.\n");
    exit(1);
  }
  /*
    if(strcmp(myhostname, p0+1) == 0) {
    melast = 1;
    }
  */

  /* Read in all the participating hosts. */
  if(fgets(str, MAXINLINE, stream) == NULL) {
    perror("fgets for clients");
    exit(2);
  }
  if(strncmp(KEY_CLIENTS, str, N_KEY_CLIENTS) != 0) {
    fprintf(stderr, "Missing 'clients ' in config-file.\n");
    exit(1);
  }
  hostnr = atoi(str+8);
  if((hostnr < 1) || (hostnr > MAXHOSTS)) {
    fprintf(stderr, " No of hosts=%d cannot be acceptable.(MAX=%d).\n",
	    hostnr,MAXHOSTS);
    exit(1);
  }
  hostlist->init(hostnr+1);	// +1 is for server.
  /*First entry is Server */
  hostlist->push(serverName,serverNameLength,AVAIL_HOST);
  for(ii = 0; ii < hostnr; ii++) {
    if(fgets(str, MAXINLINE, stream) == NULL) {
      fprintf(stderr, "fgets for host %d  ");
      fflush(stderr);
      perror("config.c");
      exit(2);
    }
    if(str[strlen(str)-1] == '\n') {
      str[strlen(str)-1] = '\0';
    }
    p0=str;
    len1 = strspn(p0," \t");
    p1 = str+len1;
    len2 = strcspn(p1," \t\0");
    if(ii==0) {
      hostlist->push(p1,len2,CALL_HOST); //first destination candidate.
    }else{
      hostlist->push(p1,len2,NOAVIL_HOST);
    }
  }
  /* Did we reach the end? */
  if(fgets(str, MAXINLINE, stream) == NULL) {
    perror("fgets for endconfig");
    exit(2);
  }
  if(strncmp(KEY_ENDCONFIG, str, N_KEY_ENDCONFIG) != 0) {
    fprintf(stderr, "Missing 'endconfig' in config-file.\n");
    exit(1);
  }
  return 0;
}

int Config::set_servername(char * name, int length) {

  if((serverName=(char *)malloc(length+1))==NULL) {
    fprintf(stderr," memory space is running short."
	    "(set_servername()@Config.cpp) \n");
    exit(2);
  }
  strncpy(serverName,name,length);
  *(serverName+length)='\0';
  serverNameLength=length;
  return 0;
}

int Config::check_filename(char *out_filename, int len) {
      int flag;

      if (strncmp(&out_filename[len-8], ".cpio.gz", 8) == 0) {
	flag = FLAG_CPIO|FLAG_GZ;
        if (flag_v) printf("CPIO and GZ flag=0%o \n",flag);
      } else if (strncmp(&out_filename[len-7], ".cpio.Z", 7) == 0) {
	flag = FLAG_CPIO|FLAG_Z ;
        if (flag_v) printf("CPIO and Z flag=0%o \n",flag);
      } else if (strncmp(&out_filename[len-7], ".tar.gz", 7) == 0) {
	flag =  FLAG_TAR|FLAG_GZ;
        if (flag_v) printf("TAR and GZ flag=0%o \n",flag);
      } else if (strncmp(&out_filename[len-6], ".tar.Z", 6) == 0) {
	flag =  FLAG_TAR|FLAG_Z;
        if (flag_v) printf("TAR and Z flag=0%o \n",flag);
      } else if (strncmp(&out_filename[len-5], ".cpio", 5) == 0) {
	flag = FLAG_CPIO ;
        if (flag_v) printf("CPIO, flag=0%o \n",flag);
      } else if (strncmp(&out_filename[len-4], ".tar", 4) == 0) {
	flag = FLAG_TAR;
        if (flag_v) printf("TAR, flag=0%o \n",flag);
      } else if	(strncmp(&out_filename[len-3], ".gz", 3) == 0) {
	flag = FLAG_GZ;
        if (flag_v) printf("GZ, flag=0%o \n",flag);
      } else if (strncmp(&out_filename[len-2], ".Z", 2) == 0) {
	flag = FLAG_Z ;
        if (flag_v) printf("Z, flag=0%o \n",flag);
      } else {
	flag = 0;
      }
      return flag;
}
