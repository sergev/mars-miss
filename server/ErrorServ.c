                    /* ========================== *\
                    ** ==        M.A.R.S.      == **
                    ** ==  Отладочные выдачи   == **
                    ** == сообщений об ошибках == **
                    \* ========================== */

#include "FilesDef"
#include "AgentDef"
         void   ErrTrace(void);

  extern void   Reply(char*);

void ErrTrace() {
  static char* errmess[] =
    {"??","Calculation overflow",
          "Syntax error" ,"Illegal name", "System Error",
          "Illegal symbol", "Illegal keyword", "Number error",
          "Illegal length", "Illegal string",
          "Too complicated",
          "Multidefined",   "Undefined",
          "Already exists", "Catalogue is full", "Object not found",
          "Illegal type",
          "Code not found",
          "Subquery table is to be modified",
          "Illegal number of columns", "Illegal column name",
          "Deadlock", "Init Error",
          "Illegal reduction", "Not a group constant",
          "Work files pool exhausted",
          "Table overflow",
          "Index overflow",
          "Update pool overflow",
          "Multidefined key in a unique index",
          "Lock space exhausted",
          "Illegal command",
          "Illegal code parameters",
          "Code cursor closed",
          "Too many codes",
          "Not allowed",
          "User name not found",
          "Incorrect password",
          "Work files storage exhausted",
          "E-query must have 1 column",
          "E-query gave more than 1 record",
          "E-query gave no record",
          "Too big volume to sort",
          "Interrupted",
          "Too long lData",
          "Illegal lData num"
    };


#ifdef MISS
  if(ErrPosition>=0) {
        short out = -1;
        do {
      static char  errline[] = {2,0,'.','!',0};
      short slice = ErrPosition-out;
      if(slice >= 0x100) slice = 0xFF;
      errline[1] = (char) slice; Reply(errline);
      out += slice;
    } while(out < ErrPosition);
  }
#endif
  Reply(errmess[ErrCode]);
}
