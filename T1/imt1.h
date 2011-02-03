#ifndef IMAGER_IMT1_H
#define IMAGER_IMT1_H

#include "imdatatypes.h"

extern undef_int
i_init_t1(int t1log);

extern void
i_close_t1(void);

extern int
i_t1_new(char *pfb,char *afm);

extern int
i_t1_destroy(int font_id);

extern void
i_t1_set_aa(int st);

extern undef_int
i_t1_cp(i_img *im,int xb,int yb,int channel,int fontnum,float points,char* str,size_t len,int align, int utf8, char const *flags);

extern int
i_t1_bbox(int fontnum,float points,const char *str,size_t len,int cords[6], int utf8,char const *flags);

extern undef_int
i_t1_text(i_img *im,int xb,int yb,const i_color *cl,int fontnum,float points,const char* str,size_t len,int align, int utf8, char const *flags);

extern int
i_t1_has_chars(int font_num, const char *text, size_t len, int utf8,
               char *out);

extern int
i_t1_face_name(int font_num, char *name_buf, size_t name_buf_size);

extern int
i_t1_glyph_name(int font_num, unsigned long ch, char *name_buf, 
		size_t name_buf_size);
#endif
