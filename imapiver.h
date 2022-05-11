#ifndef IMAGER_IMAPIVER_INCLUDED
#define IMAGER_IMAPIVER_INCLUDED

/*
 IMAGER_API_VERSION is similar to the version number in the third and
 fourth bytes of TIFF files - if it ever changes then the API has changed
 too much for any application to remain compatible.

 Version 2 changed the types of some parameters and pointers.  A
 simple recompile should be enough in most cases.

 Version 3 changed the behaviour of some of the I/O layer functions,
 and in some cases the initial seek position when calling file
 readers.  Switching away from calling readcb etc to i_io_read() etc
 should fix your code.

 Version 4 added i_psamp() and i_psampf() pointers to the i_img
 structure.

 Version 5 changed the return types of i_get_file_background() and
 i_get_file_backgroundf() from void to int.

 Version 6 moved the function pointers from i_img into a vtable.

*/
#define IMAGER_API_VERSION 6

/*
 IMAGER_API_LEVEL is the level of the structure.  New function pointers
 will always remain at the end (unless IMAGER_API_VERSION changes), and
 will result in an increment of IMAGER_API_LEVEL.
*/

#define IMAGER_API_LEVEL 10

#endif
