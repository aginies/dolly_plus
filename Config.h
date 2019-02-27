#ifndef Config_h
#define Config_h 1
#include <stdio.h>
#include "List.h"

class Config {
 public:
  Config();
  ~Config();
  int open(char *name);
  int parse(List *hostlist, FileList *filelist);
  char *get_servername() {return serverName;}
  int get_servername_len() {return serverNameLength;} 
 private:
  int set_servername(char *name, int length);
  int check_filename(char *filename,int length);
  FILE * stream;
  char * filename;
  char * serverName;
  int serverNameLength;
};
#endif
