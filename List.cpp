#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "List.h"

///////////
// List 
///////////
List::List()
{
  itemList = NULL;
}

List::~List(void)
{
  int ii;
  for (ii=0;ii<noItems;ii++) {
    free(itemList[ii]->name);
    free(itemList[ii]);
  }
}

int List::init(int num)
{
  int ii,len;
  maxItems = num;
  noItems=0;
  len=sizeof(Item)*num;
  if ((itemList=(struct Item**)malloc(len)) == NULL) {
    perror("malloc in List::init()");
    fprintf(stderr,"requested bytes is %d bytes \n",len);
    exit(2);
  }
  bzero(itemList,len);
  pushI=0;
  return 0;
}

///
/// Push()
///
int List::push(char *name, int length, unsigned char flag)
{
  char * cp;
  struct Item *ip;
  
  if (maxItems<pushI) {
    fprintf(stderr," push too much (%d > no_item(%d)) \n",pushI,noItems);
    exit(1);
  }
  if (length>NAMELENMAX || length == 0) {
    fprintf(stderr,"List::push() name='%s':name too long or 0\n",  name);
    exit(1);
  }
  ip=new struct Item;
  if ((cp=(char *)malloc(length+1)) == NULL){ // +1 for '\0'
    perror("List::push()/ malloc()");
    exit(2);
  }
  strncpy(cp,name,length);
  *(cp+length) = '\0';
  itemList[pushI] = ip;
  itemList[pushI]->name = cp;
  itemList[pushI]->length = length;
  itemList[pushI]->flag = flag;
  pushI++;
  itemBytes += length+1+
    (sizeof itemList[pushI]->flag);	// +1 for \0
  noItems++;
  return 0;
}

///
/// Packetfill()
int List::packetfill(Packet *pack)
{
  char *pp, *top;
  int ii;
  
  top=pp=pack->make(itemBytes,noItems);
  for (ii=0;ii<noItems;ii++) {
    if ((top-pp)>itemBytes) {
      fprintf(stderr,
	     "item space exceeds pre-counted space(%d) (code error) \n");
      exit(3);    }

    *(unsigned char*)pp++=itemList[ii]->flag;
    strncpy(pp,itemList[ii]->name,itemList[ii]->length+1); 
    pp+=itemList[ii]->length+1;  // +1 for \0
  }
  return 0;
}

int List::flag_sync(Packet *pack)
{
  int ii,ret;

  PacketIte * pite=new PacketIte(pack);
  for(ii=0;ii<noItems;ii++) {
    ret=strncmp(itemList[ii]->name,pite->get_name(),
		itemList[ii]->length);
    if (ret!=0) {
      return 0;			
    }
    itemList[ii]->flag=pite->get_flag();
    pite->pop_entry();
  }
  return 1;
}

void List::print()
{
  int ii;

  printf("-------------List---------------------\n");
  printf("Items = %d \n",noItems);
  printf("No.    flag   name \n");
  for (ii=0;ii<noItems;ii++) {
    printf("%3d    %d      %s \n", ii+1, itemList[ii]->flag,
	   itemList[ii]->name);
  }
  printf("--------------------------------------\n");
}


//////////////
// FileList //
//////////////

FileList::FileList() {}

FileList::~FileList()
{
  int ii;
  for (ii=0;ii<noItems;ii++) {
    free(additemList[ii]->name);
    free(additemList[ii] );
  }
}

int FileList::init(int num)
{
  int ii,len;

  List::init(num);
  len=sizeof(AddItem)*num;
  if ((additemList=(struct AddItem **)malloc(len))==NULL) {
    perror("malloc in FileList::init()");
    fprintf(stderr,"requested bytes is %d bytes \n",len);
    exit(2);
  }
  bzero(additemList,len);
  return 0;
}

int FileList::push(char *infile, int inlen, char *outfile, int outlen,
	       unsigned char flag)
{
  char * cp;
  struct AddItem *aip;
  
  // Outfile to List, InputFile to addItem
  List::push(outfile,outlen,flag);
  pushI--;			// play back !!
  if (inlen>NAMELENMAX || inlen == 0) {
    fprintf(stderr,"FileList::push() name='%s':name too long or 0 (l=%d)\n",  
	    infile,inlen);   exit(1);  }
  aip = new struct AddItem;
  if ((cp=(char *)malloc(inlen+1)) == NULL){ // +1 for '\0'
    perror("FileList::push()/ malloc()");
    exit(2);
  }
  strncpy(cp,infile,inlen);
  *(cp+inlen)='\0';
  additemList[pushI] = aip;
  additemList[pushI]->name = cp;
  additemList[pushI]->length = inlen;
  pushI++;
  return 0;
}

void FileList::print()
{
  int ii;

  printf("-------------List-------------------------------\n");
  printf("Items = %d \n",noItems);
  printf("No.    flag   name                 name \n");
  for (ii=0;ii<noItems;ii++) {
    printf("%3d    %d      %-20s %-20s\n", ii+1, itemList[ii]->flag,
	   itemList[ii]->name, additemList[ii]->name);
  }
  printf("------------------------------------------------\n");
}

//////////////
// Iterator //
//////////////

ListIte::ListIte(List *list)
{
  listObj=list;
  itemNo=0;
}

int ListIte::pop_entry() 
{
  int itemno = itemNo+1;
  if (itemno >= listObj->noItems) {
    return 0;
  } else {
    itemNo=itemno;
    return 1;
  }
}

//
// Return: Success(1; itemNo updated), Fail(0)
int ListIte::search_flag(unsigned char flag)
{
  int ii;
  for (ii=(itemNo+1);ii<listObj->noItems;ii++) {
    if ((listObj->itemList[ii])->flag == flag) {
      break;
    }
  }
  if (ii==listObj->noItems) {
    return 0;
  } else {
    itemNo = ii;
    return 1;
  }
}

FileListIte::FileListIte(FileList *list)
{
  filelistObj=list;
  itemNo=0;
}

int FileListIte::pop_entry()
{
  int itemno=itemNo+1;
  if (itemno >= filelistObj->noItems) {
    return 0;
  } else {
    itemNo=itemno;
    return 1;
  }
}




