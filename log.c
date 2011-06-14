#include "imconfig.h"
#include "log.h"
#include <stdlib.h>
#include <errno.h>
#include "imerror.h"

#ifdef IMAGER_LOG

#define DTBUFF 50
#define DATABUFF DTBUFF+3+10+1+5+1+1

static int   log_level   = 0;
static FILE *lg_file     = NULL;
static char *date_format = "%Y/%m/%d %H:%M:%S";
static char  date_buffer[DTBUFF];
static char  data_buffer[DATABUFF];


/*
 * Logging is active
 */

int
i_init_log(const char* name,int level) {
  i_clear_error();
  log_level = level;
  if (level < 0) {
    lg_file = NULL;
  } else {
    if (name == NULL) {
      lg_file = stderr;
    } else {
      if (NULL == (lg_file = fopen(name, "w+")) ) { 
	i_push_errorf(errno, "Cannot open file '%s': (%d)", name, errno);
	return 0;
      }
    }
  }
  if (lg_file) {
    setvbuf(lg_file, NULL, _IONBF, BUFSIZ);
    mm_log((0,"Imager - log started (level = %d)\n", level));
  }

  return lg_file != NULL;
}

void
i_fatal(int exitcode,const char *fmt, ... ) {
  va_list ap;
  time_t timi;
  struct tm *str_tm;
  
  if (lg_file != NULL) {
    timi = time(NULL);
    str_tm = localtime(&timi);
    if ( strftime(date_buffer, DTBUFF, date_format, str_tm) )
      fprintf(lg_file,"[%s] ",date_buffer);
    va_start(ap,fmt);
    vfprintf(lg_file,fmt,ap);
    va_end(ap);
  }
  exit(exitcode);
}


/*
=item i_loog(level, format, ...)
=category Logging

This is an internal function called by the mm_log() macro.

=cut
*/

void
i_loog(int level,const char *fmt, ... ) {
  va_list ap;
  if (level > log_level) return;
  if (lg_file != NULL) {
    fputs(data_buffer, lg_file);
    fprintf(lg_file, "%3d: ",level);
    va_start(ap,fmt);
    vfprintf(lg_file, fmt, ap);
    fflush(lg_file);
    va_end(ap);
  }
}

/*
=item i_lhead(file, line)
=category Logging

This is an internal function called by the mm_log() macro.

=cut
*/

void
i_lhead(const char *file, int line) {
  time_t timi;
  struct tm *str_tm;
  
  if (lg_file != NULL) {
    timi = time(NULL);
    str_tm = localtime(&timi);
    strftime(date_buffer, DTBUFF, date_format, str_tm);
    sprintf(data_buffer, "[%s] %10s:%-5d ", date_buffer, file, line);
  }
}

#else

/*
 * Logging is inactive - insert dummy functions
 */

int i_init_log(const char* name,int onoff) {
  i_clear_error();
  i_push_error(0, "Logging disabled");
  return 0;
}

void i_fatal(int exitcode,const char *fmt, ... ) { exit(exitcode); }

void
i_loog(int level,const char *fmt, ... ) {
}

void
i_lhead(const char *file, int line) { }

#endif
