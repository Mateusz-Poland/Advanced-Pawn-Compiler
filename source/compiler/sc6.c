/*  Pawn compiler - Binary code generation (the "assembler")
 *
 *  Copyright (c) ITB CompuPhase, 1997-2006
 *
 *  This software is provided "as-is", without any express or implied warranty.
 *  In no event will the authors be held liable for any damages arising from
 *  the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1.  The origin of this software must not be misrepresented; you must not
 *      claim that you wrote the original software. If you use this software in
 *      a product, an acknowledgment in the product documentation would be
 *      appreciated but is not required.
 *  2.  Altered source versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software.
 *  3.  This notice may not be removed or altered from any source distribution.
 *
 *  Version: $Id: sc6.c 3648 2006-10-12 11:24:50Z thiadmer $
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>     /* for macro max() */
#include <stddef.h>     /* for macro offsetof() */
#include <string.h>
#include <ctype.h>
#if defined FORTIFY
  #include <alloc/fortify.h>
#endif
#include "lstring.h"
#if !defined SC_LIGHT && !defined __MSDOS__
  #include "memfile.h"
#endif
#include "sc.h"
#include "../amx/amxdbg.h"
#if defined LINUX || defined __FreeBSD__ || defined __OpenBSD__
  #include <sclinux.h>
#elif defined _MSC_VER && defined _WIN32
  #define stricmp _stricmp
#endif


static int append_dbginfo(FILE *fout);


typedef cell (SC_FASTCALL *OPCODE_PROC)(FILE *fbin,char *params,cell opcode);

typedef struct {
  cell opcode;
  char *name;
  int segment;          /* sIN_CSEG=parse in cseg, sIN_DSEG=parse in dseg */
  OPCODE_PROC func;
} OPCODE;

static cell codeindex;  /* similar to "code_idx" */
static cell *lbltab;    /* label table */
static int writeerror;
static int bytes_in, bytes_out;
static jmp_buf compact_err;

/* apparently, strtol() does not work correctly on very large (unsigned)
 * hexadecimal values */
static ucell hex2long(const char *s,char **n)
{
  ucell result=0L;
  int negate=FALSE;
  int digit;

  /* ignore leading whitespace */
  while (*s==' ' || *s=='\t')
    s++;

  /* allow a negation sign to create the two's complement of numbers */
  if (*s=='-') {
    negate=TRUE;
    s++;
  } /* if */

  assert((*s>='0' && *s<='9') || (*s>='a' && *s<='f') || (*s>='A' && *s<='F'));
  for ( ;; ) {
    if (*s>='0' && *s<='9')
      digit=*s-'0';
    else if (*s>='a' && *s<='f')
      digit=*s-'a' + 10;
    else if (*s>='A' && *s<='F')
      digit=*s-'A' + 10;
    else
      break;    /* probably whitespace */
    result=(result<<4) | digit;
    s++;
  } /* for */
  if (n!=NULL)
    *n=(char*)s;
  if (negate)
    result=(~result)+1; /* take two's complement of the result */
  return (ucell)result;
}

static ucell getparam(const char *s,char **n)
{
  int i;
  ucell result=0;
  char name[sNAMEMAX+1];
  symbol *sym;

  if (s[0]=='.') {
    /* this is a function, find it in the global symbol table */
    for (i=0; !isspace(*(++s)); i++) {
      assert(*s!='\0');
      assert(i<sNAMEMAX);
      name[i]=*s;
    } /* for */
    name[i]='\0';
    sym=findglb(name,sGLOBAL);
    assert(sym!=NULL);
    assert(sym->ident==iFUNCTN || sym->ident==iREFFUNC);
    assert(sym->vclass==sGLOBAL);
    result=sym->addr;
  } else if (s[0]=='l' && s[1]=='.') {
    /* this is a label */
    i=(int)hex2long(s+2,NULL);
    assert(i>=0 && i<sc_labnum);
    assert(lbltab!=NULL);
    result=lbltab[i];
  } else {
    for ( ;; ) {
      result+=hex2long(s,(char**)&s);
      if (*s!='+')
        break;
      s++;
    } /* for */
  } /* if */
  if (n!=NULL)
    *n=(char*)s;
  return result;
}

#if BYTE_ORDER==BIG_ENDIAN
static uint16_t *align16(uint16_t *v)
{
  unsigned char *s = (unsigned char *)v;
  unsigned char t;

  /* swap two bytes */
  t=s[0];
  s[0]=s[1];
  s[1]=t;
  return v;
}

static uint32_t *align32(uint32_t *v)
{
  unsigned char *s = (unsigned char *)v;
  unsigned char t;

  /* swap outer two bytes */
  t=s[0];
  s[0]=s[3];
  s[3]=t;
  /* swap inner two bytes */
  t=s[1];
  s[1]=s[2];
  s[2]=t;
  return v;
}

#if PAWN_CELL_SIZE>=64
static uint64_t *align64(uint64_t *v)
{
  unsigned char *s = (unsigned char *)v;
  unsigned char t;

  t=s[0];
  s[0]=s[7];
  s[7]=t;

  t=s[1];
  s[1]=s[6];
  s[6]=t;

  t=s[2];
  s[2]=s[5];
  s[5]=t;

  t=s[3];
  s[3]=s[4];
  s[4]=t;

  return v;
}
#endif

  #if PAWN_CELL_SIZE==16
    #define aligncell(v)  align16(v)
  #elif PAWN_CELL_SIZE==32
    #define aligncell(v)  align32(v)
  #elif PAWN_CELL_SIZE==64
    #define aligncell(v)  align64(v)
  #endif
#else
  #define align16(v)    (v)
  #define align32(v)    (v)
  #define aligncell(v)  (v)
#endif

static char *skipwhitespace(char *str)
{
  while (isspace(*str))
    str++;
  return str;
}

static char *stripcomment(char *str)
{
  char *ptr=strchr(str,';');
  if (ptr!=NULL) {
    *ptr++='\n';        /* terminate the line, but leave the '\n' */
    *ptr='\0';
  } /* if */
  return str;
}

static char *stripwhitespace(char *str)
{
  if (*str!='\0') {
    size_t len = strlen(str);
    size_t i;
    for (i=len; i>=1; i--) {
      if (!isspace(str[i-1])) {
        str[i]='\0';
        break;
      }
    }
  }
  return str;
}

static unsigned char *encode_cell(ucell c,int *len)
{
  #if PAWN_CELL_SIZE == 16
    #define ENC_MAX   3     /* a 16-bit cell is encoded in max. 3 bytes */
    #define ENC_MASK  0x03  /* after 2x7 bits, 2 bits remain to make 16 bits */
  #elif PAWN_CELL_SIZE == 32
    #define ENC_MAX   5     /* a 32-bit cell is encoded in max. 5 bytes */
    #define ENC_MASK  0x0f  /* after 4x7 bits, 4 bits remain to make 32 bits */
  #elif PAWN_CELL_SIZE == 64
    #define ENC_MAX   10    /* a 32-bit cell is encoded in max. 10 bytes */
    #define ENC_MASK  0x01  /* after 9x7 bits, 1 bit remains to make 64 bits */
  #endif

  static unsigned char buffer[ENC_MAX];
  unsigned char *ptr;
  int index;

  assert(len!=NULL);
  /* Small signed immediate values fit in one signed 7-bit group. */
  if (c<=0x3f || c>=(ucell)-64) {
    buffer[ENC_MAX-1]=(unsigned char)(c & 0x7f);
    *len=1;
    return buffer+ENC_MAX-1;
  }
  index=ENC_MAX-1;
  do {
    buffer[index--]=(unsigned char)(c & 0x7f);  /* store 7 bits */
    c>>=7;
  } while (index>=0);
  index=0;
  /* skip leading zeros */
  while (index<ENC_MAX-1 && buffer[index]==0 && (buffer[index+1] & 0x40)==0)
    index++;
  /* skip leading -1s */
  if (index==0 && buffer[index]==ENC_MASK && (buffer[index+1] & 0x40)!=0)
    index++;
  while (index<ENC_MAX-1 && buffer[index]==0x7f && (buffer[index+1] & 0x40)!=0)
    index++;
  *len=ENC_MAX-index;
  ptr=&buffer[index];
  while (index<ENC_MAX-1)
    buffer[index++] |= 0x80;
  return ptr;
}

static void write_encoded(FILE *fbin,ucell *c,int num)
{
  int len;
  unsigned char *bytes;

  assert(fbin!=NULL);
  while (num-->0) {
    if (sc_compress) {
      bytes=encode_cell(*c,&len);
      assert(len>0);
      writeerror |= !pc_writebin(fbin,bytes,len);
      bytes_out+=len;
      bytes_in+=sizeof *c;
      assert(AMX_COMPACTMARGIN>2);
      if (bytes_out-bytes_in>=AMX_COMPACTMARGIN-2)
        longjmp(compact_err,1);
    } else {
      assert((pc_lengthbin(fbin) % sizeof(cell)) == 0);
      writeerror |= !pc_writebin(fbin,aligncell(c),sizeof *c);
    } /* if */
    c++;
  } /* while */
}

static void write_encoded_n(FILE *fbin,ucell c,int num)
{
  int len;
  unsigned char *bytes;

  assert(fbin != NULL);
  if (sc_compress) {
    bytes=encode_cell(c,&len);
    assert(len>0);
    bytes_in += num*sizeof c;
    bytes_out += num*len;
    if (bytes_out-bytes_in>=AMX_COMPACTMARGIN-2)
      longjmp(compact_err,1);
  } else {
    c=aligncell(c);
    bytes=(unsigned char *)&c;
    len=sizeof c;
    assert((pc_lengthbin(fbin) % sizeof(cell)) == 0);
  }
  if (num>1) {
    unsigned char block[4096];
    int capacity=sizeof block/len;
    int count=num<capacity ? num : capacity;
    int filled=1;
    memcpy(block,bytes,len);
    while (filled<count) {
      int extra=count-filled<filled ? count-filled : filled;
      memcpy(block+filled*len,block,extra*len);
      filled+=extra;
    }
    while (num>0) {
      int chunk=num<count ? num : count;
      writeerror |= !pc_writebin(fbin,block,chunk*len);
      num-=chunk;
    }
  } else if (num==1) {
    writeerror |= !pc_writebin(fbin,bytes,len);
  } /* if */
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
static cell SC_FASTCALL noop(FILE *fbin,char *params,cell opcode)
{
  return 0;
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
static cell SC_FASTCALL set_currentfile(FILE *fbin,char *params,cell opcode)
{
  fcurrent=(short)getparam(params,NULL);
  return 0;
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
static cell SC_FASTCALL parm0(FILE *fbin,char *params,cell opcode)
{
  if (fbin!=NULL)
    write_encoded(fbin,(ucell*)&opcode,1);
  return opcodes(1);
}

static cell SC_FASTCALL parm1(FILE *fbin,char *params,cell opcode)
{
  ucell p=getparam(params,NULL);
  if (fbin!=NULL) {
    write_encoded(fbin,(ucell*)&opcode,1);
    write_encoded(fbin,&p,1);
  } /* if */
  return opcodes(1)+opargs(1);
}

static cell SC_FASTCALL parm2(FILE *fbin,char *params,cell opcode)
{
  ucell p1=getparam(params,&params);
  ucell p2=getparam(params,NULL);
  if (fbin!=NULL) {
    write_encoded(fbin,(ucell*)&opcode,1);
    write_encoded(fbin,&p1,1);
    write_encoded(fbin,&p2,1);
  } /* if */
  return opcodes(1)+opargs(2);
}

static cell SC_FASTCALL parm3(FILE *fbin,char *params,cell opcode)
{
  ucell p1=getparam(params,&params);
  ucell p2=getparam(params,&params);
  ucell p3=getparam(params,NULL);
  if (fbin!=NULL) {
    write_encoded(fbin,(ucell*)&opcode,1);
    write_encoded(fbin,&p1,1);
    write_encoded(fbin,&p2,1);
    write_encoded(fbin,&p3,1);
  } /* if */
  return opcodes(1)+opargs(3);
}

static cell SC_FASTCALL parm4(FILE *fbin,char *params,cell opcode)
{
  ucell p1=getparam(params,&params);
  ucell p2=getparam(params,&params);
  ucell p3=getparam(params,&params);
  ucell p4=getparam(params,NULL);
  if (fbin!=NULL) {
    write_encoded(fbin,(ucell*)&opcode,1);
    write_encoded(fbin,&p1,1);
    write_encoded(fbin,&p2,1);
    write_encoded(fbin,&p3,1);
    write_encoded(fbin,&p4,1);
  } /* if */
  return opcodes(1)+opargs(4);
}

static cell SC_FASTCALL parm5(FILE *fbin,char *params,cell opcode)
{
  ucell p1=getparam(params,&params);
  ucell p2=getparam(params,&params);
  ucell p3=getparam(params,&params);
  ucell p4=getparam(params,&params);
  ucell p5=getparam(params,NULL);
  if (fbin!=NULL) {
    write_encoded(fbin,(ucell*)&opcode,1);
    write_encoded(fbin,&p1,1);
    write_encoded(fbin,&p2,1);
    write_encoded(fbin,&p3,1);
    write_encoded(fbin,&p4,1);
    write_encoded(fbin,&p5,1);
  } /* if */
  return opcodes(1)+opargs(5);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
static cell SC_FASTCALL do_dump(FILE *fbin,char *params,cell opcode)
{
  ucell p;
  int num=0;

  while (*params!='\0') {
    p=hex2long(params,&params);
    if (fbin!=NULL)
      write_encoded(fbin,&p,1);
    num++;
    while (isspace(*params))
      params++;
  } /* while */
  return num*sizeof(cell);
}

static cell SC_FASTCALL do_dumpn(FILE *fbin,char *params,cell opcode)
{
  ucell value;
  int num;

  value=hex2long(params,&params);
  num=(int)hex2long(params,NULL);
  if (fbin!=NULL)
    write_encoded_n(fbin,value,num);
  return num*sizeof(cell);
}

static cell SC_FASTCALL do_call(FILE *fbin,char *params,cell opcode)
{
  char name[sNAMEMAX+2]; /* +1 for a possible leading dot */
  int i;
  symbol *sym;
  ucell p;

  for (i=0; !isspace(*params); i++,params++) {
    assert(*params!='\0');
    assert(i<sNAMEMAX+1);
    name[i]=*params;
  } /* for */
  name[i]='\0';

  if (name[0]=='l' && name[1]=='.') {
    /* this is a label, not a function symbol */
    i=(int)hex2long(name+2,NULL);
    assert(i>=0 && i<sc_labnum);
    if (fbin!=NULL) {
      assert(lbltab!=NULL);
      p=lbltab[i];
    } /* if */
  } else {
    /* look up the function address; note that the correct file number must
     * already have been set (in order for static globals to be found).
     */
    assert(name[0]=='.');
    sym=findglb(name+1,sGLOBAL);
    assert(sym!=NULL);
    assert(sym->ident==iFUNCTN || sym->ident==iREFFUNC);
    assert(sym->vclass==sGLOBAL);
    p=sym->addr;
  } /* if */

  if (fbin!=NULL) {
    write_encoded(fbin,(ucell*)&opcode,1);
    write_encoded(fbin,&p,1);
  } /* if */
  return opcodes(1)+opargs(1);
}

static cell SC_FASTCALL do_jump(FILE *fbin,char *params,cell opcode)
{
  int i;
  ucell p;

  i=(int)hex2long(params,NULL);
  assert(i>=0 && i<sc_labnum);

  if (fbin!=NULL) {
    assert(lbltab!=NULL);
    p=lbltab[i];
    write_encoded(fbin,(ucell*)&opcode,1);
    write_encoded(fbin,&p,1);
  } /* if */
  return opcodes(1)+opargs(1);
}

static cell SC_FASTCALL do_switch(FILE *fbin,char *params,cell opcode)
{
  int i;
  ucell p;

  i=(int)hex2long(params,NULL);
  assert(i>=0 && i<sc_labnum);

  if (fbin!=NULL) {
    assert(lbltab!=NULL);
    p=lbltab[i];
    write_encoded(fbin,(ucell*)&opcode,1);
    write_encoded(fbin,&p,1);
  } /* if */
  return opcodes(1)+opargs(1);
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
static cell SC_FASTCALL do_case(FILE *fbin,char *params,cell opcode)
{
  int i;
  ucell p,v;

  v=hex2long(params,&params);
  i=(int)hex2long(params,NULL);
  assert(i>=0 && i<sc_labnum);

  if (fbin!=NULL) {
    assert(lbltab!=NULL);
    p=lbltab[i];
    write_encoded(fbin,&v,1);
    write_encoded(fbin,&p,1);
  } /* if */
  return opcodes(0)+opargs(2);
}

static OPCODE opcodelist[] = {
  /* node for "invalid instruction" */
  {  0, NULL,         0,        noop },
  /* opcodes in sorted order */
  { 78, "add",        sIN_CSEG, parm0 },
  { 87, "add.c",      sIN_CSEG, parm1 },
  { 14, "addr.alt",   sIN_CSEG, parm1 },
  { 13, "addr.pri",   sIN_CSEG, parm1 },
  { 30, "align.alt",  sIN_CSEG, parm1 },
  { 29, "align.pri",  sIN_CSEG, parm1 },
  { 81, "and",        sIN_CSEG, parm0 },
  {121, "bounds",     sIN_CSEG, parm1 },
  {137, "break",      sIN_CSEG, parm0 },  /* version 8 */
  { 49, "call",       sIN_CSEG, do_call },
  { 50, "call.pri",   sIN_CSEG, parm0 },
  {  0, "case",       sIN_CSEG, do_case },
  {130, "casetbl",    sIN_CSEG, parm0 },  /* version 1 */
  {118, "cmps",       sIN_CSEG, parm1 },
  {  0, "code",       sIN_CSEG, set_currentfile },
  {156, "const",      sIN_CSEG, parm2 },  /* version 9 */
  { 12, "const.alt",  sIN_CSEG, parm1 },
  { 11, "const.pri",  sIN_CSEG, parm1 },
  {157, "const.s",    sIN_CSEG, parm2 },  /* version 9 */
  {  0, "data",       sIN_DSEG, set_currentfile },
  {114, "dec",        sIN_CSEG, parm1 },
  {113, "dec.alt",    sIN_CSEG, parm0 },
  {116, "dec.i",      sIN_CSEG, parm0 },
  {112, "dec.pri",    sIN_CSEG, parm0 },
  {115, "dec.s",      sIN_CSEG, parm1 },
  {  0, "dump",       sIN_DSEG, do_dump },
  {  0, "dumpn",      sIN_DSEG, do_dumpn },
  { 95, "eq",         sIN_CSEG, parm0 },
  {106, "eq.c.alt",   sIN_CSEG, parm1 },
  {105, "eq.c.pri",   sIN_CSEG, parm1 },
/*{124, "file",       sIN_CSEG, do_file }, */
  {119, "fill",       sIN_CSEG, parm1 },
  {100, "geq",        sIN_CSEG, parm0 },
  { 99, "grtr",       sIN_CSEG, parm0 },
  {120, "halt",       sIN_CSEG, parm1 },
  { 45, "heap",       sIN_CSEG, parm1 },
  { 27, "idxaddr",    sIN_CSEG, parm0 },
  { 28, "idxaddr.b",  sIN_CSEG, parm1 },
  {109, "inc",        sIN_CSEG, parm1 },
  {108, "inc.alt",    sIN_CSEG, parm0 },
  {111, "inc.i",      sIN_CSEG, parm0 },
  {107, "inc.pri",    sIN_CSEG, parm0 },
  {110, "inc.s",      sIN_CSEG, parm1 },
  { 86, "invert",     sIN_CSEG, parm0 },
  { 55, "jeq",        sIN_CSEG, do_jump },
  { 60, "jgeq",       sIN_CSEG, do_jump },
  { 59, "jgrtr",      sIN_CSEG, do_jump },
  { 58, "jleq",       sIN_CSEG, do_jump },
  { 57, "jless",      sIN_CSEG, do_jump },
  { 56, "jneq",       sIN_CSEG, do_jump },
  { 54, "jnz",        sIN_CSEG, do_jump },
  { 52, "jrel",       sIN_CSEG, parm1 },  /* always a number */
  { 64, "jsgeq",      sIN_CSEG, do_jump },
  { 63, "jsgrtr",     sIN_CSEG, do_jump },
  { 62, "jsleq",      sIN_CSEG, do_jump },
  { 61, "jsless",     sIN_CSEG, do_jump },
  { 51, "jump",       sIN_CSEG, do_jump },
  {128, "jump.pri",   sIN_CSEG, parm0 },  /* version 1 */
  { 53, "jzer",       sIN_CSEG, do_jump },
  { 31, "lctrl",      sIN_CSEG, parm1 },
  { 98, "leq",        sIN_CSEG, parm0 },
  { 97, "less",       sIN_CSEG, parm0 },
  { 25, "lidx",       sIN_CSEG, parm0 },
  { 26, "lidx.b",     sIN_CSEG, parm1 },
/*{125, "line",       sIN_CSEG, parm2 }, */
  {  2, "load.alt",   sIN_CSEG, parm1 },
  {154, "load.both",  sIN_CSEG, parm2 },  /* version 9 */
  {  9, "load.i",     sIN_CSEG, parm0 },
  {  1, "load.pri",   sIN_CSEG, parm1 },
  {  4, "load.s.alt", sIN_CSEG, parm1 },
  {155, "load.s.both",sIN_CSEG, parm2 },  /* version 9 */
  {  3, "load.s.pri", sIN_CSEG, parm1 },
  { 10, "lodb.i",     sIN_CSEG, parm1 },
  {  6, "lref.alt",   sIN_CSEG, parm1 },
  {  5, "lref.pri",   sIN_CSEG, parm1 },
  {  8, "lref.s.alt", sIN_CSEG, parm1 },
  {  7, "lref.s.pri", sIN_CSEG, parm1 },
  { 34, "move.alt",   sIN_CSEG, parm0 },
  { 33, "move.pri",   sIN_CSEG, parm0 },
  {117, "movs",       sIN_CSEG, parm1 },
  { 85, "neg",        sIN_CSEG, parm0 },
  { 96, "neq",        sIN_CSEG, parm0 },
  {134, "nop",        sIN_CSEG, parm0 },  /* version 6 */
  { 84, "not",        sIN_CSEG, parm0 },
  { 82, "or",         sIN_CSEG, parm0 },
  { 43, "pop.alt",    sIN_CSEG, parm0 },
  { 42, "pop.pri",    sIN_CSEG, parm0 },
  { 46, "proc",       sIN_CSEG, parm0 },
  { 40, "push",       sIN_CSEG, parm1 },
  {133, "push.adr",   sIN_CSEG, parm1 },  /* version 4 */
  { 37, "push.alt",   sIN_CSEG, parm0 },
  { 39, "push.c",     sIN_CSEG, parm1 },
  { 36, "push.pri",   sIN_CSEG, parm0 },
  { 38, "push.r",     sIN_CSEG, parm1 },  /* obsolete (never generated) */
  { 41, "push.s",     sIN_CSEG, parm1 },
  {139, "push2",      sIN_CSEG, parm2 },  /* version 9 */
  {141, "push2.adr",  sIN_CSEG, parm2 },  /* version 9 */
  {138, "push2.c",    sIN_CSEG, parm2 },  /* version 9 */
  {140, "push2.s",    sIN_CSEG, parm2 },  /* version 9 */
  {143, "push3",      sIN_CSEG, parm3 },  /* version 9 */
  {145, "push3.adr",  sIN_CSEG, parm3 },  /* version 9 */
  {142, "push3.c",    sIN_CSEG, parm3 },  /* version 9 */
  {144, "push3.s",    sIN_CSEG, parm3 },  /* version 9 */
  {147, "push4",      sIN_CSEG, parm4 },  /* version 9 */
  {149, "push4.adr",  sIN_CSEG, parm4 },  /* version 9 */
  {146, "push4.c",    sIN_CSEG, parm4 },  /* version 9 */
  {148, "push4.s",    sIN_CSEG, parm4 },  /* version 9 */
  {151, "push5",      sIN_CSEG, parm5 },  /* version 9 */
  {153, "push5.adr",  sIN_CSEG, parm5 },  /* version 9 */
  {150, "push5.c",    sIN_CSEG, parm5 },  /* version 9 */
  {152, "push5.s",    sIN_CSEG, parm5 },  /* version 9 */
  { 47, "ret",        sIN_CSEG, parm0 },
  { 48, "retn",       sIN_CSEG, parm0 },
  { 32, "sctrl",      sIN_CSEG, parm1 },
  { 73, "sdiv",       sIN_CSEG, parm0 },
  { 74, "sdiv.alt",   sIN_CSEG, parm0 },
  {104, "sgeq",       sIN_CSEG, parm0 },
  {103, "sgrtr",      sIN_CSEG, parm0 },
  { 65, "shl",        sIN_CSEG, parm0 },
  { 69, "shl.c.alt",  sIN_CSEG, parm1 },
  { 68, "shl.c.pri",  sIN_CSEG, parm1 },
  { 66, "shr",        sIN_CSEG, parm0 },
  { 71, "shr.c.alt",  sIN_CSEG, parm1 },
  { 70, "shr.c.pri",  sIN_CSEG, parm1 },
  { 94, "sign.alt",   sIN_CSEG, parm0 },
  { 93, "sign.pri",   sIN_CSEG, parm0 },
  {102, "sleq",       sIN_CSEG, parm0 },
  {101, "sless",      sIN_CSEG, parm0 },
  { 72, "smul",       sIN_CSEG, parm0 },
  { 88, "smul.c",     sIN_CSEG, parm1 },
/*{127, "srange",     sIN_CSEG, parm2 }, -- version 1 */
  { 20, "sref.alt",   sIN_CSEG, parm1 },
  { 19, "sref.pri",   sIN_CSEG, parm1 },
  { 22, "sref.s.alt", sIN_CSEG, parm1 },
  { 21, "sref.s.pri", sIN_CSEG, parm1 },
  { 67, "sshr",       sIN_CSEG, parm0 },
  { 44, "stack",      sIN_CSEG, parm1 },
  {  0, "stksize",    0,        noop },
  { 16, "stor.alt",   sIN_CSEG, parm1 },
  { 23, "stor.i",     sIN_CSEG, parm0 },
  { 15, "stor.pri",   sIN_CSEG, parm1 },
  { 18, "stor.s.alt", sIN_CSEG, parm1 },
  { 17, "stor.s.pri", sIN_CSEG, parm1 },
  { 24, "strb.i",     sIN_CSEG, parm1 },
  { 79, "sub",        sIN_CSEG, parm0 },
  { 80, "sub.alt",    sIN_CSEG, parm0 },
  {132, "swap.alt",   sIN_CSEG, parm0 },  /* version 4 */
  {131, "swap.pri",   sIN_CSEG, parm0 },  /* version 4 */
  {129, "switch",     sIN_CSEG, do_switch }, /* version 1 */
/*{126, "symbol",     sIN_CSEG, do_symbol }, */
/*{136, "symtag",     sIN_CSEG, parm1 },  -- version 7 */
  {123, "sysreq.c",   sIN_CSEG, parm1 },
  {135, "sysreq.n",   sIN_CSEG, parm2 },  /* version 9 (replaces SYSREQ.d from earlier version) */
  {122, "sysreq.pri", sIN_CSEG, parm0 },
  { 76, "udiv",       sIN_CSEG, parm0 },
  { 77, "udiv.alt",   sIN_CSEG, parm0 },
  { 75, "umul",       sIN_CSEG, parm0 },
  { 35, "xchg",       sIN_CSEG, parm0 },
  { 83, "xor",        sIN_CSEG, parm0 },
  { 91, "zero",       sIN_CSEG, parm1 },
  { 90, "zero.alt",   sIN_CSEG, parm0 },
  { 89, "zero.pri",   sIN_CSEG, parm0 },
  { 92, "zero.s",     sIN_CSEG, parm1 },
};

static int findopcode(char *instr,int maxlen)
{
  /* case insensitive hash lookup (the opcode list is static); the names are
   * kept in lower case copies so that no locale-aware compare is needed */
  static short table[512];
  static char lname[sizeof opcodelist/sizeof opcodelist[0]][MAX_INSTR_LEN];
  static int ready=0;
  int i,len;
  unsigned h;
  char str[MAX_INSTR_LEN];

  if (maxlen>=MAX_INSTR_LEN)
    return 0;
  if (!ready) {
    memset(table,0,sizeof table);
    for (i=1; i<(int)(sizeof opcodelist/sizeof opcodelist[0]); i++) {
      const char *p=opcodelist[i].name;
      int n;
      for (n=0,h=5381; p[n]!='\0' && n<MAX_INSTR_LEN-1; n++) {
        lname[i][n]=(char)tolower((unsigned char)p[n]);
        h=h*33u+(unsigned char)lname[i][n];
      } /* for */
      lname[i][n]='\0';
      h&=511;
      while (table[h]!=0)
        h=(h+1)&511;
      table[h]=(short)i;
    } /* for */
    ready=1;
  } /* if */
  for (len=0,h=5381; len<maxlen; len++) {
    unsigned char ch=(unsigned char)instr[len];
    if (ch=='\0')
      break;
    ch=(unsigned char)tolower(ch);
    str[len]=(char)ch;
    h=h*33u+ch;
  } /* for */
  str[len]='\0';
  h&=511;
  while (table[h]!=0) {
    if (strcmp(str,lname[table[h]])==0)
      return table[h];
    h=(h+1)&511;
  } /* while */
  return 0;             /* not found, return special index */
}

static void append_amx_entry(unsigned char *metadata,long *stubofs,long *nameofs,
                             ucell address,const char *name)
{
  AMX_FUNCSTUBNT entry;
  size_t length=strlen(name)+1;

  entry.address=address;
  entry.nameofs=*nameofs;
  #if BYTE_ORDER==BIG_ENDIAN
    align32(&entry.address);
    align32(&entry.nameofs);
  #endif
  memcpy(metadata+*stubofs,&entry,sizeof entry);
  memcpy(metadata+*nameofs,name,length);
  *stubofs+=sizeof entry;
  *nameofs+=length;
}

static char *read_asm_line(FILE *fin,char *line,int size)
{
  char *result;
  PC_PROFILE_BEGIN(asm_read);
  result=pc_readasm(fin,line,size);
  PC_PROFILE_END(asm_read,result!=NULL);
  return result;
}

static int read_asm_opcode(char *instr,char **params)
{
  char *end;
  int op;
  PC_PROFILE_BEGIN(asm_opcode);
  for (end=instr; *end!='\0' && !isspace((unsigned char)*end); end++)
    /* nothing */;
  op=findopcode(instr,(int)(end-instr));
  *params=end;
  PC_PROFILE_END(asm_opcode,1);
  return op;
}

static cell call_asm_handler(int op,FILE *fout,char *params)
{
  cell size;
  PC_PROFILE_BEGIN(asm_operands);
  size=opcodelist[op].func(fout,skipwhitespace(params),opcodelist[op].opcode);
  PC_PROFILE_END(asm_operands,1);
  return size;
}

#if !defined SC_LIGHT && !defined __MSDOS__
  #define HAS_ASM_INDEX
  /* Four bytes per instruction. Cap the optional index across both segments. */
  #define ASM_INDEX_LIMIT (16u*1024u*1024u/sizeof(uint32_t))

typedef struct {
  uint32_t *records;
  size_t count,capacity;
  #if defined PC_TIMING
    size_t instructions;
  #endif
  long previous;
} asm_index;

static asm_index code_records,data_records;
static int asm_index_ready;
static short asm_index_file;

static int push_asm_record(asm_index *index,uint32_t record)
{
  if (index->count==index->capacity) {
    size_t other=(index==&code_records ? data_records.capacity : code_records.capacity);
    size_t capacity=index->capacity ? index->capacity+index->capacity/2 : 1024;
    uint32_t *records;
    if (capacity>ASM_INDEX_LIMIT-other)
      capacity=ASM_INDEX_LIMIT-other;
    if (capacity<=index->capacity)
      return FALSE;
    records=(uint32_t*)realloc(index->records,capacity*sizeof(*records));
    if (records==NULL)
      return FALSE;
    index->records=records;
    index->capacity=capacity;
  }
  index->records[index->count++]=record;
  return TRUE;
}

static int index_asm_line(asm_index *index,long offset,unsigned int column,unsigned int op)
{
  long delta=offset-index->previous;
  if (column>255 || op>255)
    return FALSE;
  /* An opcode of zero advances over a long gap without emitting anything. */
  while (delta>65535) {
    if (!push_asm_record(index,65535))
      return FALSE;
    delta-=65535;
  }
  index->previous=offset;
  #if defined PC_TIMING
    index->instructions++;
  #endif
  return push_asm_record(index,(uint32_t)delta | ((uint32_t)column<<16) | ((uint32_t)op<<24));
}
#endif

SC_FUNC void clear_asm_index(void)
{
  #if defined HAS_ASM_INDEX
    free(code_records.records);
    free(data_records.records);
    memset(&code_records,0,sizeof code_records);
    memset(&data_records,0,sizeof data_records);
    asm_index_ready=FALSE;
  #endif
  free(lbltab);
  lbltab=NULL;
}

#if defined HAS_ASM_INDEX
static int prepare_asm_index(FILE *fin,char *line,int line_size)
{
  long offset;
  memfile_t *memory=(memfile_t*)fin;
  cell width[sizeof opcodelist/sizeof opcodelist[0]];
  size_t i;
  PC_PROFILE_COUNTER(lines);
  PC_PROFILE_BEGIN(asm_labels);

  for (i=1; i<sizeof width/sizeof width[0]; i++) {
    OPCODE_PROC fn=opcodelist[i].func;
    width[i]=-1;
    if (fn==parm0) width[i]=opcodes(1);
    else if (fn==parm1 || fn==do_call || fn==do_jump || fn==do_switch) width[i]=opcodes(1)+opargs(1);
    else if (fn==parm2) width[i]=opcodes(1)+opargs(2);
    else if (fn==parm3) width[i]=opcodes(1)+opargs(3);
    else if (fn==parm4) width[i]=opcodes(1)+opargs(4);
    else if (fn==parm5) width[i]=opcodes(1)+opargs(5);
    else if (fn==do_case) width[i]=opargs(2);
    else if (fn==noop) width[i]=0;
  }
  lbltab=(cell*)malloc((size_t)sc_labnum*sizeof(*lbltab));
  if (lbltab==NULL)
    return FALSE;
  codeindex=0;
  pc_resetasm(fin);
  for (offset=memory->offs; read_asm_line(fin,line,line_size)!=NULL; offset=memory->offs) {
    char *instr=skipwhitespace(line),*params;
    int op;
    PC_PROFILE_INCREMENT(lines);
    if (*instr=='\0' || *instr==';')
      continue;
    stripcomment(instr);
    if ((*instr=='l' || *instr=='L') && instr[1]=='.') {
      int label=(int)hex2long(instr+2,NULL);
      assert(label>=0 && label<sc_labnum);
      lbltab[label]=codeindex;
      continue;
    }
    op=read_asm_opcode(instr,&params);
    if (op==0) {
      *params='\0';
      error(104,instr);
    }
    if (opcodelist[op].segment==sIN_CSEG) {
      if (!index_asm_line(&code_records,offset,(unsigned int)(params-line),(unsigned int)op))
        return FALSE;
      /* Fixed-width instructions don't need their operands during relocation. */
      codeindex+=width[op]>=0 ? width[op] : call_asm_handler(op,NULL,params);
    } else if (opcodelist[op].segment==sIN_DSEG) {
      if (!index_asm_line(&data_records,offset,(unsigned int)(params-line),(unsigned int)op))
        return FALSE;
    }
  }
  asm_index_file=fcurrent;
  PC_PROFILE_END(asm_labels,lines);
  {
    PC_PROFILE_BEGIN(asm_index);
    PC_PROFILE_END(asm_index,(code_records.capacity+data_records.capacity)*sizeof(uint32_t));
  }
  return TRUE;
}

static void write_asm_index(FILE *fout,FILE *fin,const asm_index *index,char *line,int line_size)
{
  size_t i;
  long offset=0;
  memfile_t *memory=(memfile_t*)fin;
  for (i=0; i<index->count; i++) {
    uint32_t record=index->records[i];
    unsigned int op=record>>24;
    char *params=line;
    offset+=(long)(record & 65535);
    if (op==0)
      continue;
    if (opcodelist[op].func==parm0 || opcodelist[op].func==noop) {
      line[0]='\0';
    } else {
      memory->offs=offset;
      read_asm_line(fin,line,line_size);
      params+=((record>>16) & 255);
      stripcomment(params);
    }
    call_asm_handler(op,fout,params);
  }
}
#endif

SC_FUNC int assemble(FILE *fout,FILE *fin)
{
  AMX_HEADER hdr;
  int numpublics,numnatives,numlibraries,numpubvars,numtags,padding;
  uint64_t nametablesize,tableend,codeoffset,dataoffset,heapoffset;
  long stubofs,nameofs,filesize;
  #if PAWN_CELL_SIZE > 32
    char line[512];
  #else
    char line[256];
  #endif
  char *instr,*params;
  int i,pass,size;
  unsigned char data_initial[UCHAR_MAX+1];
  int16_t maxname;
  symbol *sym, **nativelist;
  constvalue *constptr;
  cell mainaddr;
  unsigned char *metadata;
  PC_PROFILE_COUNTER(instructions);
  PC_PROFILE_BEGIN(assemble);
  PC_PROFILE_BEGIN(amx);


  /* if compression failed, restart the assembly with compaction switched off */
  if (setjmp(compact_err)!=0) {
    assert(sc_compress);  /* cannot arrive here if compact encoding was disabled */
    #if defined HAS_ASM_INDEX
      if (!asm_index_ready)
    #endif
        clear_asm_index();
    sc_compress=FALSE;
    pc_resetbin(fout,0);
    error(232);           /* disabled compact encoding */
  } /* if */

  #if !defined NDEBUG
    /* verify that the opcode list is sorted (skip entry 1; it is reserved
     * for a non-existant opcode)
     */
    assert(opcodelist[1].name!=NULL);
    for (i=2; i<(sizeof opcodelist / sizeof opcodelist[0]); i++) {
      assert(opcodelist[i].name!=NULL);
      assert(stricmp(opcodelist[i].name,opcodelist[i-1].name)>0);
    } /* for */
  #endif

  writeerror=FALSE;
  nametablesize=sizeof(int16_t);
  numpublics=0;
  numnatives=0;
  numpubvars=0;
  mainaddr=-1;
  /* count number of public and native functions and public variables */
  for (sym=global_symbol_table()->next; sym!=NULL; sym=sym->next) {
    int match=0;
    if (sym->ident==iFUNCTN) {
      if ((sym->usage & uNATIVE)!=0 && (sym->usage & uREAD)!=0 && sym->addr>=0)
        match=++numnatives;
      if ((sym->usage & uPUBLIC)!=0 && (sym->usage & uDEFINE)!=0)
        match=++numpublics;
      if (strcmp(sym->name,uMAINFUNC)==0) {
        assert(sym->vclass==sGLOBAL);
        mainaddr=sym->addr;
      } /* if */
    } else if (sym->ident==iVARIABLE) {
      if ((sym->usage & uPUBLIC)!=0
          && ((sym->usage & uSTOCK)==0 || (sym->usage & (uREAD | uWRITTEN))!=0))
        match=++numpubvars;
    } /* if */
    if (match) {
      char alias[sNAMEMAX+1];
      assert(sym!=NULL);
      if ((sym->usage & uNATIVE)==0 || !lookup_alias(alias,sym->name)) {
        assert(strlen(sym->name)<=sNAMEMAX);
        strcpy(alias,sym->name);
      } /* if */
      nametablesize+=strlen(alias)+1;
    } /* if */
  } /* for */
  assert(numnatives==ntv_funcid);

  /* count number of libraries */
  numlibraries=0;
  if (pc_addlibtable) {
    for (constptr=libname_tab.first; constptr!=NULL; constptr=constptr->next) {
      if (constptr->value>0) {
        assert(strlen(constptr->name)>0);
        numlibraries++;
        nametablesize+=strlen(constptr->name)+1;
      } /* if */
    } /* for */
  } /* if */

  /* count number of public tags */
  numtags=0;
  for (constptr=tagname_tab.first; constptr!=NULL; constptr=constptr->next) {
    if ((constptr->value & PUBLICTAG)!=0) {
      assert(strlen(constptr->name)>0);
      numtags++;
      nametablesize+=strlen(constptr->name)+1;
    } /* if */
  } /* for */

  /* Align the end of the metadata, including all stubs and names. */
  tableend=sizeof hdr+((uint64_t)numpublics+numnatives+numlibraries+numpubvars+numtags)
          *sizeof(AMX_FUNCSTUBNT);
  assert(sc_dataalign!=0);
  padding=(int)(sc_dataalign-(tableend+nametablesize)%sc_dataalign);
  if (padding==sc_dataalign)
    padding=0;
  codeoffset=tableend+nametablesize+padding;
  if (code_idx<0 || glb_declared<0 || pc_stksize<0
      || codeoffset>INT32_MAX || code_idx>INT32_MAX
      || glb_declared>INT32_MAX/sizeof(cell) || pc_stksize>INT32_MAX/sizeof(cell))
    error(105);
  dataoffset=codeoffset+(uint64_t)code_idx;
  heapoffset=dataoffset+(uint64_t)glb_declared*sizeof(cell);
  if (heapoffset+(uint64_t)pc_stksize*sizeof(cell)>INT32_MAX)
    error(105);

  /* write the abstract machine header */
  memset(&hdr, 0, sizeof hdr);
  hdr.magic=(unsigned short)AMX_MAGIC;
  hdr.file_version=(char)((pc_optimize<=sOPTIMIZE_NOMACRO) ? MAX_FILE_VER_JIT : CUR_FILE_VERSION);
  hdr.amx_version=(char)((pc_optimize<=sOPTIMIZE_NOMACRO) ? MIN_AMX_VER_JIT : MIN_AMX_VERSION);
  hdr.flags=(short)(sc_debug & sSYMBOLIC);
  if (sc_compress)
    hdr.flags|=AMX_FLAG_COMPACT;
  if (sc_debug==0)
    hdr.flags|=AMX_FLAG_NOCHECKS;
  if (pc_memflags & suSLEEP_INSTR)
    hdr.flags|=AMX_FLAG_SLEEP;
  hdr.defsize=sizeof(AMX_FUNCSTUBNT);
  hdr.publics=sizeof hdr; /* public table starts right after the header */
  hdr.natives=hdr.publics + numpublics*sizeof(AMX_FUNCSTUBNT);
  hdr.libraries=hdr.natives + numnatives*sizeof(AMX_FUNCSTUBNT);
  hdr.pubvars=hdr.libraries + numlibraries*sizeof(AMX_FUNCSTUBNT);
  hdr.tags=hdr.pubvars + numpubvars*sizeof(AMX_FUNCSTUBNT);
  hdr.nametable=hdr.tags + numtags*sizeof(AMX_FUNCSTUBNT);
  hdr.cod=(int32_t)codeoffset;
  hdr.dat=(int32_t)dataoffset;
  hdr.hea=(int32_t)heapoffset;
  hdr.stp=(int32_t)(heapoffset+(uint64_t)pc_stksize*sizeof(cell));
  hdr.cip=mainaddr;
  hdr.size=hdr.hea; /* preset, this is incorrect in case of compressed output */
  /* Buffer the tables and names so they can be written in file order. The
   * header itself is filled in after the final code size is known. */
  metadata=(unsigned char *)calloc((size_t)hdr.cod,1);
  if (metadata==NULL)
    error(103);
  stubofs=hdr.publics;
  nameofs=hdr.nametable+sizeof(int16_t);

  /* write the public functions table */
  for (sym=global_symbol_table()->next; sym!=NULL; sym=sym->next) {
    if (sym->ident==iFUNCTN
        && (sym->usage & uPUBLIC)!=0 && (sym->usage & uDEFINE)!=0)
    {
      assert(sym->vclass==sGLOBAL);
      append_amx_entry(metadata,&stubofs,&nameofs,sym->addr,sym->name);
    } /* if */
  } /* for */

  /* write the natives table */
  /* The native functions must be written in sorted order. (They are
   * sorted on their "id", not on their name). A nested loop to find
   * each successive function would be an O(n^2) operation. But we
   * do not really need to sort, because the native function id's
   * are sequential and there are no duplicates. So we first walk
   * through the complete symbol list and store a pointer to every
   * native function of interest in a temporary table, where its id
   * serves as the index in the table. Now we can walk the table and
   * have all native functions in sorted order.
   */
  if (numnatives>0) {
    nativelist=(symbol **)malloc(numnatives*sizeof(symbol *));
    if (nativelist==NULL) {
      free(metadata);
      error(103);               /* insufficient memory */
    }
    #if !defined NDEBUG
      memset(nativelist,0,numnatives*sizeof(symbol *)); /* for NULL checking */
    #endif
    for (sym=global_symbol_table()->next; sym!=NULL; sym=sym->next) {
      if (sym->ident==iFUNCTN && (sym->usage & uNATIVE)!=0 && (sym->usage & uREAD)!=0 && sym->addr>=0) {
        assert(sym->addr < numnatives);
        nativelist[(int)sym->addr]=sym;
      } /* if */
    } /* for */
    for (i=0; i<numnatives; i++) {
      char alias[sNAMEMAX+1];
      sym=nativelist[i];
      assert(sym!=NULL);
      if (!lookup_alias(alias,sym->name)) {
        assert(strlen(sym->name)<=sNAMEMAX);
        strcpy(alias,sym->name);
      } /* if */
      assert(sym->vclass==sGLOBAL);
      append_amx_entry(metadata,&stubofs,&nameofs,0,alias);
    } /* for */
    free(nativelist);
  } /* if */

  /* write the libraries table */
  if (pc_addlibtable) {
    for (constptr=libname_tab.first; constptr!=NULL; constptr=constptr->next) {
      if (constptr->value>0) {
        assert(strlen(constptr->name)>0);
        append_amx_entry(metadata,&stubofs,&nameofs,0,constptr->name);
      } /* if */
    } /* for */
  } /* if */

  /* write the public variables table */
  for (sym=global_symbol_table()->next; sym!=NULL; sym=sym->next) {
    if (sym->ident==iVARIABLE && (sym->usage & uPUBLIC)!=0) {
      if ((sym->usage & uSTOCK)!=0 && (sym->usage & (uREAD | uWRITTEN))==0)
        continue;
      assert((sym->usage & uDEFINE)!=0);
      assert(sym->vclass==sGLOBAL);
      append_amx_entry(metadata,&stubofs,&nameofs,sym->addr,sym->name);
    } /* if */
  } /* for */

  /* write the public tagnames table */
  for (constptr=tagname_tab.first; constptr!=NULL; constptr=constptr->next) {
    if ((constptr->value & PUBLICTAG)!=0) {
      assert(strlen(constptr->name)>0);
      append_amx_entry(metadata,&stubofs,&nameofs,constptr->value & TAGMASK,constptr->name);
    } /* if */
  } /* for */

  /* write the "maximum name length" field in the name table */
  assert(stubofs==hdr.nametable);
  assert((uint64_t)nameofs==tableend+nametablesize);
  maxname=sNAMEMAX;
  #if BYTE_ORDER==BIG_ENDIAN
    align16(&maxname);
  #endif
  memcpy(metadata+hdr.nametable,&maxname,sizeof maxname);
  writeerror=!pc_writebin(fout,metadata,hdr.cod);
  free(metadata);
  PC_PROFILE_END(amx,hdr.cod);
  if (writeerror)
    error(101,binfname);

  #if defined HAS_ASM_INDEX
    if (!asm_index_ready && sc_labnum>0 && ((memfile_t*)fin)->usedoffs>=16384) {
      asm_index_ready=prepare_asm_index(fin,line,sizeof line);
      if (!asm_index_ready)
        clear_asm_index();
    }
    if (asm_index_ready) {
      fcurrent=asm_index_file;
      bytes_in=bytes_out=0;
      {
        PC_PROFILE_BEGIN(asm_code);
        write_asm_index(fout,fin,&code_records,line,sizeof line);
        PC_PROFILE_END(asm_code,code_records.count);
      }
      {
        PC_PROFILE_BEGIN(asm_data);
        write_asm_index(fout,fin,&data_records,line,sizeof line);
        PC_PROFILE_END(asm_data,data_records.count);
      }
      #if defined PC_TIMING
        instructions=code_records.instructions+data_records.instructions;
      #endif
    } else
  #endif
  {
  /* First pass: relocate all labels */
  /* This pass is necessary because the code addresses of labels is only known
   * after the peephole optimization flag. Labels can occur inside expressions
   * (e.g. the conditional operator), which are optimized.
   */
  lbltab=NULL;
  if (sc_labnum>0) {
    /* only very short programs have zero labels; no first pass is needed
     * if there are no labels */
    lbltab=(cell *)malloc(sc_labnum*sizeof(cell));
    if (lbltab==NULL)
      error(103);               /* insufficient memory */
    PC_PROFILE_COUNTER(lines);
    PC_PROFILE_BEGIN(asm_labels);
    codeindex=0;
    pc_resetasm(fin);
    while (read_asm_line(fin,line,sizeof line)!=NULL) {
      PC_PROFILE_INCREMENT(lines);
      stripcomment(line);
      instr=skipwhitespace(line);
      /* ignore empty lines */
      if (*instr=='\0')
        continue;
      if ((*instr=='l' || *instr=='L') && *(instr+1)=='.') {
        int lindex=(int)hex2long(instr+2,NULL);
        assert(lindex>=0 && lindex<sc_labnum);
        lbltab[lindex]=codeindex;
      } else {
        /* get to the end of the instruction (make use of the '\n' that fgets()
         * added at the end of the line; this way we will *always* drop on a
         * whitespace character) */
        i=read_asm_opcode(instr,&params);
        if (opcodelist[i].name==NULL) {
          *params='\0';
          error(104,instr);     /* invalid assembler instruction */
        } /* if */
        if (opcodelist[i].segment==sIN_CSEG)
          codeindex+=call_asm_handler(i,NULL,params);
      } /* if */
    } /* while */
    PC_PROFILE_END(asm_labels,lines);
  } /* if */

  /* The code scan validates every opcode. In the data scan, skip lines whose
   * first byte cannot start a data opcode before parsing the instruction. */
  memset(data_initial,0,sizeof data_initial);
  for (i=1; i<(int)(sizeof opcodelist/sizeof opcodelist[0]); i++) {
    if (opcodelist[i].segment==sIN_DSEG) {
      unsigned char first=(unsigned char)opcodelist[i].name[0];
      data_initial[(unsigned char)tolower(first)]=1;
      data_initial[(unsigned char)toupper(first)]=1;
    }
  }
  bytes_in=0;
  bytes_out=0;
  for (pass=sIN_CSEG; pass<=sIN_DSEG; pass++) {
    PC_PROFILE_COUNTER(lines);
    PC_PROFILE_BEGIN(asm_code);
    PC_PROFILE_BEGIN(asm_data);
    pc_resetasm(fin);
    while (read_asm_line(fin,line,sizeof line)!=NULL) {
      PC_PROFILE_INCREMENT(lines);
      instr=skipwhitespace(line);
      if (pass==sIN_DSEG && !data_initial[(unsigned char)*instr])
        continue;
      /* Comments, labels and empty lines carry no segment instructions. */
      if (*instr=='\0' || *instr==';' || ((*instr=='l' || *instr=='L') && instr[1]=='.'))
        continue;
      stripcomment(instr);
      /* get to the end of the instruction (make use of the '\n' that fgets()
       * added at the end of the line; this way we will *always* drop on a
       * whitespace character) */
        i=read_asm_opcode(instr,&params);
      if (opcodelist[i].name==NULL)
        error(104, stripwhitespace(instr)); /* invalid assembler instruction */
      if (opcodelist[i].segment==pass) {
        PC_PROFILE_INCREMENT(instructions);
        call_asm_handler(i,fout,params);
      }
    } /* while */
    if (pass==sIN_CSEG)
      PC_PROFILE_END(asm_code,lines);
    else
      PC_PROFILE_END(asm_data,lines);
  } /* for */
  }
  if (bytes_out-bytes_in>0)
    error(105);         /* compression buffer overflow */

  clear_asm_index();

  if (sc_compress) {
    filesize=pc_lengthbin(fout);/* get this value before appending debug info */
    if (filesize<hdr.cod)
      error(101,binfname);
    if (filesize>INT32_MAX)
      error(105);
    hdr.size=(int32_t)filesize;
  }
  if (!writeerror && (hdr.flags & AMX_FLAG_DEBUG)!=0 && !append_dbginfo(fout)) {
    hdr.flags&=~AMX_FLAG_DEBUG;
    error(240);
  }

  if (writeerror)
    error(101,binfname);

  /* adjust the header */
  size=(int)hdr.cod;    /* save, the value in the header may need to be swapped */
  #if BYTE_ORDER==BIG_ENDIAN
    align32(&hdr.size);
    align16(&hdr.magic);
    align16(&hdr.flags);
    align16(&hdr.defsize);
    align32(&hdr.publics);
    align32(&hdr.natives);
    align32(&hdr.libraries);
    align32(&hdr.pubvars);
    align32(&hdr.tags);
    align32(&hdr.nametable);
    align32(&hdr.cod);
    align32(&hdr.dat);
    align32(&hdr.hea);
    align32(&hdr.stp);
    align32(&hdr.cip);
  #endif
  pc_resetbin(fout,0);
  if (!pc_writebin(fout,&hdr,sizeof hdr))
    error(101,binfname);
  pc_resetbin(fout,0);          /* detect deferred output errors */
  PC_PROFILE_END(assemble,instructions);

  /* return the size of the header (including name tables, but excluding code
   * or data sections)
   */
  return size;
}

static int append_dbginfo(FILE *fout)
{
  AMX_DBG_HDR dbghdr;
  AMX_DBG_LINE dbgline;
  AMX_DBG_SYMBOL dbgsym;
  AMX_DBG_SYMDIM dbgidxtag[sDIMEN_MAX];
  int index,dim,dbgsymdim;
  char *str,*prevstr,*name,*prevname;
  ucell codeidx,previdx;
  constvalue *constptr;
  char symname[2*sNAMEMAX+16];
  int16_t id1,id2;
  ucell address;
  uint64_t debugsize=sizeof dbghdr;

  /* header with general information */
  memset(&dbghdr, 0, sizeof dbghdr);
  dbghdr.magic=AMX_DBG_MAGIC;
  dbghdr.file_version=CUR_FILE_VERSION;
  dbghdr.amx_version=MIN_AMX_VERSION;

  /* first pass: collect the number of items in various tables */

  /* file table */
  previdx=0;
  prevstr=NULL;
  prevname=NULL;
  for (index=0; (str=get_dbgstring(index))!=NULL; index++) {
    assert(str!=NULL);
    assert(str[0]!='\0' && str[1]==':');
    if (str[0]=='F') {
      codeidx=hex2long(str+2,&name);
      if (codeidx!=previdx) {
        if (prevstr!=NULL) {
          assert(prevname!=NULL);
          if (dbghdr.files==INT16_MAX)
            return FALSE;
          dbghdr.files++;
          debugsize+=sizeof(AMX_DBG_FILE)+strlen(prevname);
        } /* if */
        previdx=codeidx;
      } /* if */
      prevstr=str;
      prevname=skipwhitespace(name);
    } /* if */
  } /* for */
  if (prevstr!=NULL) {
    assert(prevname!=NULL);
    if (dbghdr.files==INT16_MAX)
      return FALSE;
    dbghdr.files++;
    debugsize+=sizeof(AMX_DBG_FILE)+strlen(prevname);
  } /* if */

  /* line number table */
  for (index=0; (str=get_dbgstring(index))!=NULL; index++) {
    assert(str!=NULL);
    assert(str[0]!='\0' && str[1]==':');
    if (str[0]=='L') {
      if (dbghdr.lines==INT16_MAX)
        return FALSE;
      dbghdr.lines++;
      debugsize+=sizeof(AMX_DBG_LINE);
    } /* if */
  } /* for */

  /* symbol table */
  for (index=0; (str=get_dbgstring(index))!=NULL; index++) {
    assert(str!=NULL);
    assert(str[0]!='\0' && str[1]==':');
    if (str[0]=='S') {
      if (dbghdr.symbols==INT16_MAX)
        return FALSE;
      dbghdr.symbols++;
      name=skipwhitespace(strchr(str+2,':')+1);
      assert(name!=NULL);
      str=strchr(name,' ');
      assert(str!=NULL);
      assert((int)(str-name)<sizeof symname);
      debugsize+=sizeof(AMX_DBG_SYMBOL)+(str-name);
      if ((prevstr=strchr(name,'['))!=NULL)
        while ((prevstr=strchr(prevstr+1,':'))!=NULL)
          debugsize+=sizeof(AMX_DBG_SYMDIM);
    } /* if */
  } /* for */

  /* tag table */
  for (constptr=tagname_tab.first; constptr!=NULL; constptr=constptr->next) {
    assert(strlen(constptr->name)>0);
    if (dbghdr.tags==INT16_MAX)
      return FALSE;
    dbghdr.tags++;
    debugsize+=sizeof(AMX_DBG_TAG)+strlen(constptr->name);
  } /* for */

  /* automaton table */
  for (constptr=sc_automaton_tab.first; constptr!=NULL; constptr=constptr->next) {
    assert(constptr->index==0 && strlen(constptr->name)==0 || strlen(constptr->name)>0);
    if (dbghdr.automatons==INT16_MAX)
      return FALSE;
    dbghdr.automatons++;
    debugsize+=sizeof(AMX_DBG_MACHINE)+strlen(constptr->name);
  } /* for */

  /* state table */
  for (constptr=sc_state_tab.first; constptr!=NULL; constptr=constptr->next) {
    assert(strlen(constptr->name)>0);
    if (dbghdr.states==INT16_MAX)
      return FALSE;
    dbghdr.states++;
    debugsize+=sizeof(AMX_DBG_STATE)+strlen(constptr->name);
  } /* for */


  if (debugsize>INT32_MAX)
    return FALSE;
  dbghdr.size=(int32_t)debugsize;

  /* pass 2: generate the tables */
  #if BYTE_ORDER==BIG_ENDIAN
    align32((uint32_t*)&dbghdr.size);
    align16(&dbghdr.magic);
    align16(&dbghdr.flags);
    align16(&dbghdr.files);
    align16(&dbghdr.lines);
    align16(&dbghdr.symbols);
    align16(&dbghdr.tags);
    align16(&dbghdr.automatons);
    align16(&dbghdr.states);
  #endif
  writeerror |= !pc_writebin(fout,&dbghdr,sizeof dbghdr);

  /* file table */
  previdx=0;
  prevstr=NULL;
  prevname=NULL;
  for (index=0; (str=get_dbgstring(index))!=NULL; index++) {
    assert(str!=NULL);
    assert(str[0]!='\0' && str[1]==':');
    if (str[0]=='F') {
      codeidx=hex2long(str+2,&name);
      if (codeidx!=previdx) {
        if (prevstr!=NULL) {
          assert(prevname!=NULL);
          #if BYTE_ORDER==BIG_ENDIAN
            aligncell(&previdx);
          #endif
          writeerror |= !pc_writebin(fout,&previdx,sizeof previdx);
          writeerror |= !pc_writebin(fout,prevname,strlen(prevname)+1);
        } /* if */
        previdx=codeidx;
      } /* if */
      prevstr=str;
      prevname=skipwhitespace(name);
    } /* if */
  } /* for */
  if (prevstr!=NULL) {
    assert(prevname!=NULL);
    #if BYTE_ORDER==BIG_ENDIAN
      aligncell(&previdx);
    #endif
    writeerror |= !pc_writebin(fout,&previdx,sizeof previdx);
    writeerror |= !pc_writebin(fout,prevname,strlen(prevname)+1);
  } /* if */

  /* line number table */
  for (index=0; (str=get_dbgstring(index))!=NULL; index++) {
    assert(str!=NULL);
    assert(str[0]!='\0' && str[1]==':');
    if (str[0]=='L') {
      dbgline.address=hex2long(str+2,&str);
      dbgline.line=(int32_t)hex2long(str,NULL);
      #if BYTE_ORDER==BIG_ENDIAN
        aligncell(&dbgline.address);
        align32(&dbgline.line);
      #endif
      writeerror |= !pc_writebin(fout,&dbgline,sizeof dbgline);
    } /* if */
  } /* for */

  /* symbol table */
  for (index=0; (str=get_dbgstring(index))!=NULL; index++) {
    assert(str!=NULL);
    assert(str[0]!='\0' && str[1]==':');
    if (str[0]=='S') {
      dbgsym.address=hex2long(str+2,&str);
      dbgsym.tag=(int16_t)hex2long(str,&str);
      str=skipwhitespace(str);
      assert(*str==':');
      name=skipwhitespace(str+1);
      str=strchr(name,' ');
      assert(str!=NULL);
      assert((int)(str-name)<sizeof symname);
      strlcpy(symname,name,(int)(str-name)+1);
      dbgsym.codestart=hex2long(str,&str);
      dbgsym.codeend=hex2long(str,&str);
      dbgsym.ident=(char)hex2long(str,&str);
      dbgsym.vclass=(char)hex2long(str,&str);
      dbgsym.dim=0;
      str=skipwhitespace(str);
      if (*str=='[') {
        while (*(str=skipwhitespace(str+1))!=']') {
          dbgidxtag[dbgsym.dim].tag=(int16_t)hex2long(str,&str);
          str=skipwhitespace(str);
          assert(*str==':');
          dbgidxtag[dbgsym.dim].size=hex2long(str+1,&str);
          dbgsym.dim++;
        } /* while */
      } /* if */
      dbgsymdim = dbgsym.dim;
      #if BYTE_ORDER==BIG_ENDIAN
        aligncell(&dbgsym.address);
        align16(&dbgsym.tag);
        aligncell(&dbgsym.codestart);
        aligncell(&dbgsym.codeend);
        align16(&dbgsym.dim);
      #endif
      writeerror |= !pc_writebin(fout,&dbgsym,offsetof(AMX_DBG_SYMBOL, name));
      writeerror |= !pc_writebin(fout,symname,strlen(symname)+1);
      for (dim=0; dim<dbgsymdim; dim++) {
        #if BYTE_ORDER==BIG_ENDIAN
          align16(&dbgidxtag[dim].tag);
          aligncell(&dbgidxtag[dim].size);
        #endif
        writeerror |= !pc_writebin(fout,&dbgidxtag[dim],sizeof dbgidxtag[dim]);
      } /* for */
    } /* if */
  } /* for */

  /* tag table */
  for (constptr=tagname_tab.first; constptr!=NULL; constptr=constptr->next) {
    assert(strlen(constptr->name)>0);
    id1=(int16_t)(constptr->value & TAGMASK);
    #if BYTE_ORDER==BIG_ENDIAN
      align16(&id1);
    #endif
    writeerror |= !pc_writebin(fout,&id1,sizeof id1);
    writeerror |= !pc_writebin(fout,constptr->name,strlen(constptr->name)+1);
  } /* for */

  /* automaton table */
  for (constptr=sc_automaton_tab.first; constptr!=NULL; constptr=constptr->next) {
    assert(constptr->index==0 && strlen(constptr->name)==0 || strlen(constptr->name)>0);
    id1=(int16_t)constptr->index;
    address=(ucell)constptr->value;
    #if BYTE_ORDER==BIG_ENDIAN
      align16(&id1);
      aligncell(&address);
    #endif
    writeerror |= !pc_writebin(fout,&id1,sizeof id1);
    writeerror |= !pc_writebin(fout,&address,sizeof address);
    writeerror |= !pc_writebin(fout,constptr->name,strlen(constptr->name)+1);
  } /* for */

  /* state table */
  for (constptr=sc_state_tab.first; constptr!=NULL; constptr=constptr->next) {
    assert(strlen(constptr->name)>0);
    id1=(int16_t)constptr->value;
    id2=(int16_t)constptr->index;
    address=(ucell)constptr->value;
    #if BYTE_ORDER==BIG_ENDIAN
      align16(&id1);
      align16(&id2);
    #endif
    writeerror |= !pc_writebin(fout,&id1,sizeof id1);
    writeerror |= !pc_writebin(fout,&id2,sizeof id2);
    writeerror |= !pc_writebin(fout,constptr->name,strlen(constptr->name)+1);
  } /* for */

  delete_dbgstringtable();
  return TRUE;
}
