                   /* ============================= *\
                   ** ==         M.A.R.S.        == **
                   ** ==  Генерация описателя    == **
                   ** == созданного кода запроса == **
                   \* ============================= */

#include "FilesDef"
#include "AgentDef"
#include "TreeDef"

   void Describe(node, unchar, dsc_table*);

    /*--------*-----*------------*------------*---------*------------*--
     | полная |ид-ор| число вх.  | длина фикс | число   | число вых. |
     | длина  | кода| параметров | части вx.  | Nvar вх.| колонок    |
     *--------*-----*------------*------------*---------*------------*--
      -*-----------------------*--------------*--------*
       | типы, длины и смещения|  адреса      | имена  |
       | (входные, потом вых.) |  концов имен |        |
      -*-----------------------*--------------*--------*/

typedef  struct {short type,length,shift;} edescr;

typedef struct {
  short  totallength;
  short  ident;
  short  Ninput,fixlength,Nvar;
  short  Noutput;
  edescr elements[DummySize];
} descriptor;

void Describe(node Node, unchar code, dsc_table* Fields) {

  descriptor* header = (descriptor*) CommonBuffer;
  edescr*     escan  = header->elements;

  char*   nscan;
  char*   nscan0;
  short   Ncolumns = 0;
  short*  lscan;

  dsc_column* Param = (dsc_column*) (Fields+1);
  projection  proj;

  header->ident = code;

  /* описатели входных параметров */
  header->Ninput    = Fields->Ncolumns;
  header->fixlength = Fields->Mfixlength;
  header->Nvar      = Fields->Nvarlength;

  for(Ncolumns = 0; Ncolumns < Fields->Ncolumns; ++Ncolumns) {
    escan->type   = Param->type;
    escan->length = Param->length;
    escan->shift  = Param->shift;
    ++escan;
    Param = (dsc_column*) ((char*) Param + Param->totalleng);
  }

  /* описатели выходных записей */
  header->Noutput = 0;
  if( Node->mode < MODIFICATION) {
    for(Ncolumns=0, proj = Node->projlist; proj!=0; proj = proj->next) {
      escan->type   = proj->expr->type;
      escan->length = proj->expr->leng;
      escan->shift  = proj->location;
      ++Ncolumns; ++escan;
    }
    header->Noutput = Ncolumns;
  }

  /* заносим имена (входные параметры и выходные столбцы */
  lscan = (short*) escan + header->Noutput + Fields->Ncolumns;
  nscan0= nscan = (char*) lscan;

  /* заносим имена входных параметров */
  Param = (dsc_column*) (Fields+1);
  for(Ncolumns = 0; Ncolumns < Fields->Ncolumns; ++Ncolumns) {
    int length = *Param->name;
    MVS(Param->name+1,length,nscan); nscan += length;
    *--lscan = nscan-nscan0;
    Param = (dsc_column*) ((char*) Param + Param->totalleng);
  }
    /* заносим имена выходных столбцов */
  if( Node->mode < MODIFICATION) {
    for(proj = Node->projlist; proj != 0; proj = proj->next) {
      int length = *proj->name;
      MVS(proj->name+1,length,nscan); nscan += length;
      *--lscan = nscan-nscan0;
    }
  }

  header->totallength = nscan - (char*) header;
  OutBuffer = (short*) header; OutAgent = Agent->ident;
}
