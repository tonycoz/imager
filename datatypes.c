#include "io.h"
#include "datatypes.h"
#include <stdlib.h>
#include <stdio.h>



/*
  2d bitmask with test and set operations
*/

struct i_bitmap*
btm_new(int xsize,int ysize) {
  int i;
  struct i_bitmap *btm;
  btm=(struct i_bitmap*)mymalloc(sizeof(struct i_bitmap));
  btm->data=(char*)mymalloc((xsize*ysize+8)/8);
  btm->xsize=xsize;
  btm->ysize=ysize;
  for(i=0;i<(xsize*ysize+8)/8;i++) btm->data[i]=0; /* Is this always needed */
  return btm;
}


void
btm_destroy(struct i_bitmap *btm) {
  myfree(btm->data);
  myfree(btm);
}


int
btm_test(struct i_bitmap *btm,int x,int y) {
  int btno;
  if (x<0 || x>btm->xsize-1 || y<0 || y>btm->ysize-1) return 0;
  btno=btm->xsize*y+x;
  return (1<<(btno%8))&(btm->data[btno/8]);
}

void
btm_set(struct i_bitmap *btm,int x,int y) {
  int btno;
  btno=btm->xsize*y+x;
  btm->data[btno/8]|=1<<(btno%8);
}





/*
  Bucketed linked list - stack type 
*/

struct llink *
llink_new(struct llink* p,int size) {
  struct llink *l;
  l       = mymalloc(sizeof(struct llink));
  l->n    = NULL;
  l->p    = p;
  l->fill = 0;
  l->data = mymalloc(size);
  return l;
}

/* free's the data pointer, itself, and sets the previous' next pointer to null */

void
llink_destroy(struct llink* l) {
  if (l->p != NULL) { l->p->n=NULL; }
  myfree(l->data);
  myfree(l);
}


/* if it returns true there wasn't room for the
   item on the link */

int
llist_llink_push(struct llist *lst, struct llink *lnk,void *data) {
  int multip;
  multip = lst->multip;

  /*   fprintf(stderr,"llist_llink_push: data=0x%08X -> 0x%08X\n",data,*(int*)data);
       fprintf(stderr,"ssize = %d, multip = %d, fill = %d\n",lst->ssize,lst->multip,lnk->fill); */
  if (lnk->fill == lst->multip) return 1;
  /*   memcpy((char*)(lnk->data)+lnk->fill*lst->ssize,data,lst->ssize); */
  memcpy((char*)(lnk->data)+lnk->fill*lst->ssize,data,lst->ssize);
  
  /*   printf("data=%X res=%X\n",*(int*)data,*(int*)(lnk->data));*/
  lnk->fill++;
  lst->count++;
  return 0;
}

struct llist *
llist_new(int multip, int ssize) {
  struct llist *l;
  l         = mymalloc(sizeof(struct llist));
  l->h      = NULL;
  l->t      = NULL;
  l->multip = multip;
  l->ssize  = ssize;
  l->count  = 0;
  return l;
}

void
llist_push(struct llist *l,void *data) {
  int ssize  = l->ssize;
  int multip = l->multip;
  
  /*  fprintf(stderr,"llist_push: data=0x%08X\n",data);
      fprintf(stderr,"Chain size: %d\n", l->count); */
    
  if (l->t == NULL) {
    l->t = l->h = llink_new(NULL,ssize*multip);  /* Tail is empty - list is empty */
    /* fprintf(stderr,"Chain empty - extended\n"); */
  }
  else { /* Check for overflow in current tail */
    if (l->t->fill >= l->multip) {
      struct llink* nt = llink_new(l->t, ssize*multip);
      l->t->n=nt;
      l->t=nt;
      /* fprintf(stderr,"Chain extended\n"); */
    }
  }
  /*   fprintf(stderr,"0x%08X\n",l->t); */
  if (llist_llink_push(l,l->t,data)) { 
    m_fatal(3, "out of memory\n");
  }
}

/* returns 0 if the list is empty */

int
llist_pop(struct llist *l,void *data) {
  /*   int ssize=l->ssize; 
       int multip=l->multip;*/
  if (l->t == NULL) return 0;
  l->t->fill--;
  l->count--;
  memcpy(data,(char*)(l->t->data)+l->ssize*l->t->fill,l->ssize);
  
  if (!l->t->fill) {			 	/* This link empty */
    if (l->t->p == NULL) {                      /* and it's the only link */
      llink_destroy(l->t);
      l->h = l->t = NULL;
    }
    else {
      l->t=l->t->p;
      llink_destroy(l->t->n);
    }
  }
  return 1;
}

void
llist_dump(struct llist *l) {
  int k,j;
  int i=0;
  struct llink *lnk; 
  lnk=l->h;
  while(lnk != NULL) {
    for(j=0;j<lnk->fill;j++) {
      /*       memcpy(&k,(char*)(lnk->data)+l->ssize*j,sizeof(void*));*/
      memcpy(&k,(char*)(lnk->data)+l->ssize*j,sizeof(void*));
      printf("%d - %X\n",i,k);
      i++;
    }
    lnk=lnk->n;
  }
}

void
llist_destroy(struct llist *l) {
  struct llink *t,*lnk = l->h;
  while( lnk != NULL ) {
    t=lnk;
    lnk=lnk->n;
    myfree(t);
  }
  myfree(l);
}






/*
  Oct-tree implementation 
*/

struct octt *
octt_new() {
  int i;
  struct octt *t;
  
  t=(struct octt*)mymalloc(sizeof(struct octt));
  for(i=0;i<8;i++) t->t[i]=NULL;
  t->cnt=0;
  return t;
}


/* returns 1 if the colors wasn't in the octtree already */


int
octt_add(struct octt *ct,unsigned char r,unsigned char g,unsigned char b) {
  struct octt *c;
  int i,cm;
  int ci,idx[8];
  int rc;
  rc=0;
  c=ct;
  /*  printf("[r,g,b]=[%d,%d,%d]\n",r,g,b); */
  ct->cnt++;
  for(i=7;i>-1;i--) {
    cm=1<<i;
    ci=((!!(r&cm))<<2)+((!!(g&cm))<<1)+!!(b&cm); 
    /* printf("idx[%d]=%d\n",i,ci); */
    if (c->t[ci] == NULL) { c->t[ci]=octt_new(); rc=1; }
    c=c->t[ci];
    c->cnt++;
    idx[i]=ci;
  }
  return rc;
}


void
octt_delete(struct octt *ct) {
  int i;
  for(i=0;i<8;i++) if (ct->t[i] != NULL) octt_delete(ct->t[i]);  /* do not free instance here because it will free itself */
  myfree(ct);
}


void
octt_dump(struct octt *ct) {
	int i;
	/* 	printf("node [0x%08X] -> (%d)\n",ct,ct->cnt); */
	for(i=0;i<8;i++) if (ct->t[i] != NULL) printf("[ %d ] -> 0x%08X\n",i,(unsigned int)ct->t[i]);	
	for(i=0;i<8;i++) if (ct->t[i] != NULL) octt_dump(ct->t[i]);
}

/* note that all calls of octt_count are operating on the same overflow 
   variable so all calls will know at the same time if an overflow
   has occured and stops there. */

void
octt_count(struct octt *ct,int *tot,int max,int *overflow) {
  int i,c;
  c=0;
  if (!(*overflow)) return;
  for(i=0;i<8;i++) if (ct->t[i]!=NULL) { 
    octt_count(ct->t[i],tot,max,overflow);
    c++;
  }
  if (!c) (*tot)++;
  if ( (*tot) > (*overflow) ) *overflow=0;
}
