#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
/* 
   input:  name of file to log too
   input:  onoff, 0 means no logging
   global: creates a global variable FILE* lg_file
*/

void m_lhead ( const char *file, int line  );
void m_init_log( const char *name, int onoff );
void m_loog(int level,const char *msg, ... );
void m_fatal ( int exitcode,const char *fmt, ... );


#ifdef IMAGER_LOG
#define mm_log(x) { m_lhead(__FILE__,__LINE__); m_loog x; } 
#else
#define mm_log(x)
#endif


#endif /* _LOG_H_ */
