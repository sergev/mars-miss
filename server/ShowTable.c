                   /* ============================ *\
                   ** ==         M.A.R.S.       == **
                   ** == Отображение таблиц на  == **
                   ** == на "консоли оператора" == **
                   \* ============================ */

#include "FilesDef"
#include "CacheDef"
#include "CodeDef"
#include "AgentDef"

         void       ShowTable(int);

#ifdef R_11
#  define stat                            /* чтоб не пропали при Ovrly*/
#else
#  define stat static
#endif

  /* инициализируем просмотр таблицы */
  extern int        shiftshow;            /* номер показываемого эл.  */
         short      Nelems;               /* полное число элементов   */
         char       regshow = 'C';        /* номер показываемого эл.  */
         char*      title;                /* название таблицы         */
         char*      line;                 /* заголовок таблицы        */

  stat   agent      Ascan;
  stat   CacheElem  Cscan;
  stat   file_elem* Dscan;
  stat   PD         Lscan;
  stat   int        L0scan;
  stat   CdBlockH   Pscan;
  stat   int        Wscan;

void ShowTable(command) int command; {
  int i,j;
  if (command >= 0) {                     /* инициация таблицы        */
    if(command == ' ') command = regshow;
    switch((char) command) {
    default:
      title  = "Cache Blocks";
      line   = "_Numb_File_Page_Lock_Mod_";
      Nelems = NbCacheBuf; Cscan = First;
      for (i=0; i<shiftshow; ++i) Cscan = Cscan->next;

    break; case 'A':
      title = "Agents";
      line="_Id_Usr_St_Lock_Nx_Timer_Resour_Prog0_Prog1_Prog2_Prog3_";
      Nelems = NbAgents; Ascan = AgentTable+shiftshow;

    break; case 'W':
      title = "Work Files";
      for(j=0,i=TempFree; i>=0; ++j, i=TempMap[i]);
      Wscan = -1;
      for(Nelems=i=0; i<MaxFileNu-BgWrkFiles; ++i) {
        if(TempHeads[i].owner != 0xFF && Nelems++ == shiftshow) {
          Wscan = i;
        }
      }
      line=Fdump("Fi_Id_first_size__Free: %2.10/%3.10",
                    (unsigned)MaxFileNu-BgWrkFiles-Nelems,
                    (unsigned)j );

    break; case 'D':
      title = "Directory";
      line= "name__clst_state_addr_num_own_typ_nxt_size__Nrec__iR___iW___iM__";
      Dscan = Files + MaxFiles;
      for(i=Nelems=0; i<MaxFiles; ++i) {
        if((Files[i].status & BsyFile) && Nelems++ == shiftshow) {
          Dscan = Files+i;
        }
      }

    break; case 'L':
      title  = "Page Locks";

      {int k; PD Lscan1;
        Lscan= NIL; L0scan = HashSize;
        i= NbNewLocks; j = NbLocks;
        Nelems = 0;
        for(k=0; k<HashSize; ++k) {
          for(Lscan1=HashTable[k]; Lscan1 != NIL; Lscan1= Lscan1->link){
            if(Nelems == shiftshow) {L0scan=k; Lscan=Lscan1;}
            ++Nelems;
            if(Lscan1>=WhiteLocks) --i; else --j;
          }
        }
      }

      line = Fdump(
          "_Addr_File_Page_New__Id_Modp_delt_dnum__Free: %4.10/%4.10",
          (unsigned)i, (unsigned)j);

    break; case 'P':
      title = "Program blocks";
      line=
       "_Own_Cod___Ad_T__Ad_T__Ad_T__Ad_T__Ad_T__Ad_T__Ad_T__Ad_T_____";
      Nelems = 1;
      for(Pscan=Hblock; Pscan != NIL; Pscan=Pscan->nextloaded) ++Nelems;
      Pscan = Hblock;
    }
    regshow = (char) command;
    return;
  }

  line = NIL;
  switch(regshow) {
  case 'A':
    if (Ascan >= AgentTable + NbAgents) return;
    if(_cmps((char*)Ascan->pentry,(char*)initial,sizeof(jmp_buf))!=0){
      static char format[]=" %2.10 %3.10 %c  %4.16 %2.10 %4.16  %5.10";
      line = Fdump(format,
                   (unsigned) Ascan->ident,(unsigned) Ascan->userid,
                   (unsigned) ("RLID?"[Ascan->status]),
                   (unsigned) Ascan->waits,(unsigned) Ascan->queue,
                   (unsigned) (Ascan->starttime == -1 ? -1 :
                               Timer - Ascan->starttime),
                   (unsigned) Ascan->resources);
    } else {
      line = Fdump(" %2.10     %c",
                           (unsigned)Ascan->ident,(unsigned)'I');
    }
/*  for(j=0; j<MaxCodes; ++j) if (Ascan->codes[j].codesize != 0) {
      dfield(codes[j].outside) = (Ascan->codes[j].outside ? 'D':'M');
      dump(Ascan->codes[j].address ,codes[j].addr,16);
      dump(Ascan->codes[j].codesize,codes[j].size,16);
      dfield(codes[j].f1)=':';
    } */
    ++Ascan;

  break; default:
    if(Cscan == NIL) return;
    line = Fdump(" %2.10   %2.16   %4.16 %4.16 %c",
                  (unsigned)(Cscan-CacheTable),
                  (unsigned)Cscan->table, (unsigned)Cscan->page,
                  (unsigned)Cscan->locking,
                  (Cscan->modified ? '*' : ' ')
                );
    if( (Cscan = Cscan->next) == First) Cscan = NIL;

  break; case 'W':
    while(Wscan<MaxFileNu-BgWrkFiles && TempHeads[Wscan].owner==0xFF) {
      ++Wscan;
    }
    if (Wscan>=MaxFileNu-BgWrkFiles) return;
    for(i=0, j = TempHeads[Wscan].first; j>=0; ++i, j=TempMap[j]);
    line = Fdump("%2.16 %2.10 %4.16  %4.16",
                  (unsigned)(Wscan+BgWrkFiles),
                  (unsigned)TempHeads[Wscan].owner,
                  (unsigned)TempHeads[Wscan].first,(unsigned)i
                );

    ++Wscan;

  break; case 'D':
    while(Dscan<Files+MaxFiles && ! (Dscan->status & BsyFile)) ++Dscan;
    if (Dscan>=Files+MaxFiles) return;

    { static char dumpline[] =
       "           XXXX  %4.16 %2.16  %2.16  %2.16  %2.16  %5.10 %5.10 %4.16 %4.16 %4.16";
      if(Dscan->kind == tableKind) {       /* это таблица             */
        MVS(Dscan->b.tablename,10,dumpline);
      } else if(Dscan->kind == indexKind) {/* это индекс              */
        MVS(Dscan->b.indexdescr.indexname,MaxIndxName,dumpline);
        MVS(_conv((unsigned)Dscan->b.indexdescr.clustercount,6,10),6,
            dumpline+4);
      } else {
        MVS("<longdata>",10,dumpline);
      }
      for(j=0; j<4 ; ++j)
            dumpline[j+11] = (Dscan->status & (2<<j) ? "O-+$"[j] : ' ');
      line = Fdump(dumpline,(unsigned) Dscan,
                           (unsigned)(Dscan-Files),
                           (unsigned) Dscan->owner,
                           (unsigned) Dscan->mainfile,
                           (unsigned) Dscan->nextfile,
                           (unsigned) Dscan->actualsize,
                           (unsigned) Dscan->recordnum,
                           (unsigned) (Dscan->iR.Slock >> 16),
                           (unsigned) (Dscan->iW.Slock >> 16),
                           (unsigned) (Dscan->iM.Slock >> 16)
                  );
    }
    ++Dscan;

  break; case 'L':
    while (Lscan == NIL) {
      if(++L0scan >= HashSize) return;
      Lscan = HashTable[L0scan];
    }

    if(Xlocked(Lscan->locks)) {
      int count = Lscan->locks.Xlock.newspace;
      line = Fdump("%4.16  %2.16   %4.16 %4.16 %2.10 %4.16 %c%3.10 %c%2.10",
                    (unsigned) Lscan,
                    (unsigned) Lscan->table,(unsigned) Lscan->oldpage,
                    (unsigned)(Lscan < WhiteLocks ? Lscan->newpage : 0),
                    (unsigned) Lscan->locks.Xlock.owner,
                    (unsigned) Lscan->locks.Xlock.modpage,
                    (unsigned)(Files[Lscan->table].kind!=indexKind ?
                           (++count, ' ') : count & 1 ?
                           (count=count/2, '-') : (count=count/2 ,'+')),
                    (unsigned) count,
                    (unsigned)(Lscan->locks.Xlock.decrflag ? '-' : '+'),
                    (unsigned) Lscan->deltakey
                  );
    } else {
      line = Fdump("%4.16  %2.16   %4.16 %4.16 S%15.2",
                    (unsigned) Lscan,
                    (unsigned)Lscan->table,(unsigned) Lscan->oldpage,
                    (unsigned)(Lscan < WhiteLocks ? Lscan->newpage : 0),
                    (unsigned)(Lscan->locks.Slock>>16)
                  );
    }
    Lscan = Lscan->link;

  break; case 'P':
    {
    char*   plist;
    if (Pscan == (CdBlockH) -1) return;
    if (Pscan == NIL) {
      CdBlock block = Eblock;
      line = Fdump(" <Free> ");
      plist = line + 8;
      for(j=0; block != NIL && j<8; ++j) {
        MVS(_conv(block-CodeBuf,5,10),5,plist); plist +=5;
        *plist++ = ' ';
        block = *(CdBlock*) block;
      }
      Pscan = (CdBlockH) -1;
    } else {
      line = Fdump(" %3.10 %3.10  ",(unsigned)Pscan->owner,
                                    (unsigned)Pscan->codenum);
      plist = line + 10;
      for(j=0; j<8 && j<Pscan->Nblocks; ++j) {
        CdBlock block = Pscan->PageMap[j];
        MVS(_conv(block-CodeBuf,3,10),3,plist); plist += 3;
        *plist++ = '(';
        *plist++ = "HDC?"[block->head.blocktype];
        *plist++ = ')';
      }
      Pscan = Pscan->nextloaded;
    }
    *plist++ = 0;
    }
  }
  return;
}
