#include "io.h"
#include <stdlib.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif


/* FIXME: make allocation dynamic */


#ifdef IMAGER_DEBUG_MALLOC

#define MAXMAL 102400
#define MAXDESC 65

#define UNDRRNVAL 10
#define OVERRNVAL 10

#define PADBYTE 0xaa


static int malloc_need_init = 1;

typedef struct {
  void* point;
  size_t size;
  char  comm[MAXDESC];
} malloc_entry;

malloc_entry malloc_pointers[MAXMAL];

/*
#define mymalloc(x) (mymalloc_file_line(x,__FILE__,__LINE__)) 
*/


void
malloc_state() {
  int i, total;
  total=0;
  mm_log((0,"malloc_state()\n"));
  bndcheck_all();
  for(i=0;i<MAXMAL;i++) if (malloc_pointers[i].point!=NULL) {
    mm_log((0,"%d: %d (0x%x) : %s\n",i,malloc_pointers[i].size,malloc_pointers[i].point,malloc_pointers[i].comm));
    total+=malloc_pointers[i].size;
  }
  if (total==0 ) mm_log((0,"No memory currently used!\n"))
  else mm_log((0,"total: %d\n",total));
}


void*
mymalloc_file_line(int size, char* file, int line) {
  char *buf;
  int i;
  if (malloc_need_init) {
    for(i=0; i<MAXMAL; i++) malloc_pointers[i].point = NULL;
    malloc_need_init = 0;
    atexit(malloc_state);
  }

  /* bndcheck_all(); ACTIVATE FOR LOTS OF THRASHING */

  if ( (buf = malloc(size+UNDRRNVAL+OVERRNVAL)) == NULL ) { mm_log((1,"Unable to allocate %i for %s (%i)\n", size, file, line)); exit(3); }
  memset( buf, PADBYTE, UNDRRNVAL );
  memset( &buf[UNDRRNVAL+size], PADBYTE, OVERRNVAL );

  mm_log((1,"mymalloc_file_line: real address %p\n", buf));
  buf += UNDRRNVAL; /* Do this now so we will see the returned address in the logs. */
  
  for(i=0;i<MAXMAL;i++) if (malloc_pointers[i].point == NULL) {
    malloc_pointers[i].size = size;
    sprintf(malloc_pointers[i].comm,"%s (%d)", file, line);
    malloc_pointers[i].point = buf;
    mm_log((1,"mymalloc_file_line: slot <%d> %d bytes allocated at %p for %s (%d)\n", i, size, buf, file, line));
    return buf; 
  }
  
  mm_log((0,"more than %d segments allocated at %s (%d)\n", MAXMAL, file, line));
  exit(255);
  return NULL;
}




/* This function not maintained for now */

/*
void*
mymalloc_comm(int size,char *comm) {
  void *buf;
  int i;
  if (malloc_need_init) {
    for(i=0;i<MAXMAL;i++) malloc_pointers[i].point=NULL;
    malloc_need_init=0;
  }
  
  if ((buf=malloc(size))==NULL) { mm_log((1,"Unable to malloc.\n")); exit(3); }

  for(i=0;i<MAXMAL;i++) if (malloc_pointers[i].point==NULL) {
    malloc_pointers[i].point=buf;
    malloc_pointers[i].size=size;
    strncpy(malloc_pointers[i].comm,comm,MAXDESC-1);
    return buf;
  }
  mm_log((0,"more than %d segments malloced\n",MAXMAL));
  exit(255);
  return NULL;
}
*/

static
void
bndcheck(int idx) {
  int i;
  size_t s = malloc_pointers[idx].size;
  unsigned char *pp = malloc_pointers[idx].point;
  if (!pp) {
    mm_log((1, "bndcheck: No pointer in slot %d\n", idx));
    return;
  }
  
  for(i=0;i<UNDRRNVAL;i++)
    if (pp[-(1+i)] != PADBYTE)
      mm_log((1,"bndcheck: UNDERRUN OF %d bytes detected: slot = %d, point = %p, size = %d\n", i+1, idx, pp, s ));
  
  for(i=0;i<OVERRNVAL;i++)
    if (pp[s+i] != PADBYTE)
      mm_log((1,"bndcheck: OVERRUN OF %d bytes detected: slot = %d, point = %p, size = %d\n", i+1, idx, pp, s ));
}

void
bndcheck_all() {
  int idx;
  mm_log((1, "bndcheck_all()\n"));
  for(idx=0; idx<MAXMAL; idx++)
    if (malloc_pointers[idx].point)
      bndcheck(idx);
}





void
myfree_file_line(void *p, char *file, int line) {
  char  *pp = p;
  int match = 0;
  int i;
  
  for(i=0; i<MAXMAL; i++) if (malloc_pointers[i].point == p) {
    mm_log((1,"myfree_file_line: pointer %i (%s) freed at %s (%i)\n", i, malloc_pointers[i].comm, file, line));
    bndcheck(i);
    malloc_pointers[i].point = NULL;
    match++;
  }
  
  if (match != 1) {
    mm_log((1, "myfree_file_line: INCONSISTENT REFCOUNT %d\n", match));
  }
  
  mm_log((1, "myfree_file_line: freeing address %p\n", pp-UNDRRNVAL));
  
  free(pp-UNDRRNVAL);
}

#else 

#define malloc_comm(a,b) (mymalloc(a))

void
malloc_state() {
  printf("malloc_state: not in debug mode\n");
}

void*
mymalloc(int size) {
  void *buf;

  mm_log((1, "mymalloc(size %d)\n", size));
  if ( (buf = malloc(size)) == NULL ) {
    mm_log((1, "mymalloc: unable to malloc\n", size));
    fprintf(stderr,"Unable to malloc.\n"); exit(3);
  }
  return buf;
}

void
myfree(void *p) {
  free(p);
}

#endif /* IMAGER_MALLOC_DEBUG */








#undef min
#undef max

int
min(int a,int b) {
  if (a<b) return a; else return b;
}

int
max(int a,int b) {
  if (a>b) return a; else return b;
}

int
myread(int fd,void *buf,int len) {
  unsigned char* bufc;
  int bc,rc;
  bufc = (unsigned char*)buf;
  bc=0;
  while( ((rc=read(fd,bufc+bc,len-bc))>0 ) && (bc!=len) ) bc+=rc;
  if ( rc < 0 ) return rc;
  else return bc;
}

int
mywrite(int fd,void *buf,int len) {
  unsigned char* bufc;
  int bc,rc;
  bufc=(unsigned char*)buf;
  bc=0;
  while(((rc=write(fd,bufc+bc,len-bc))>0) && (bc!=len)) bc+=rc;
  if (rc<0) return rc;
  else return bc;
}

void
interleave(unsigned char *inbuffer,unsigned char *outbuffer,int rowsize,int channels) {
  int ch,ind,i;
  i=0;
  if ( inbuffer==outbuffer ) return; /* Check if data is already in interleaved format */
  for( ind=0; ind<rowsize; ind++) for (ch=0; ch<channels; ch++) outbuffer[i++] = inbuffer[rowsize*ch+ind]; 
}


