#include "dynaload.h"
#include "XSUB.h" /* so we can compile on threaded perls */

/* These functions are all shared - then comes platform dependant code */


int getstr(void *hv_t,char *key,char **store) {
  SV** svpp;
  HV* hv=(HV*)hv_t;

  mm_log((1,"getstr(hv_t 0x%X, key %s, store 0x%X)\n",hv_t,key,store));

  if ( !hv_exists(hv,key,strlen(key)) ) return 0;

  svpp=hv_fetch(hv, key, strlen(key), 0);
  *store=SvPV(*svpp, PL_na );

  return 1;
}

int getint(void *hv_t,char *key,int *store) {
  SV** svpp;
  HV* hv=(HV*)hv_t;  

  mm_log((1,"getint(hv_t 0x%X, key %s, store 0x%X)\n",hv_t,key,store));

  if ( !hv_exists(hv,key,strlen(key)) ) return 0;

  svpp=hv_fetch(hv, key, strlen(key), 0);
  *store=(int)SvIV(*svpp);
  return 1;
}

int getdouble(void *hv_t,char* key,double *store) {
  SV** svpp;
  HV* hv=(HV*)hv_t;

  mm_log((1,"getdouble(hv_t 0x%X, key %s, store 0x%X)\n",hv_t,key,store));

  if ( !hv_exists(hv,key,strlen(key)) ) return 0;
  svpp=hv_fetch(hv, key, strlen(key), 0);
  *store=(float)SvNV(*svpp);
  return 1;
}

int getvoid(void *hv_t,char* key,void **store) {
  SV** svpp;
  HV* hv=(HV*)hv_t;

  mm_log((1,"getvoid(hv_t 0x%X, key %s, store 0x%X)\n",hv_t,key,store));

  if ( !hv_exists(hv,key,strlen(key)) ) return 0;

  svpp=hv_fetch(hv, key, strlen(key), 0);
  *store=(void*)SvIV(*svpp);

  return 1;
}

int getobj(void *hv_t,char *key,char *type,void **store) {
  SV** svpp;
  HV* hv=(HV*)hv_t;

  mm_log((1,"getobj(hv_t 0x%X, key %s,type %s, store 0x%X)\n",hv_t,key,type,store));

  if ( !hv_exists(hv,key,strlen(key)) ) return 0;

  svpp=hv_fetch(hv, key, strlen(key), 0);

  if (sv_derived_from(*svpp,type)) {
    IV tmp = SvIV((SV*)SvRV(*svpp));
    *store = (void*) tmp;
  } else {
    mm_log((1,"getobj: key exists in hash but is not of correct type"));
    return 0;
  }

  return 1;
}


UTIL_table_t UTIL_table={getstr,getint,getdouble,getvoid,getobj};
extern symbol_table_t symbol_table;

/*
  Dynamic loading works like this:
  dynaload opens the shared object and
  loads all the functions into an array of functions
  it returns a string from the dynamic function that
  can be supplied to the parser for evaling.
*/

void
DSO_call(DSO_handle *handle,int func_index,HV* hv) {
  mm_log((1,"DSO_call(handle 0x%X, func_index %d, hv 0x%X)\n",handle,func_index,hv));
  (handle->function_list[func_index].iptr)((void*)hv);
}


#if defined( OS_hpux )

void*
DSO_open(char* file,char** evalstring) {
  shl_t tt_handle;
  void *d_handle,**plugin_symtab,**plugin_utiltab;
  int  rc,*iptr, (*fptr)(int);
  func_ptr *function_list;
  DSO_handle *dso_handle;
  void (*f)(void *s,void *u); /* these will just have to be void for now */
  int i;

  *evalstring=NULL;

  mm_log( (1,"DSO_open(file '%s' (0x%08X), evalstring 0x%08X)\n",file,file,evalstring) );

  if ( (tt_handle = shl_load(file, BIND_DEFERRED,0L)) == NULL) return NULL; 
  if ( (shl_findsym(&tt_handle, I_EVALSTR,TYPE_UNDEFINED,(void*)evalstring))) return NULL;

  /*
  if ( (shl_findsym(&tt_handle, "symbol_table",TYPE_UNDEFINED,(void*)&plugin_symtab))) return NULL;
  if ( (shl_findsym(&tt_handle, "util_table",TYPE_UNDEFINED,&plugin_utiltab))) return NULL;
  (*plugin_symtab)=&symbol_table;
  (*plugin_utiltab)=&UTIL_table;
  */

  if ( (shl_findsym(&tt_handle, I_INSTALL_TABLES ,TYPE_UNDEFINED, &f ))) return NULL; 
 
  mm_log( (1,"Calling install_tables\n") );
  f(&symbol_table,&UTIL_table);
  mm_log( (1,"Call ok.\n") ); 
 
  if ( (shl_findsym(&tt_handle, I_FUNCTION_LIST ,TYPE_UNDEFINED,(func_ptr*)&function_list))) return NULL; 
  if ( (dso_handle=(DSO_handle*)malloc(sizeof(DSO_handle))) == NULL) return NULL;

  dso_handle->handle=tt_handle; /* needed to close again */
  dso_handle->function_list=function_list;
  if ( (dso_handle->filename=(char*)malloc(strlen(file))) == NULL) { free(dso_handle); return NULL; }
  strcpy(dso_handle->filename,file);

  mm_log((1,"DSO_open <- (0x%X)\n",dso_handle));
  return (void*)dso_handle;
}

undef_int
DSO_close(void *ptr) {
  DSO_handle *handle=(DSO_handle*) ptr;
  mm_log((1,"DSO_close(ptr 0x%X)\n",ptr));
  return !shl_unload((handle->handle));
}

#elif defined(WIN32)

void *
DSO_open(char *file, char **evalstring) {
  HMODULE d_handle;
  func_ptr *function_list;
  DSO_handle *dso_handle;
  
  void (*f)(void *s,void *u); /* these will just have to be void for now */

  mm_log( (1,"DSO_open(file '%s' (0x%08X), evalstring 0x%08X)\n",file,file,evalstring) );

  *evalstring = NULL;
  if ((d_handle = LoadLibrary(file)) == NULL) {
    mm_log((1, "DSO_open: LoadLibrary(%s) failed: %lu\n", file, GetLastError()));
    return NULL;
  }
  if ( (*evalstring = (char *)GetProcAddress(d_handle, I_EVALSTR)) == NULL) {
    mm_log((1,"DSO_open: GetProcAddress didn't fine '%s': %lu\n", I_EVALSTR, GetLastError()));
    FreeLibrary(d_handle);
    return NULL;
  }
  if ((f = (void (*)(void *, void*))GetProcAddress(d_handle, I_INSTALL_TABLES)) == NULL) {
    mm_log((1, "DSO_open: GetProcAddress didn't find '%s': %lu\n", I_INSTALL_TABLES, GetLastError()));
    FreeLibrary(d_handle);
    return NULL;
  }
  mm_log((1, "Calling install tables\n"));
  f(&symbol_table, &UTIL_table);
  mm_log((1, "Call ok\n"));
  
  if ( (function_list = (func_ptr *)GetProcAddress(d_handle, I_FUNCTION_LIST)) == NULL) {
    mm_log((1, "DSO_open: GetProcAddress didn't find '%s': %lu\n", I_FUNCTION_LIST, GetLastError()));
    FreeLibrary(d_handle);
    return NULL;
  }
  if ( (dso_handle = (DSO_handle*)malloc(sizeof(DSO_handle))) == NULL) {
    mm_log( (1, "DSO_Open: out of memory\n") );
    FreeLibrary(d_handle);
    return NULL;
  }
  dso_handle->handle=d_handle; /* needed to close again */
  dso_handle->function_list=function_list;
  if ( (dso_handle->filename=(char*)malloc(strlen(file))) == NULL) { free(dso_handle); FreeLibrary(d_handle); return NULL; }
  strcpy(dso_handle->filename,file);

  mm_log( (1,"DSO_open <- 0x%X\n",dso_handle) );
  return (void*)dso_handle;

}

undef_int
DSO_close(void *ptr) {
  DSO_handle *handle = (DSO_handle *)ptr;
  FreeLibrary(handle->handle);
  free(handle->filename);
  free(handle);
}

#else

/* OS/2 has no dlclose; Perl doesn't provide one. */
#ifdef __EMX__ /* OS/2 */
int
dlclose(minthandle_t h) {
  return DosFreeModule(h) ? -1 : 0;
}
#endif /* __EMX__ */


void*
DSO_open(char* file,char** evalstring) {
  void *d_handle;
  func_ptr *function_list;
  DSO_handle *dso_handle;

  void (*f)(void *s,void *u); /* these will just have to be void for now */
  
  *evalstring=NULL;

  mm_log( (1,"DSO_open(file '%s' (0x%08X), evalstring 0x%08X)\n",file,file,evalstring) );

  if ( (d_handle = dlopen(file, RTLD_LAZY)) == NULL) {
    mm_log( (1,"DSO_open: dlopen failed: %s.\n",dlerror()) );
    return NULL;
  }

  if ( (*evalstring = (char *)dlsym(d_handle, I_EVALSTR)) == NULL) {
    mm_log( (1,"DSO_open: dlsym didn't find '%s': %s.\n",I_EVALSTR,dlerror()) );
    return NULL;
  }

  /*

    I'll just leave this thing in here for now if I need it real soon

   mm_log( (1,"DSO_open: going to dlsym '%s'\n", I_SYMBOL_TABLE ));
   if ( (plugin_symtab = dlsym(d_handle, I_SYMBOL_TABLE)) == NULL) {
     mm_log( (1,"DSO_open: dlsym didn't find '%s': %s.\n",I_SYMBOL_TABLE,dlerror()) );
     return NULL;
   }
  
   mm_log( (1,"DSO_open: going to dlsym '%s'\n", I_UTIL_TABLE ));
    if ( (plugin_utiltab = dlsym(d_handle, I_UTIL_TABLE)) == NULL) {
     mm_log( (1,"DSO_open: dlsym didn't find '%s': %s.\n",I_UTIL_TABLE,dlerror()) );
     return NULL;
   }

  */

  f = (void(*)(void *s,void *u))dlsym(d_handle, I_INSTALL_TABLES);
  mm_log( (1,"DSO_open: going to dlsym '%s'\n", I_INSTALL_TABLES ));
  if ( (f = (void(*)(void *s,void *u))dlsym(d_handle, I_INSTALL_TABLES)) == NULL) {
    mm_log( (1,"DSO_open: dlsym didn't find '%s': %s.\n",I_INSTALL_TABLES,dlerror()) );
    return NULL;
  }

  mm_log( (1,"Calling install_tables\n") );
  f(&symbol_table,&UTIL_table);
  mm_log( (1,"Call ok.\n") );

  /* (*plugin_symtab)=&symbol_table;
     (*plugin_utiltab)=&UTIL_table; */
  
  mm_log( (1,"DSO_open: going to dlsym '%s'\n", I_FUNCTION_LIST ));
  if ( (function_list=(func_ptr *)dlsym(d_handle, I_FUNCTION_LIST)) == NULL) {
    mm_log( (1,"DSO_open: dlsym didn't find '%s': %s.\n",I_FUNCTION_LIST,dlerror()) );
    return NULL;
  }
  
  if ( (dso_handle=(DSO_handle*)malloc(sizeof(DSO_handle))) == NULL) return NULL;
  
  dso_handle->handle=d_handle; /* needed to close again */
  dso_handle->function_list=function_list;
  if ( (dso_handle->filename=(char*)malloc(strlen(file))) == NULL) { free(dso_handle); return NULL; }
  strcpy(dso_handle->filename,file);

  mm_log( (1,"DSO_open <- 0x%X\n",dso_handle) );
  return (void*)dso_handle;
}

undef_int
DSO_close(void *ptr) {
  DSO_handle *handle;
  mm_log((1,"DSO_close(ptr 0x%X)\n",ptr));
  handle=(DSO_handle*) ptr;
  return !dlclose(handle->handle);
}

#endif

