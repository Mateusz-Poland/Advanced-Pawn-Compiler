/*  Pawn compiler  - maintenance of various lists
 *
 *  o  Name list (aliases)
 *  o  Include path list
 *  o  Macro definitions (text substitutions)
 *  o  Documentation tags and automatic listings
 *  o  Debug strings
 *
 *  Copyright (c) ITB CompuPhase, 2001-2006
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
 *  Version: $Id: sclist.c 3660 2006-11-05 13:05:09Z thiadmer $
 */
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "sc.h"

#if defined FORTIFY
  #include <alloc/fortify.h>
#endif

/* a "private" implementation of strdup(), so that porting
 * to other memory allocators becomes easier.
 * By S�ren Hannibal.
 */
SC_FUNC char* duplicatestring(const char* sourcestring)
{
  char* result=(char*)malloc(strlen(sourcestring)+1);
  strcpy(result,sourcestring);
  return result;
}


static stringpair *insert_stringpair(stringpair *root,char *first,char *second,int matchlength)
{
  stringpair *cur,*pred;

  assert(root!=NULL);
  assert(first!=NULL);
  assert(second!=NULL);
  /* create a new node, and check whether all is okay */
  if ((cur=(stringpair*)malloc(sizeof(stringpair)))==NULL)
    return NULL;
  cur->first=duplicatestring(first);
  cur->second=duplicatestring(second);
  cur->matchlength=matchlength;
  if (cur->first==NULL || cur->second==NULL) {
    if (cur->first!=NULL)
      free(cur->first);
    if (cur->second!=NULL)
      free(cur->second);
    free(cur);
    return NULL;
  } /* if */
  /* link the node to the tree, find the position */
  for (pred=root; pred->next!=NULL && strcmp(pred->next->first,first)<0; pred=pred->next)
    /* nothing */;
  cur->next=pred->next;
  pred->next=cur;
  return cur;
}

static void delete_stringpairtable(stringpair *root)
{
  stringpair *cur, *next;

  assert(root!=NULL);
  cur=root->next;
  while (cur!=NULL) {
    next=cur->next;
    assert(cur->first!=NULL);
    assert(cur->second!=NULL);
    free(cur->first);
    free(cur->second);
    free(cur);
    cur=next;
  } /* while */
  memset(root,0,sizeof(stringpair));
}

static stringpair *find_stringpair(stringpair *cur,char *first,int matchlength)
{
  int result=0;

  assert(matchlength>0);  /* the function cannot handle zero-length comparison */
  assert(first!=NULL);
  while (cur!=NULL && result<=0) {
    result=(int)*cur->first - (int)*first;
    if (result==0 && matchlength==cur->matchlength) {
      result=strncmp(cur->first,first,matchlength);
      if (result==0)
        return cur;
    } /* if */
    cur=cur->next;
  } /* while */
  return NULL;
}

/* ----- string list functions ----------------------------------- */
static stringlist *insert_string(stringlist *list,char *string)
{
  stringlist newlist;

  assert(list!=NULL);
  assert(string!=NULL);
  if (list->data==NULL) {
    /* inserted for the first time */
    newlist.length=0;
    newlist.size=2;
    newlist.data=calloc(newlist.size,sizeof(char*));
    newlist.strings=newlist.data;
    if (newlist.data==NULL)
      error(103);       /* insufficient memory (fatal error) */
    *list=newlist;
  } else if (list->length==list->size ||
             list->length==list->data+list->size-list->strings) {
    /* allocate more elements */
    newlist.length=list->length;
    newlist.size=list->size+list->size/2;
    newlist.data=realloc(list->data,newlist.size*sizeof(char*));
    newlist.strings=newlist.data+(list->strings-list->data);
    if (newlist.data==NULL)
      error(103);       /* insufficient memory (fatal error) */
    memset(newlist.data+list->size,0,list->size/2*sizeof(char*));
    *list=newlist;
  } /* if */
  if ((list->strings[list->length]=duplicatestring(string))==NULL)
    error(103);         /* insufficient memory (fatal error) */
  list->length++;
  return list;
}

static char *get_string(stringlist *list,int index)
{
  assert(list!=NULL);
  if (index>=0 && index<list->length)
    return list->strings[index];
  return NULL;
}

static int delete_string(stringlist *list,int index)
{
  assert(list!=NULL);
  assert(index==0);
  if (list->length>0) {
    free(list->strings[0]);
    list->strings[0]=NULL;
    list->strings++;
    list->length--;
    return TRUE;
  } /* if */
  return FALSE;
}

SC_FUNC void delete_stringtable(stringlist *list)
{
  int i;

  assert(list!=NULL);
  for (i=0; i<list->size; i++)
    free(list->data[i]);
  free(list->data);
  memset(list,0,sizeof(stringlist));
}


/* ----- alias table --------------------------------------------- */
static stringpair alias_tab = {NULL, NULL, NULL};   /* alias table */

SC_FUNC stringpair *insert_alias(char *name,char *alias)
{
  stringpair *cur;

  assert(name!=NULL);
  assert(strlen(name)<=sNAMEMAX);
  assert(alias!=NULL);
  assert(strlen(alias)<=sNAMEMAX);
  if ((cur=insert_stringpair(&alias_tab,name,alias,strlen(name)))==NULL)
    error(103);       /* insufficient memory (fatal error) */
  return cur;
}

SC_FUNC int lookup_alias(char *target,char *name)
{
  stringpair *cur=find_stringpair(alias_tab.next,name,strlen(name));
  if (cur!=NULL) {
    assert(strlen(cur->second)<=sNAMEMAX);
    strcpy(target,cur->second);
  } /* if */
  return cur!=NULL;
}

SC_FUNC void delete_aliastable(void)
{
  delete_stringpairtable(&alias_tab);
}

/* ----- include paths list -------------------------------------- */
static stringlist includepaths = {NULL, NULL, 0, 0};  /* directory list for include files */

SC_FUNC stringlist *insert_path(char *path)
{
  clear_include_cache();
  return insert_string(&includepaths,path);
}

SC_FUNC char *get_path(int index)
{
  return get_string(&includepaths,index);
}

SC_FUNC void delete_pathtable(void)
{
  delete_stringtable(&includepaths);
  assert(includepaths.strings==NULL);
}


/* ----- text substitution patterns ------------------------------ */
#if !defined NO_DEFINE

typedef struct {
  stringpair pair;
  uint32_t hash;
} macro_definition;

static stringpair **macro_buckets;
static size_t macro_capacity,macro_count;

/* Hash only the identifier prefix, so function-like patterns and plain
 * definitions with the same name retain the existing redefinition rules. */
static uint32_t macro_hash(const char *name,int length)
{
  uint32_t hash=UINT32_C(2166136261);
  while (length-->0)
    hash=(hash^(unsigned char)*name++)*UINT32_C(16777619);
  return hash;
}

static void grow_macro_table(void)
{
  size_t i,capacity=macro_capacity ? macro_capacity*2 : 64;
  stringpair **buckets=(stringpair**)calloc(capacity,sizeof(*buckets));

  if (buckets==NULL)
    error(103);
  for (i=0; i<macro_capacity; i++) {
    stringpair *item=macro_buckets[i];
    while (item!=NULL) {
      stringpair *next=item->next;
      size_t slot=((macro_definition*)item)->hash & (capacity-1);
      item->next=buckets[slot];
      buckets[slot]=item;
      item=next;
    }
  }
  free(macro_buckets);
  macro_buckets=buckets;
  macro_capacity=capacity;
}

static stringpair **find_macro_slot(const char *name,int length)
{
  uint32_t hash=macro_hash(name,length);
  stringpair **slot=&macro_buckets[hash & (macro_capacity-1)];

  while (*slot!=NULL) {
    macro_definition *definition=(macro_definition*)*slot;
    if (definition->hash==hash && definition->pair.matchlength==length
        && memcmp(definition->pair.first,name,length)==0)
      break;
    slot=&definition->pair.next;
  }
  return slot;
}

SC_FUNC stringpair *insert_subst(char *pattern,char *substitution,int prefixlen)
{
  size_t pattern_size=strlen(pattern)+1;
  size_t substitution_size=strlen(substitution)+1;
  macro_definition *definition;
  stringpair *item;
  size_t slot;

  if (macro_count==macro_capacity)
    grow_macro_table();
  definition=(macro_definition*)malloc(sizeof(*definition)+pattern_size+substitution_size);
  if (definition==NULL)
    error(103);
  definition->hash=macro_hash(pattern,prefixlen);
  item=&definition->pair;
  item->first=(char*)(definition+1);
  item->second=item->first+pattern_size;
  item->matchlength=prefixlen;
  memcpy(item->first,pattern,pattern_size);
  memcpy(item->second,substitution,substitution_size);
  slot=definition->hash & (macro_capacity-1);
  item->next=macro_buckets[slot];
  macro_buckets[slot]=item;
  macro_count++;
  return item;
}

SC_FUNC stringpair *find_subst(char *name,int length)
{
  assert(length>0);
  return macro_count ? *find_macro_slot(name,length) : NULL;
}

SC_FUNC int delete_subst(char *name,int length)
{
  if (macro_count) {
    stringpair **slot=find_macro_slot(name,length);
    if (*slot!=NULL) {
      stringpair *item=*slot;
      *slot=item->next;
      free(item);
      macro_count--;
      return TRUE;
    }
  }
  return FALSE;
}

SC_FUNC void delete_substtable(void)
{
  size_t i;
  for (i=0; i<macro_capacity; i++) {
    stringpair *item=macro_buckets[i];
    while (item!=NULL) {
      stringpair *next=item->next;
      free(item);
      item=next;
    }
  }
  free(macro_buckets);
  macro_buckets=NULL;
  macro_capacity=macro_count=0;
}

#endif /* !defined NO_SUBST */


/* ----- input file list ----------------------------------------- */
static stringlist sourcefiles = {NULL, NULL, 0, 0};

SC_FUNC stringlist *insert_sourcefile(char *string)
{
  return insert_string(&sourcefiles,string);
}

SC_FUNC char *get_sourcefile(int index)
{
  return get_string(&sourcefiles,index);
}

SC_FUNC void delete_sourcefiletable(void)
{
  delete_stringtable(&sourcefiles);
  assert(sourcefiles.strings==NULL);
}


/* ----- documentation tags -------------------------------------- */
#if !defined SC_LIGHT
static stringlist docstrings = {NULL, NULL, 0, 0};

SC_FUNC stringlist *insert_docstring(char *string)
{
  return insert_string(&docstrings,string);
}

SC_FUNC char *get_docstring(int index)
{
  return get_string(&docstrings,index);
}

SC_FUNC void delete_docstring(int index)
{
  assert(index==0);
  delete_string(&docstrings, index);
}

SC_FUNC void delete_docstringtable(void)
{
  delete_stringtable(&docstrings);
  assert(docstrings.strings==NULL);
}
#endif /* !defined SC_LIGHT */


/* ----- autolisting --------------------------------------------- */
static stringlist autolist = {NULL, NULL, 0, 0};

SC_FUNC stringlist *insert_autolist(char *string)
{
  return insert_string(&autolist,string);
}

SC_FUNC char *get_autolist(int index)
{
  return get_string(&autolist,index);
}

SC_FUNC void delete_autolisttable(void)
{
  delete_stringtable(&autolist);
  assert(autolist.strings==NULL);
}


/* ----- value pair list ----------------------------------------- */
static valuepair heaplist = {NULL, 0, 0};

SC_FUNC valuepair *push_heaplist(long first, long second)
{
  valuepair *cur, *last;
  if ((cur=malloc(sizeof(valuepair)))==NULL)
    error(103);       /* insufficient memory (fatal error) */

  cur->first=first;
  cur->second=second;
  cur->next=NULL;

  for (last=&heaplist; last->next!=NULL; last=last->next)
    /* nothing */;
  last->next=cur;
  return cur;
}

SC_FUNC int popfront_heaplist(long *first, long *second)
{
  valuepair *front=heaplist.next;
  if (front==NULL)
    return 0;

  /* copy fields */
  *first=front->first;
  *second=front->second;

  /* unlink and free */
  heaplist.next=front->next;
  free(front);
  return 1;
}

SC_FUNC void delete_heaplisttable(void)
{
  valuepair *cur;
  while (heaplist.next!=NULL) {
    cur=heaplist.next;
    heaplist.next=cur->next;
    free(cur);
  } /* while */
}


/* ----- debug information --------------------------------------- */
static stringlist dbgstrings = {NULL, NULL, 0, 0};

SC_FUNC stringlist *insert_dbgfile(const char *filename)
{
  if (sc_status==statWRITE && (sc_debug & sSYMBOLIC)!=0) {
    char string[_MAX_PATH+40];
    assert(filename!=NULL);
    assert(strlen(filename)+40<sizeof string);
    sprintf(string,"F:%" PRIxC " %s",code_idx,filename);
    return insert_string(&dbgstrings,string);
  } /* if */
  return NULL;
}

SC_FUNC stringlist *insert_dbgline(int linenr)
{
  if (sc_status==statWRITE && (sc_debug & sSYMBOLIC)!=0) {
    char string[40];
    if (linenr>0)
      linenr--;         /* line numbers are zero-based in the debug information */
    sprintf(string,"L:%" PRIxC " %x",code_idx,linenr);
    return insert_string(&dbgstrings,string);
  } /* if */
  return NULL;
}

SC_FUNC stringlist *insert_dbgsymbol(symbol *sym)
{
  if (sc_status==statWRITE && (sc_debug & sSYMBOLIC)!=0) {
    char string[2*sNAMEMAX+128];
    char symname[2*sNAMEMAX+16];

    funcdisplayname(symname,sym->name);
    /* address tag:name codestart codeend ident vclass [tag:dim ...] */
    if (sym->ident==iFUNCTN) {
      sprintf(string,"S:%" PRIxC " %x:%s %" PRIxC " %" PRIxC " %x %x",
              sym->addr,sym->tag,symname,sym->addr,sym->codeaddr,sym->ident,sym->vclass);
    } else {
      sprintf(string,"S:%" PRIxC " %x:%s %" PRIxC " %" PRIxC " %x %x",
              sym->addr,sym->tag,symname,sym->codeaddr,code_idx,sym->ident,sym->vclass);
    } /* if */
    if (sym->ident==iARRAY || sym->ident==iREFARRAY) {
      #if !defined NDEBUG
        int count=sym->dim.array.level;
      #endif
      symbol *sub;
      strcat(string," [ ");
      for (sub=sym; sub!=NULL; sub=finddepend(sub)) {
        assert(sub->dim.array.level==count--);
        sprintf(string+strlen(string),"%x:%x ",sub->x.tags.index,sub->dim.array.length);
      } /* for */
      strcat(string,"]");
    } /* if */

    return insert_string(&dbgstrings,string);
  } /* if */
  return NULL;
}

SC_FUNC char *get_dbgstring(int index)
{
  return get_string(&dbgstrings,index);
}

SC_FUNC void delete_dbgstringtable(void)
{
  delete_stringtable(&dbgstrings);
  assert(dbgstrings.strings==NULL);
}
