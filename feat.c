#include "feat.h"

undef_int
i_has_format(char *frmt) {
  int rc,i;
  rc=0;
  i=0;
  while(i_format_list[i] != NULL) if ( !strcmp(frmt,i_format_list[i++]) ) rc=1;
  return(rc);
}
