#ifndef _IO_H_
#define _IO_H_
#include <stdio.h>
#include <sys/stat.h>
#ifndef _MSC_VER
#include <unistd.h>
#include <sys/mman.h>
#endif

#include "log.h"


/* #define MALLOC_DEBUG */

#ifdef IMAGER_DEBUG_MALLOC

#define mymalloc(x) (mymalloc_file_line((x), __FILE__, __LINE__))
#define myfree(x) (myfree_file_line((x), __FILE__, __LINE__))

void  malloc_state();
void* mymalloc_file_line(int size,char* file,int line);
void* mymalloc_comm(int size,char *comm);

void  myfree_file_line(void *p, char*file, int line);

void  bndcheck_all(void);

#else

#define malloc_comm(a,b) (mymalloc(a))
void malloc_state();
void* mymalloc(int size);
void myfree(void *p);

#endif /* IMAGER_MALLOC_DEBUG */


#ifdef _MSC_VER
#undef min
#undef max
#endif
int min(int a,int b);
int max(int x,int y);
int myread(int fd,void *buf,int len);
int mywrite(int fd,void *buf,int len);
void interleave(unsigned char *inbuffer,unsigned char *outbuffer,int rowsize,int channels);

#endif
