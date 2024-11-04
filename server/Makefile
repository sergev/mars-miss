# For Interactive Unix 386
#CC              = gcc -O -fstrength-reduce -finline-functions
#CFLAGS          = -DUSE_NET_ERRNO -DNO_FCHDIR -DNO_MEMMOVE
#LIBS            = -linet -lcposix

# For Xenix386 with Lachman TCP/IP
#CC              = gcc -pipe -O -fstrength-reduce -finline-functions
#CFLAGS          = -DLAI_TCP -DNO_FSYNC -DNO_FCHDIR -DNO_MEMMOVE
#LIBS            = -lsocket -lmalloc

# For Ultrix
#CC              = gcc -pipe -O -fstrength-reduce -finline-functions
#CFLAGS          = -DUSE_UNIX_DOMAIN -DNO_STDLIB -DFCNTL\
#                  -DNO_SUN_LEN -DNO_STRERROR -DNO_FCHDIR -DNO_MEMMOVE

# For 386bsd
#CC              = cc -pipe -O -fstrength-reduce -finline-functions
#CFLAGS          = -DUSE_UNIX_DOMAIN

# For Mac OSX
CC              = cc -O
CFLAGS          = -DUSE_UNIX_DOMAIN

SHELL           = /bin/sh
SRCS            = Archive.c CacheServ.c Calendar.c Compile.c CreateDrop.c\
		  Describe.c Directory.c ErrorServ.c Executor.c Fdump.c\
		  FormKey.c GetCatalog.c GetIndex.c GetMem.c GetRecord.c\
		  InitSystem.c LinkTree.c LongData.c Main.c Match.c Optimize.c\
		  OutSyncro.c ParsBufSrv.c Parser.c Rights.c Scheduler.c\
		  Show.c ShowTable.c Sort.c Swapping.c Talk.c

OBJS            = Archive.o CacheServ.o Calendar.o Compile.o CreateDrop.o\
		  Describe.o Directory.o ErrorServ.o Executor.o Fdump.o\
		  FormKey.o GetCatalog.o GetIndex.o GetMem.o GetRecord.o\
		  InitSystem.o LinkTree.o LongData.o Main.o Match.o Optimize.o\
		  OutSyncro.o ParsBufSrv.o Parser.o Rights.o Scheduler.o\
		  Show.o ShowTable.o Sort.o Swapping.o Talk.o

marsd:          $(OBJS)
		$(CC) $(LDFLAGS) -o marsd $(OBJS) $(LIBS)

clean:
		rm -f *.b marsd *.o .,*

Archive.o: FilesDef TypesDef Globals CacheDef AgentDef CodeDef FSysDef
CacheServ.o: FilesDef TypesDef Globals AgentDef CodeDef CacheDef DataDef
Calendar.o: FilesDef TypesDef Globals
Compile.o: FilesDef TypesDef Globals TreeDef CodeDef ExecDef
CreateDrop.o: FilesDef TypesDef Globals AgentDef TreeDef CacheDef
Describe.o: FilesDef TypesDef Globals AgentDef TreeDef
Directory.o: FilesDef TypesDef Globals AgentDef
ErrorServ.o: FilesDef TypesDef Globals AgentDef
Executor.o: FilesDef TypesDef Globals CodeDef ExecDef AgentDef BuiltinDef
Fdump.o: FilesDef TypesDef Globals
FormKey.o: FilesDef TypesDef Globals
GetCatalog.o: FilesDef TypesDef Globals AgentDef TreeDef
GetIndex.o: FilesDef TypesDef Globals
GetMem.o: FilesDef TypesDef Globals AgentDef TreeDef
GetRecord.o: FilesDef TypesDef Globals AgentDef DataDef
InitSystem.o: FilesDef TypesDef Globals CacheDef
LinkTree.o: FilesDef TypesDef Globals AgentDef TreeDef BuiltinDef
LongData.o: FilesDef TypesDef Globals CodeDef AgentDef
Main.o: FilesDef TypesDef Globals AgentDef
Match.o: FilesDef TypesDef Globals
Optimize.o: FilesDef TypesDef Globals TreeDef
OutSyncro.o: FilesDef TypesDef Globals CacheDef
ParsBufSrv.o: FilesDef TypesDef Globals AgentDef TreeDef
Parser.o: FilesDef TypesDef Globals AgentDef TreeDef BuiltinDef
Rights.o: FilesDef TypesDef Globals AgentDef TreeDef
Scheduler.o: FilesDef TypesDef Globals AgentDef
Show.o: FilesDef TypesDef Globals
ShowTable.o: FilesDef TypesDef Globals CacheDef CodeDef AgentDef
Sort.o: FilesDef TypesDef Globals AgentDef TreeDef
Swapping.o: FilesDef TypesDef Globals AgentDef CodeDef ExecDef
Talk.o: FilesDef TypesDef Globals AgentDef
