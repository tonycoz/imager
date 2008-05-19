#include "imager.h"
#include "imager.h"
#include <math.h>

typedef struct {
  i_pen_t base;
  i_pen_thick_corner_t corner;
  i_pen_thick_end_t front;
  i_pen_thick_end_t back;
  double thickness;
  int custom_front_count;
  i_point_t *custom_front;
  int custom_back_count;
  i_point_t *custom_back;
  i_fill_t *fill;
} i_pen_thick_t;

static int thick_draw(i_pen_t *, i_img *im, int line_count, const i_polyline_t **lines);
static void thick_destroy(i_pen_t *);
static i_pen_t *thick_clone(i_pen_t *);

static const i_pen_vtable_t thick_vtable = {
  thick_draw,
  thick_destroy,
  thick_clone
};

static i_pen_t *
i_new_thick_pen_low(double thickness,
		    i_fill_t *fill,
		    i_pen_thick_corner_t corner, 
		    i_pen_thick_end_t front,
		    i_pen_thick_end_t back,
		    int custom_front_count,
		    i_point_t *custom_front_points,
		    int custom_back_count,
		    i_point_t *custom_back_points) {
  i_pen_thick_t *pen = mymalloc(sizeof(i_pen_thick_t));
  int i;

  pen->base.vtable = &thick_vtable;
  pen->corner = corner;
  pen->front = front;
  pen->back = back;
  pen->thickness = thickness;
  pen->fill = fill;
  if (custom_front_points) {
    pen->custom_front_count = custom_front_count;
    pen->custom_front = mymalloc(sizeof(i_point_t) * custom_front_count);
    for (i = 0; i < custom_front_count; ++i) {
      pen->custom_front[i] = custom_front_points[i];
    }
  }
  else {
    pen->custom_front_count = 0;
    pen->custom_front = NULL;
  }

  if (custom_back_points) {
    pen->custom_back_count = custom_back_count;
    pen->custom_back = mymalloc(sizeof(i_point_t) * custom_back_count);
    for (i = 0; i < custom_back_count; ++i) {
      pen->custom_back[i] = custom_back_points[i];
    }
  }
  else {
    pen->custom_back_count = 0;
    pen->custom_back = NULL;
  }

  return (i_pen_t *)pen;
}

i_pen_t *
i_new_thick_pen_color(double thickness,
		      i_color const *color,
		      int combine,
		      i_pen_thick_corner_t corner, 
		      i_pen_thick_end_t front,
		      i_pen_thick_end_t back,
		      int custom_front_count,
		      i_point_t *custom_front_points,
		      int custom_back_count,
		      i_point_t *custom_back_points) {
  i_fill_t *fill = i_new_fill_solid(color, combine);

  return i_new_thick_pen_low(thickness, fill, corner, front, back, 
			     custom_front_count, custom_front_points, 
			     custom_back_count, custom_back_points);
}
		
i_pen_t *
i_new_thick_pen_fill(double thickness,
		     i_fill_t *fill,
		     i_pen_thick_corner_t corner,
		     i_pen_thick_end_t front,
		     i_pen_thick_end_t back,
		     int custom_front_count,
		     i_point_t *custom_front_points,
		     int custom_back_count,
		     i_point_t *custom_back_points) {
  i_fill_t *cloned = i_fill_clone(fill);

  return i_new_thick_pen_low(thickness, cloned, corner, front, back, 
			     custom_front_count, custom_front_points, 
			     custom_back_count, custom_back_points);
}
		
static void
thick_destroy(i_pen_t *pen) {
  i_pen_thick_t *thick = (i_pen_thick_t *)pen;

  if (thick->custom_front)
    myfree(thick->custom_front);
  if (thick->custom_back)
    myfree(thick->custom_back);
  myfree(thick);
}

static i_pen_t *
thick_clone(i_pen_t *pen) {
  i_pen_thick_t *thick = (i_pen_thick_t *)pen;

  return i_new_thick_pen_fill(thick->thickness,
			      thick->fill,
			      thick->corner,
			      thick->front,
			      thick->back,
			      thick->custom_front_count,
			      thick->custom_front,
			      thick->custom_back_count,
			      thick->custom_back);
}

typedef struct {
  i_point_t a, b, diff;
  double start, end;
  double length;
} line_seg;

typedef struct {
  double length;
  double sin_slope, cos_slope;
  line_seg left, right;
} thick_seg;

static int
make_seg(i_pen_thick_t *thick, const i_point_t *a, const i_point_t *b, thick_seg *seg) {
  double dx = b->x - a->x;
  double dy = b->y - a->y;
  double length = sqrt(dx*dx + dy*dy);
  double cos_thick_2, sin_thick_2;

  printf("make_seg thick %g (%g, %g) - (%g, %g)\n", thick->thickness, a->x, a->y, b->x, b->y);

  if (length < 0.1)
    return 0;

  seg->length = length;
  seg->sin_slope = dy / length;
  seg->cos_slope = dx / length;
  cos_thick_2 = thick->thickness * seg->cos_slope / 2.0;
  sin_thick_2 = thick->thickness * seg->sin_slope / 2.0;
  seg->left.start = seg->right.start = 0.0;
  seg->left.end   = seg->right.end   = 1.0;
  seg->left.a.x = a->x + 0.5 + sin_thick_2;
  seg->left.a.y = a->y + 0.5 - cos_thick_2;
  seg->left.b.x = b->x + 0.5 + sin_thick_2;
  seg->left.b.y = b->y + 0.5 - cos_thick_2;
  seg->right.a.x = a->x + 0.5 - sin_thick_2;
  seg->right.a.y = a->y + 0.5 + cos_thick_2;
  seg->right.b.x = b->x + 0.5 - sin_thick_2;
  seg->right.b.y = b->y + 0.5 + cos_thick_2;

  printf("seg len %g sin %g cos %g\n", seg->length, seg->sin_slope, seg->cos_slope);
  printf("    left (%g, %g) - (%g, %g)\n", seg->left.a.x, seg->left.a.y, seg->left.b.x, seg->left.b.y);
  printf("   right (%g, %g) - (%g, %g)\n", seg->right.a.x, seg->right.a.y, seg->right.b.x, seg->right.b.y);

  return 1;
}

static int
fill_segs(i_pen_thick_t *thick, thick_seg *segs, const i_polyline_t *line) {
  int seg_count = 0;
  int from_index = 0;
  int to_index = 1;
  
  while (to_index < line->point_count) {
    if (make_seg(thick, &line->points[from_index], &line->points[to_index],
		 segs+seg_count)) {
      ++from_index;
      ++seg_count;
    }
    ++to_index;
  }
  if (line->closed) {
    if (make_seg(thick, &line->points[from_index], &line->points[0],
		 segs+seg_count)) {
      ++seg_count;
    }
  }
  
  return seg_count;
}

/*
http://local.wasp.uwa.edu.au/~pbourke/geometry/lineline2d/
 */
static void
intersect_two_segs(line_seg *first, line_seg *next) {
  double first_dx = first->b.x - first->a.x;
  double first_dy = first->b.y - first->a.y;
  double next_dx = next->b.x - next->a.x;
  double next_dy = next->b.y - next->a.y;
  double denom = next_dy * first_dx - next_dx * first_dy;
  double ufirst, unext;

  printf("P1(%g, %g) P2(%g,%g) P3(%g,%g) P4(%g,%g)\n",
	 first->a.x, first->a.y, first->b.x, first->b.y, 
	 next->a.x, next->a.y, next->b.x, next->b.y);
  if (denom == 0) {
    /* lines are co-incident (since we know they pass through the same
       point), so leave start/end alone
    */
    return;
  }

  ufirst = ( next_dx * (first->a.y - next->a.y) 
    - next_dy * (first->a.x - next->a.x) ) / denom;
  unext = ( first_dx * (first->a.y - next->a.y) 
    - first_dy * (first->a.x - next->a.x) ) / denom;

  printf("intersect -> (%g, %g)\n", ufirst, unext);

  first->end = ufirst;
  next->start = unext;
}

static void
intersect_segs(i_pen_thick_t *thick, thick_seg *segs, int seg_count, 
	       int closed) {
  int i;
  
  /* TODO: handle line elimination */
  for (i = 0; i < seg_count-1; ++i) {
    intersect_two_segs(&segs[i].left, &segs[i+1].left);
    intersect_two_segs(&segs[i].right, &segs[i+1].right);
  }
  if (closed) {
    intersect_two_segs(&segs[seg_count-1].left, &segs->left);
    intersect_two_segs(&segs[seg_count-1].right, &segs->right);
  }
}

static double
line_x(line_seg *line, double pos) {
  return line->a.x + pos * (line->b.x - line->a.x);
}

static double
line_y(line_seg *line, double pos) {
  return line->a.y + pos * (line->b.y - line->a.y);
}

static int
make_poly(thick_seg *segs, int seg_count, double **px, double **py) {
  int side_count = seg_count * 3 + 2; /* just rough */
  double *x = mymalloc(sizeof(double) * side_count);
  double *y = mymalloc(sizeof(double) * side_count);
  int i;
  int point_count = 0;
  int add_start = 1;

  /* up one side of the line */
  for (i = 0; i < seg_count; ++i) {
    thick_seg *seg = segs + i;
    double start = seg->left.start < 0 ? 0 : seg->left.start;
    double end = seg->left.end > 1.0 ? 1.0 : seg->left.end;

    //add_start=1;
    if (add_start) {
      x[point_count] = line_x(&seg->left, start);
      y[point_count] = line_y(&seg->left, start);
      ++point_count;
    }
    
    if (end > start) {
      x[point_count] = line_x(&seg->left, end);
      y[point_count] = line_y(&seg->left, end);
      ++point_count;
    }

    add_start = end == 1.0;
  }

  add_start = 1;
  /* and down the other */
  for (i = seg_count-1; i >= 0; --i) {
    thick_seg *seg = segs + i;
    double start = seg->right.start < 0 ? 0 : seg->right.start;
    double end = seg->right.end > 1.0 ? 1.0 : seg->right.end;
    printf("  right %g - %g\n", start, end);

    //add_start=1;
    if (add_start) {
      x[point_count] = line_x(&seg->right, end);
      y[point_count] = line_y(&seg->right, end);
      ++point_count;
    }
    
    if (end > start) {
      x[point_count] = line_x(&seg->right, start);
      y[point_count] = line_y(&seg->right, start);
      ++point_count;
    }
    add_start = start == 0.0;
  }

  *px = x;
  *py = y;

  {
    int i;
    for (i = 0; i < point_count; ++i) {
      printf("(%g,%g)\n", x[i], y[i]);
    }
  }

  return point_count;
}

static int
thick_draw_line(i_img *im, i_pen_thick_t *thick, const i_polyline_t *line) {
  thick_seg *segs = mymalloc(sizeof(thick_seg) * line->point_count);
  double *x = NULL;
  double *y = NULL;
  int poly_count;
  int seg_count = fill_segs(thick, segs, line);

  {
    int i;
    for (i = 0; i < line->point_count; ++i) {
      printf("%d: (%g, %g)\n", i, line->points[i].x, line->points[i].y);
    }
  }

#if 1
  if (seg_count > 1)
    intersect_segs(thick, segs, seg_count, line->closed);
#endif

#if 0
  if (!line->closed) {
    switch (thick->back) {
    case ptc_square:
    case ptc_block:
    case ptc_round:
      break;
    }
  }
#endif

  poly_count = make_poly(segs, seg_count, &x, &y);

  if (poly_count) {
    i_poly_aa_cfill(im, poly_count, x, y, thick->fill);
#if 0
    {
      i_color red;
      int i;
      red.rgba.r = red.rgba.a = 255;
      red.rgba.g = red.rgba.b = 0;
      for (i = 0; i < poly_count-1; ++i) {
	i_line(im, x[i], y[i], x[i+1], y[i+1], &red, 0);
      }
    }
#endif
  }
  
  myfree(segs);
  myfree(x);
  myfree(y);
}

static int
thick_draw(i_pen_t *pen, i_img *im, int line_count, const i_polyline_t **lines) {
  i_pen_thick_t *thick = (i_pen_thick_t *)pen;
  int result = 0;
  int i;

  for (i = 0; i < line_count; ++i) {
    if (thick_draw_line(im, thick, lines[i]))
      result = 1;
  }

  return result;
}
