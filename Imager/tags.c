/*
=head1 NAME

tags.c - functions for manipulating an images tags list

=head1 SYNOPSIS

  i_img_tags tags;
  i_tags_new(&tags);
  i_tags_destroy(&tags);
  i_tags_addn(&tags, "name", code, idata);
  i_tags_add(&tags, "name", code, data, data_size, idata);
  if (i_tags_find(&tags, name, start, &entry)) { found }
  if (i_tags_findn(&tags, code, start, &entry)) { found }
  i_tags_delete(&tags, index);
  count = i_tags_delbyname(tags, name);
  count = i_tags_delbycode(tags, code);

=head1 DESCRIPTION

Provides functions which give write access to the tags list of an image.

For read access directly access the fields (do not write any fields
directly).

A tag is represented by an i_img_tag structure:

  typedef enum {
    itt_double,
    iit_text
  } i_tag_type;

  typedef struct {
    char *name; // name of a given tag, might be NULL 
    int code; // number of a given tag, -1 if it has no meaning 
    char *data; // value of a given tag if it's not an int, may be NULL 
    int size; // size of the data 
    int idata; // value of a given tag if data is NULL 
  } i_img_tag;


=over

=cut
*/

#include "image.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

/* useful for debugging */
void i_tags_print(i_img_tags *tags);

/*
=item i_tags_new(i_img_tags *tags)

Initialize a tags structure.  Should not be used if the tags structure
has been previously used.

To destroy the contents use i_tags_destroy()

=cut
*/

void i_tags_new(i_img_tags *tags) {
  tags->count = tags->alloc = 0;
  tags->tags = NULL;
}

/*
=item i_tags_addn(i_img_tags *tags, char *name, int code, int idata)

Adds a tag that has an integer value.  A simple wrapper around i_tags_add().

Duplicate tags can be added.

Returns non-zero on success.

=cut
*/

int i_tags_addn(i_img_tags *tags, char const *name, int code, int idata) {
  return i_tags_add(tags, name, code, NULL, 0, idata);
}

/*
=item i_tags_add(i_img_tags *tags, char *name, int code, char *data, int size, i_tag_type type, int idata)

Adds a tag to the tags list.

Duplicate tags can be added.

Returns non-zero on success.

=cut
*/

int i_tags_add(i_img_tags *tags, char const *name, int code, char const *data, 
               int size, int idata) {
  i_img_tag work = {0};
  /*printf("i_tags_add(tags %p [count %d], name %s, code %d, data %p, size %d, idata %d)\n",
    tags, tags->count, name, code, data, size, idata);*/
  if (tags->tags == NULL) {
    int alloc = 10;
    tags->tags = mymalloc(sizeof(i_img_tag) * alloc);
    if (!tags->tags)
      return 0;
    tags->alloc = alloc;
  }
  else if (tags->count == tags->alloc) {
    int newalloc = tags->alloc + 10;
    void *newtags = myrealloc(tags->tags, sizeof(i_img_tag) * newalloc);
    if (!newtags) {
      return 0;
    }
    tags->tags = newtags;
    tags->alloc = newalloc;
  }
  if (name) {
    work.name = mymalloc(strlen(name)+1);
    if (!work.name)
      return 0;
    strcpy(work.name, name);
  }
  if (data) {
    if (size == -1)
      size = strlen(data);
    work.data = mymalloc(size+1);
    if (!work.data) {
      if (work.name) myfree(work.name);
      return 0;
    }
    memcpy(work.data, data, size);
    work.data[size] = '\0'; /* convenience */
    work.size = size;
  }
  work.code = code;
  work.idata = idata;
  tags->tags[tags->count++] = work;

  /*i_tags_print(tags);*/

  return 1;
}

void i_tags_destroy(i_img_tags *tags) {
  if (tags->tags) {
    int i;
    for (i = 0; i < tags->count; ++i) {
      if (tags->tags[i].name)
	myfree(tags->tags[i].name);
      if (tags->tags[i].data)
	myfree(tags->tags[i].data);
    }
    myfree(tags->tags);
  }
}

int i_tags_find(i_img_tags *tags, char const *name, int start, int *entry) {
  if (tags->tags) {
    while (start < tags->count) {
      if (tags->tags[start].name && strcmp(name, tags->tags[start].name) == 0) {
	*entry = start;
	return 1;
      }
      ++start;
    }
  }
  return 0;
}

int i_tags_findn(i_img_tags *tags, int code, int start, int *entry) {
  if (tags->tags) {
    while (start < tags->count) {
      if (tags->tags[start].code == code) {
	*entry = start;
	return 1;
      }
      ++start;
    }
  }
  return 0;
}

int i_tags_delete(i_img_tags *tags, int entry) {
  /*printf("i_tags_delete(tags %p [count %d], entry %d)\n",
    tags, tags->count, entry);*/
  if (tags->tags && entry >= 0 && entry < tags->count) {
    i_img_tag old = tags->tags[entry];
    memmove(tags->tags+entry, tags->tags+entry+1,
	    (tags->count-entry-1) * sizeof(i_img_tag));
    if (old.name)
      myfree(old.name);
    if (old.data)
      myfree(old.data);
    --tags->count;

    return 1;
  }
  return 0;
}

int i_tags_delbyname(i_img_tags *tags, char const *name) {
  int count = 0;
  int i;
  /*printf("i_tags_delbyname(tags %p [count %d], name %s)\n",
    tags, tags->count, name);*/
  if (tags->tags) {
    for (i = tags->count-1; i >= 0; --i) {
      if (tags->tags[i].name && strcmp(name, tags->tags[i].name) == 0) {
        ++count;
        i_tags_delete(tags, i);
      }
    }
  }
  /*i_tags_print(tags);*/

  return count;
}

int i_tags_delbycode(i_img_tags *tags, int code) {
  int count = 0;
  int i;
  if (tags->tags) {
    for (i = tags->count-1; i >= 0; --i) {
      if (tags->tags[i].code == code) {
        ++count;
        i_tags_delete(tags, i);
      }
    }
  }
  return count;
}

int i_tags_get_float(i_img_tags *tags, char const *name, int code, 
                     double *value) {
  int index;
  i_img_tag *entry;

  if (name) {
    if (!i_tags_find(tags, name, 0, &index))
      return 0;
  }
  else {
    if (!i_tags_findn(tags, code, 0, &index))
      return 0;
  }
  entry = tags->tags+index;
  if (entry->data)
    *value = atof(entry->data);
  else
    *value = entry->idata;

  return 1;
}

int i_tags_set_float(i_img_tags *tags, char const *name, int code, 
                     double value) {
  char temp[40];

  sprintf(temp, "%.30g", value);
  if (name)
    i_tags_delbyname(tags, name);
  else
    i_tags_delbycode(tags, code);

  return i_tags_add(tags, name, code, temp, strlen(temp), 0);
}

int i_tags_get_int(i_img_tags *tags, char const *name, int code, int *value) {
  int index;
  i_img_tag *entry;

  if (name) {
    if (!i_tags_find(tags, name, 0, &index))
      return 0;
  }
  else {
    if (!i_tags_findn(tags, code, 0, &index))
      return 0;
  }
  entry = tags->tags+index;
  if (entry->data)
    *value = atoi(entry->data);
  else
    *value = entry->idata;

  return 1;
}

static int parse_long(char *data, char **end, long *out) {
#if 0
  /* I wrote this without thinking about strtol */
  long x = 0;
  int neg = *data == '-';

  if (neg)
    ++data;
  if (!isdigit(*data))
    return 0;
  while (isdigit(*data)) {
    /* this check doesn't guarantee we don't overflow, but it helps */
    if (x > LONG_MAX / 10)
      return 0; 
    x = x * 10 + *data - '0';
    ++data;
  }
  if (neg)
    x = -x;

  *end = data;

  return 1;
#else
  long result;
  int savederr = errno;
  char *myend;

  errno = 0;
  result = strtol(data, &myend, 10);
  if ((result == LONG_MIN || result == LONG_MAX) && errno == ERANGE
      || myend == data) {
    return 0;
  }

  *out = result;
  *end = myend;

  return 1;
#endif
}

/* parse a comma-separated list of integers
   returns when it has maxcount numbers, finds a non-comma after a number
   or can't parse a number
   if it can't parse a number after a comma, that's considered an error
*/
static int parse_long_list(char *data, char **end, int maxcount, long *out) {
  int i;

  i = 0;
  while (i < maxcount-1) {
    if (!parse_long(data, &data, out))
      return 0;
    out++;
    i++;
    if (*data != ',')
      return i;
    ++data;
  }
  if (!parse_long(data, &data, out))
    return 0;
  ++i;
  *end = data;
  return i;
}

/* parse "color(red,green,blue,alpha)" */
static int parse_color(char *data, char **end, i_color *value) {
  long n[4];
  int count, i;
  
  if (memcmp(data, "color(", 6))
    return 0; /* not a color */
  data += 6;
  count = parse_long_list(data, &data, 4, n);
  if (count < 3)
    return 0;
  for (i = 0; i < count; ++i)
    value->channel[i] = n[i];
  if (count < 4)
    value->channel[3] = 255;

  return 1;
}

int i_tags_get_color(i_img_tags *tags, char const *name, int code, 
                     i_color *value) {
  int index;
  i_img_tag *entry;
  char *end;

  if (name) {
    if (!i_tags_find(tags, name, 0, &index))
      return 0;
  }
  else {
    if (!i_tags_findn(tags, code, 0, &index))
      return 0;
  }
  entry = tags->tags+index;
  if (!entry->data) 
    return 0;

  if (!parse_color(entry->data, &end, value))
    return 0;
  
  /* for now we're sloppy about the end */

  return 1;
}

int i_tags_set_color(i_img_tags *tags, char const *name, int code, 
                     i_color const *value) {
  char temp[80];

  sprintf(temp, "color(%d,%d,%d,%d)", value->channel[0], value->channel[1],
          value->channel[2], value->channel[3]);
  if (name)
    i_tags_delbyname(tags, name);
  else
    i_tags_delbycode(tags, code);

  return i_tags_add(tags, name, code, temp, strlen(temp), 0);
}

int i_tags_get_string(i_img_tags *tags, char const *name, int code, 
                      char *value, size_t value_size) {
  int index;
  i_img_tag *entry;

  if (name) {
    if (!i_tags_find(tags, name, 0, &index))
      return 0;
  }
  else {
    if (!i_tags_findn(tags, code, 0, &index))
      return 0;
  }
  entry = tags->tags+index;
  if (entry->data) {
    size_t cpsize = value_size < entry->size ? value_size : entry->size;
    memcpy(value, entry->data, cpsize);
    if (cpsize == value_size)
      --cpsize;
    value[cpsize] = '\0';
  }
  else {
    sprintf(value, "%d", entry->data);
  }

  return 1;
}

void i_tags_print(i_img_tags *tags) {
  int i;
  printf("Alloc %d\n", tags->alloc);
  printf("Count %d\n", tags->count);
  for (i = 0; i < tags->count; ++i) {
    i_img_tag *tag = tags->tags + i;
    printf("Tag %d\n", i);
    if (tag->name)
      printf(" Name : %s (%p)\n", tag->name, tag->name);
    printf(" Code : %d\n", tag->code);
    if (tag->data) {
      int pos;
      printf(" Data : %d (%p) => '", tag->size, tag->data);
      for (pos = 0; pos < tag->size; ++pos) {
	if (tag->data[pos] == '\\' || tag->data[pos] == '\'') {
	  putchar('\\');
	  putchar(tag->data[pos]);
	}
	else if (tag->data[pos] < ' ' || tag->data[pos] >= '\x7E')
	  printf("\\x%02X", tag->data[pos]);
	else
	  putchar(tag->data[pos]);
      }
      printf("'\n");
      printf(" Idata: %d\n", tag->idata);
    }
  }
}

/*
=back

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

Imager(3)

=cut
*/
