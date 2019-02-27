#ifndef List_h
#define List_h 1
#include "Packet.h"

//for htype
#define RING 0
#define HOST 1

#define NAMELENMAX 256

struct Item {
  char * name;
  int length;
  unsigned char flag;
};

class List {
 public:
  List();
  virtual ~List(void);
  virtual int init(int num_items);
  virtual int flag_sync(Packet *pack);
  virtual int push(char* name, int length, unsigned char flag);
  virtual int packetfill(Packet *pack);
  virtual void print();
 private:

 protected:
  int pushI;			/* pointer for push */
  int noItems;			/* currently noItems == pushI */
  int maxItems;
  struct Item **itemList;
  int itemBytes;		/* (name bytes + flag bytes)xitem */
  
  //reference
  
  //friend
  friend class ListIte;
};

///////////////
// File List //
///////////////
struct AddItem {
  char * name; //store outfilename
  int length;
};

class FileList : public List {
 public:
  FileList();
  ~FileList();
  int init(int num);
  int push(char *infile, int inlen, char *outfile, int outlen, 
       unsigned char flag);
  void print();
 private:
  struct AddItem **additemList;

  friend class FileListIte;
};


///////////////
/// Iterator //
///////////////

class ListIte {
 public:
  ListIte(List *list);
  virtual ~ListIte() {}
  void rewind() { itemNo=0; }
  int pop_entry();
  int search_flag(unsigned char flag);
  const char * get_name() const {
    return (listObj->itemList[itemNo])->name ; }
  int get_name_len() const {
    return (listObj->itemList[itemNo])->length ; }
  unsigned char get_flag() const{
    return (listObj->itemList[itemNo])->flag ; }
  int set_flag(unsigned char ffff) {
    (listObj->itemList[itemNo])->flag = ffff ; }
  void all_print() { listObj->print(); }
 private:
  List *listObj;
  int itemNo;
};

class FileListIte {
 public:
  FileListIte(FileList *filelist);
  virtual ~FileListIte() {}
  void rewind() { itemNo=0; }
  int pop_entry();
  const char * get_outputname() const {
    return (filelistObj->itemList[itemNo])->name ; }
  const char * get_inputname() const {
    return (filelistObj->additemList[itemNo])->name ; }
  int get_outputname_len() const {
    return (filelistObj->itemList[itemNo])->length ; }
  int get_inputname_len() const {
    return (filelistObj->additemList[itemNo])->length ; }
  unsigned char get_flag() const{
    return (filelistObj->itemList[itemNo])->flag ; }
 private:
  FileList *filelistObj;
  int itemNo;
};
#endif










