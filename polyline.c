#include "impolyline.h"
#include "imager.h"

static int
expand_poly(i_polyline_t *poly, ssize_t new_point_count);


i_polyline_t *
i_polyline_new(int closed, int point_count, const i_point_t *points) {
  i_polyline_t *poly;
  int i;

  i_clear_error();
  if (point_count < 0) {
    i_push_error(0, "i_polyline_new(): point_count must by non-negative");
    return NULL;
  }
  if (point_count > 0 && !points) {
    i_push_error(0, "i_polyline_new(): points must be non-null if point_count is positive");
  }
  poly = mymalloc(sizeof(i_polyline_t));
  poly->closed = closed;
  poly->point_alloc = poly->point_count = point_count;
  poly->x = mymalloc(sizeof(double) * point_count);
  poly->y = mymalloc(sizeof(double) * point_count);
  for (i = 0; i < poly->point_count; ++i) {
    poly->x[i] = points[i].x;
    poly->y[i] = points[i].y;
  }

  return poly;
}

i_polyline_t *
i_polyline_new_empty(int closed, int alloc) {
  i_polyline_t *poly;

  i_clear_error();
  if (alloc < 10)
    alloc = 10;

  poly = mymalloc(sizeof(i_polyline_t));
  poly->closed = closed;
  poly->point_alloc = alloc;
  poly->point_count = 0;
  poly->x = mymalloc(sizeof(double) * alloc);
  poly->y = mymalloc(sizeof(double) * alloc);

  return poly;
}

void
i_polyline_delete(i_polyline_t *poly) {
  myfree(poly->x);
  myfree(poly->y);
  myfree(poly);
}

int
i_polyline_add_points(i_polyline_t *poly, int point_count, 
		      const i_point_t *points) {
  int i;
  i_clear_error();
  if (poly->point_alloc < poly->point_count + point_count) {
    if (!expand_poly(poly, poly->point_count + point_count)) 
      return 0;
  }

  for (i = 0; i < point_count; ++i) {
    poly->x[poly->point_count] = points[i].x;
    poly->y[poly->point_count] = points[i].y;
    ++poly->point_count;
  }

  return 1;
}

int
i_polyline_add_point(i_polyline_t *poly, const i_point_t *point) {
  return i_polyline_add_points(poly, 1, point);
}

int
i_polyline_add_point_xy(i_polyline_t *poly, double x, double y) {
  i_point_t p;
  p.x = x;
  p.y = y;
  return i_polyline_add_points(poly, 1, &p);
}

static int
expand_poly(i_polyline_t *poly, ssize_t new_point_count) {
  ssize_t new_alloc;
  ssize_t new_size;
  if (new_point_count < poly->point_alloc) {
    i_push_error(0, "integer overflow in polyline point count");
    return 0;
  }

  new_alloc = poly->point_alloc * 2;
  if (new_alloc < new_point_count)
    new_alloc = new_point_count;
  
  new_size = new_alloc * sizeof(double);
  if (new_size / sizeof(double) != new_alloc) {
    i_push_error(0, "integer overflow calculating allocation");
    return 0;
  }

  poly->x = myrealloc(poly->x, new_size);
  poly->y = myrealloc(poly->y, new_size);
  poly->point_alloc = new_alloc;

  return 1;
}

void
i_polyline_dump(const i_polyline_t *poly) {
  int i;
  printf("Points: %d (%d allocated)\n", poly->point_count,
	 poly->point_alloc);
  for (i = 0; i < poly->point_count; ++i)
    printf(" %4d: (%.3f,%.3f) (%d, %d)\n", i, poly->x[i], poly->y[i], (int)(poly->x[i]+0.5), (int)(poly->y[i]+0.5));
}
