namespace Server {
  Config *config;
  List *hosts;
  FileList *files;
  ListIte *hostite;
  CntlIn  *cntlin;
  CntlOut *cntlout;
  DataIn  *datain;
  DataOut *dataout;
  void data_transfer();
}

