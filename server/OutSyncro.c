/*
 * Сброс рабочего множества на диск
 */
#include "FilesDef"
#include "CacheDef"

         void     OutSyncro(void);

  extern void     DiskIO(fileid,void*,unsigned,unsigned,boolean);
  extern void     SyncFlush(fileid);

void OutSyncro() {
  ((Scheck*) SyncBuffer)->timer = Timer;

  MVS(SyncBuffer,sizeof(Scheck),SyncBuffer+SyncSize-sizeof(Scheck));

  DiskIO(UpdFile,SyncBuffer,SyncSize,0,TRUE);  /* выводим в файл      */
  SyncFlush(UpdFile);
}
