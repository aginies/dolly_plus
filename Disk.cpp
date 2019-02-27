#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "Disk.h"
#include "Buffer.h"
#include "packet.h"
#define STDIN  0
#define STDOUT 1

extern bool flag_v;
static int n_forks=0;

//Name:               [open]
//Return:  Fail(0),Success(1)       
#define CURRENT_DIR_NAME_LEN 256
int ToDisk::open(const char *path, const int flag)
{
  int ret;
  int fd;
  char current_dir[CURRENT_DIR_NAME_LEN];

  if (flag_v) {
    fprintf(stderr,"opening file pathname=%s flag=0%o\n",path,flag);
  }
  if ( flag & MASK_ARCHIVE ) /* path is dir. to chdir */ {
    if (flag_v) {
      fprintf(stderr," change working directory to %s \n",path);
    }
    if ( getcwd(current_dir,CURRENT_DIR_NAME_LEN) == NULL ){
      fprintf(stderr,"ToDisk::open getcwd failed.\n");
      perror("Reason");
    }
    if ( chdir(path) == -1 ) {
      fprintf(stderr,"ToDisk::open chdir failed.\n"
	      " I expect absolute pathnames in archive and continue \n");
      perror("chdir fail Reason");
    }
    fd = STDOUT; /* set to STDOUT as a default */
  } else /* path is a filename */ {
    fd = ::open(path,O_WRONLY|O_CREAT|O_TRUNC,0600);
    if (fd==(-1)) {
      fprintf(stderr,"ToDisk::open() Filename=%s open fail.\n",path);
      perror("Reason");
      return 0;
    }
  }
   
  if (flag==0) {
    aFD=fd;
    // seqNo=0;  not used in ToDisk::
    fStat=S_BEGIN;		// not used for the moment.
    return 1;
  } else /* need exec commands */ {
    ret = pipes_forks(fd,flag,1);
    if (ret==-1) {
      return 0;
    }
    aFD  = ret;
    fStat= S_BEGIN;
    return 1;
  }
}


//Name:               [pipes_forks]
//Return:   Fail(-1), Success(PipeOutputFileDescriptor)
int ToDisk::pipes_forks(const int fd, const int flag, const int level) {
  int ret;
  int pfd[2];
  int ztype,atype;
  int nflag;
  const char *command_name,*command_arg1,*command_arg2;

  if (! (flag &  (MASK_COMPRESS | MASK_ARCHIVE))) {
    fprintf(stderr,"flag=0%o is strange! level=%d \n",flag,level);
    return -1;
  }

  if (pipe(pfd) == -1) { //create pipe
    fprintf(stderr,"ToDisk::pipes_forks()  flag=0%o level=%d\n",
	    flag,level);
    perror("Reason");
    return -1;
  }

  n_forks++;
  ret=fork();
  if (ret < 0) { //Failure
    fprintf(stderr,"ToDisk::pipes_forks() fork() failed.(#=%d)\n",n_forks);
    perror("Reason");
    exit(0);
  } else if (ret == 0) /* child */  {
    if (::close(pfd[1]) == -1) {
      fprintf(stderr,"ToDisk::pipes_forks() close out pipe (level=%d)",level);
      perror("Reason");
      exit(-1);
    }
    ::close(STDIN);   // redirect stdin to pipe input
    if (dup2(pfd[0],STDIN) == -1) {
      fprintf(stderr,"ToDisk::open() dup2 (level=%d)",level);
      perror("Reason");
      exit(-1);
    }

    if ( ztype = (flag & MASK_COMPRESS) ) /*Compress type*/ {
      switch (ztype) {
      case FLAG_GZ:
	command_name="zcat";
	break;
      case FLAG_Z:
	command_name="zcat";
	break;
      default:
	fprintf(stderr,"flag=0%o is strange in compress type! level=%d\n"
		,flag,level);
	return -1;
	break;
      }

      if ( flag & MASK_ARCHIVE ) /*redirect STDOUT to PIPE*/ {
	nflag = flag & ~MASK_COMPRESS;
	ret = pipes_forks(fd,nflag,2);
	if (ret < 0) {
	  fprintf(stderr,"fail in ToDisk::open call pipes_forks(2)\n");
	  return -1;
	}
	::close(STDOUT);  // redirect stdout to pipe output;
	if (dup2(ret,STDOUT) == -1) {
	  fprintf(stderr,"ToDisk::pipes_forks() dup2 '|' (level=%d)",level);
	  perror("Reason");
	  exit(-1);
	}
	ret = execlp(command_name,command_name,NULL);
      } else /*redirect STDOUT to a file */ {
	::close (STDOUT);
	if (dup2(fd,STDOUT) == -1) {
	  fprintf(stderr,"ToDisk::open() dup2 '>'(level=%d)",level);
	  perror("Reason");
	  exit(-1);
	}
	ret = execlp(command_name,command_name,NULL);
      }
    } else if ( atype=(flag & MASK_ARCHIVE) ) /*Archive type*/ {
      switch (atype) {
	case FLAG_CPIO:
	  command_name = "cpio";
	  command_arg1 = "-i";
	  command_arg2 = "-dm";
	  break;
	case FLAG_TAR:
	  command_name = "tar";
	  if (flag_v) {
	    command_arg1 = "-xvf";
	  } else {
	    command_arg1 = "-xf";
	  }
	  command_arg2 = "-";
	  break;
	default:
	  break;
      }
      ret = execlp(command_name,command_name,command_arg1,command_arg2,NULL);
    }
    /* never come here. if succed to exec */
    fprintf(stderr,"ToDisk::open() exel failed (level=%d)\n",level);
    perror("Reason");
    exit(-1);
  }

  // parent proc.
  if (::close(pfd[0]) == -1) {
    fprintf(stderr,"ToDisk::open() close pipe pfd[0] (level=%d)\n",
	    level);
    perror("Reason");
    return 0;
  }
  return pfd[1];
}
      
//Name:               [open]
//Return:  Fail(0),Success(1)       
int FromDisk::open(const char *filename) 
{
  int ret;

  ret=::open(filename,O_RDONLY);
  if (ret==(-1)) {
    fprintf(stderr,"Filename = %s \n",filename);
    perror("FromDisk::open()");
    return 0;
  } else {
    aFD=ret;
    seqNo=0;
    fStat=S_BEGIN;
    return 1;
  }
}

#define MAX_DISK_RETRY 4
//Name:			[ read_tobuff() ]
//Return:  Fail(0),Success(1),EOF(2)
int FromDisk::read_tobuff(Buffer *buff)
{
  int ii,ret,r_bytes;

  int sum=0;
  buff->set_seqno(seqNo);
  seqNo++;

  buff->set_eof(B_NORMAL);
  r_bytes=buff->get_size();
  for (ii=0;ii<MAX_DISK_RETRY;ii++) {
    if ((ret=read(aFD,buff->get_addr(),r_bytes))<0) {
      perror("disk read");
      return(0);
    }
    sum += ret;
    r_bytes -= ret;
    if (ret==0) {
      fStat=S_EOF;
      buff->set_eof(B_EOF);
      buff->set_contsize(sum);
      return 2;			// EOF detected
    } else {
      fStat=S_MIDDLE;
    }
    if (r_bytes==0) {
      break;
    }    
  }
  if (r_bytes!=0) {
    fprintf(stderr,"read() repeated %d times but failed. buffsize=%d "
	    "remain=%d bytes.\n", MAX_DISK_RETRY,buff->get_size(),r_bytes);
    buff->set_contsize(sum);
    return 0;
  }
  buff->set_contsize(sum);
  return 1;
}

//Name:			[ write_frombuff() ]
//Return: Success(1), Fail(0);
int ToDisk::write_frombuff(Buffer *buff)
{
  int ii,ret,w_bytes;

  w_bytes=buff->get_contsize();
  
  for (ii=0;ii<MAX_DISK_RETRY;ii++) {
    if ((ret=write(aFD,buff->get_addr(),w_bytes)) < 0) {
      perror("disk write");
      return 0;
    }
    w_bytes -= ret;
    if (w_bytes==0) break;
  }
  if (w_bytes>0) {
    fprintf(stderr,"write() repeated %d times but failed. buffsize=%d "
	    "remain=%d bytes.\n", MAX_DISK_RETRY,buff->get_size(),w_bytes);
    return 0;
  } else {
    /*
    if ( fdatasync(aFD) < 0 ) {
      perror("fdatasync at write_fromBuffer()@Disk.cpp \n");
      exit(3);
    }
    */
    return 1;
  }
}









