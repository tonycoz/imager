#include "imager.h"
#include "imager.h"
#include <math.h>
#include "impolyline.h"

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
make_seg(i_pen_thick_t *thick, double ax, double ay, double bx, double by, thick_seg *seg) {
  double dx = bx - ax;
  double dy = by - ay;
  double length = sqrt(dx*dx + dy*dy);
  double cos_thick_2, sin_thick_2;

  printf("make_seg thick %g (%g, %g) - (%g, %g)\n", thick->thickness, ax, ay, bx, by);

  if (length < 0.1)
    return 0;

  seg->length = length;
  seg->sin_slope = dy / length;
  seg->cos_slope = dx / length;
  cos_thick_2 = thick->thickness * seg->cos_slope / 2.0;
  sin_thick_2 = thick->thickness * seg->sin_slope / 2.0;
  seg->left.start = seg->right.start = 0.0;
  seg->left.end   = seg->right.end   = 1.0;

  seg->left.a.x = ax + 0.5 + sin_thick_2;
  seg->left.a.y = ay + 0.5 - cos_thick_2;
  seg->left.b.x = bx + 0.5 + sin_thick_2;
  seg->left.b.y = by + 0.5 - cos_thick_2;
  seg->left.diff.x = dx;
  seg->left.diff.y = dy;

  seg->right.a.x = bx + 0.5 - sin_thick_2;
  seg->right.a.y = by + 0.5 + cos_thick_2;
  seg->right.b.x = ax + 0.5 - sin_thick_2;
  seg->right.b.y = ay + 0.5 + cos_thick_2;
  seg->right.diff.x = -dx;
  seg->right.diff.y = -dy;

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
    if (make_seg(thick, 
		 line->x[from_index], line->y[from_index],
		 line->x[to_index], line->y[to_index],
		 segs+seg_count)) {
      ++from_index;
      ++seg_count;
    }
    ++to_index;
  }
  if (line->closed) {
    if (make_seg(thick, 
		 line->x[from_index], line->y[from_index], 
		 line->x[0], line->y[0],
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
    intersect_two_segs(&segs[i+1].right, &segs[i].right);
  }
  if (closed) {
    intersect_two_segs(&segs[seg_count-1].left, &segs->left);
    intersect_two_segs(&segs->right, &segs[seg_count-1].right);
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

static void
line_end_square(i_polyline_t *poly, thick_seg *seg,
		line_seg *begin, line_seg *end, i_pen_thick_t *pen) {
  double scale = 0.5 / seg->length;
  i_polyline_add_point_xy(poly, line_x(begin, 1.0+scale),
			  line_y(begin, 1.0+scale));
  i_polyline_add_point_xy(poly, line_x(end, -scale),
  			  line_y(end, -scale));
}

static void
line_end_block(i_polyline_t *poly, thick_seg *seg,
		line_seg *begin, line_seg *end, i_pen_thick_t *pen) {
  double scale = pen->thickness * 0.5 / seg->length;
  i_polyline_add_point_xy(poly, line_x(begin, 1.0+scale),
			  line_y(begin, 1.0+scale));
  i_polyline_add_point_xy(poly, line_x(end, -scale),
  			  line_y(end, -scale));
}

static void
line_end_round(i_polyline_t *poly, thick_seg *seg,
		line_seg *begin, line_seg *end, i_pen_thick_t *pen) {
  double scale = pen->thickness * 0.5 / seg->length;
  double cx = (begin->b.x + end->a.x) / 2.0;
  double cy = (begin->b.y + end->a.y) / 2.0;
  double radius = pen->thickness / 2.0;
  double angle = atan2(begin->b.y - end->a.y, begin->b.x - end->a.x);
  double end_angle = angle + PI;
  int steps = pen->thickness > 8 ? pen->thickness : 8;
  double step = PI / steps;

  /* there's probably some clever trig method of doing this, but I don't
     see it */
  for (; angle < end_angle; angle += step) {
    i_polyline_add_point_xy(poly, cx + radius * cos(angle),
			    cy + radius * sin(angle));
  }
}

static void
line_end(i_polyline_t *poly, thick_seg *seg, 
	 line_seg *begin, line_seg *end, i_pen_thick_end_t end_type,
	 i_pen_thick_t *pen) {
  switch (end_type) {
  case i_pte_square:
  default:
    line_end_square(poly, seg, begin, end, pen);
    break;

  case i_pte_block:
    line_end_block(poly, seg, begin, end, pen);
    break;

  case i_pte_round:
    line_end_round(poly, seg, begin, end, pen);
    break;
  }
}

static i_polyline_t *
make_poly_cut(thick_seg *segs, int seg_count, int closed, 
	      i_pen_thick_t *pen) {
  int side_count = seg_count * 3 + 2; /* just rough */
  int i;
  int add_start = 1;
  i_polyline_t *poly = i_polyline_new_empty(1, side_count);

  /* up one side of the line */
  for (i = 0; i < seg_count; ++i) {
    thick_seg *seg = segs + i;
    double start = seg->left.start < 0 ? 0 : seg->left.start;
    double end = seg->left.end > 1.0 ? 1.0 : seg->left.end;

    //add_start=1;
    if (add_start) {
      i_polyline_add_point_xy(poly, 
			      line_x(&seg->left, start), 
			      line_y(&seg->left, start));
    }
    
    if (end > start) {
      i_polyline_add_point_xy(poly,
			      line_x(&seg->left, end),
			      line_y(&seg->left, end));
    }

    add_start = end == 1.0;
  }

  if (!closed) {
    thick_seg *seg = segs + seg_count - 1;
    line_end(poly, seg, &seg->left, &seg->right, pen->front, pen);
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
      i_polyline_add_point_xy(poly,
			      line_x(&seg->right, start),
			      line_y(&seg->right, start));
    }
    
    if (end > start) {
      i_polyline_add_point_xy(poly,
			      line_x(&seg->right, end),
			      line_y(&seg->right, end));
    }
    add_start = end == 1.0;
  }

  if (!closed) {
    line_end(poly, segs, &segs->right, &segs->left, pen->back, pen);
  }

  return poly;
}

static i_polyline_t *
make_poly_round(thick_seg *segs, int seg_count, int closed, 
	      i_pen_thick_t *pen) {
  int side_count = seg_count * 3 + 2; /* just rough */
  int i;
  int add_start = 1;
  i_polyline_t *poly = i_polyline_new_empty(1, side_count);
  double radius = pen->thickness / 2.0;
  int circle_steps = pen->thickness > 8 ? pen->thickness : 8;
  double step = PI / circle_steps / 2;

  /* up one side of the line */
  for (i = 0; i < seg_count; ++i) {
    thick_seg *seg = segs + i;
    double start = seg->left.start < 0 ? 0 : seg->left.start;
    double end = seg->left.end > 1.0 ? 1.0 : seg->left.end;

    if (add_start) {
      double startx = line_x(&seg->left, start);
      double starty = line_y(&seg->left, start);
#if 1
      if (i) { /* if not the first point */
	/* curve to the new point */
	double start_angle = atan2(-segs[i-1].cos_slope, -segs[i-1].sin_slope);
	double end_angle = atan2(-segs[i].cos_slope, -segs[i-1].sin_slope);
	double angle;
	double cx = (seg->left.a.x + seg->right.b.x) / 2.0;
	double cy = (seg->left.a.y + seg->right.b.y) / 2.0;

	if (end_angle < start_angle) 
	  end_angle += 2 * PI;
	printf("drawing round corners start %g end %g step %g\n", start_angle * 180/PI, end_angle * 180 / PI, step * 180 / PI);
	for (angle = start_angle + step; angle < end_angle; angle += step) {
	  i_polyline_add_point_xy(poly,
				  cx + radius * cos(angle),
				  cy + radius * sin(angle));
	  printf("pt %g %g\n", cx + radius * cos(angle), cy + radius * sin(angle));
	}
      }
#endif
      i_polyline_add_point_xy(poly, startx, starty);
    }
    
    if (end > start) {
      i_polyline_add_point_xy(poly,
			      line_x(&seg->left, end),
			      line_y(&seg->left, end));
    }

    add_start = end == 1.0;
  }

  if (!closed) {
    thick_seg *seg = segs + seg_count - 1;
    line_end(poly, seg, &seg->left, &seg->right, pen->front, pen);
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
      i_polyline_add_point_xy(poly,
			      line_x(&seg->right, start),
			      line_y(&seg->right, start));
    }
    
    if (end > start) {
      i_polyline_add_point_xy(poly,
			      line_x(&seg->right, end),
			      line_y(&seg->right, end));
    }
    add_start = end == 1.0;
  }

  if (!closed) {
    line_end(poly, segs, &segs->right, &segs->left, pen->back, pen);
  }

  return poly;
}

static void
marker(i_img *im, int x, int y, const i_color *c) {
  i_ppix(im, x, y, c);
  i_ppix(im, x+1, y, c);
  i_ppix(im, x, y+1, c);
  i_ppix(im, x-1, y, c);
  i_ppix(im, x, y-1, c);
}

static i_polyline_t *
thick_draw_line(i_img *im, i_pen_thick_t *thick, const i_polyline_t *line) {
  thick_seg *segs = mymalloc(sizeof(thick_seg) * line->point_count);
  int seg_count = fill_segs(thick, segs, line);
  i_polyline_t *poly;

  {
    int i;
    for (i = 0; i < line->point_count; ++i) {
      printf("%d: (%g, %g)\n", i, line->x[i], line->y[i]);
    }
  }
#if 0
  {
    int i;
    i_color blue, green;
      blue.rgba.r = blue.rgba.a = blue.rgba.g = 128;
      blue.rgba.b = 255;
      green.rgba.r = green.rgba.a = green.rgba.b = 128;
      green.rgba.g = 255;
      
    for (i = 0; i < seg_count; ++i) {
      i_line(im, 
	     line_x(&segs[i].left, segs[i].left.start),
	     line_y(&segs[i].left, segs[i].left.start),
	     line_x(&segs[i].left, segs[i].left.end),
	     line_y(&segs[i].left, segs[i].left.end),
	     &blue, 0);
      i_line(im, 
	     line_x(&segs[i].right, segs[i].right.start),
	     line_y(&segs[i].right, segs[i].right.start),
	     line_x(&segs[i].right, segs[i].right.end),
	     line_y(&segs[i].right, segs[i].right.end),
	     &green, 0);
    }
  }
#endif

#if 0
    {
      int i;
      i_color white;
      white.rgba.r = white.rgba.g = white.rgba.b = 255;
      for (i = 0; i < seg_count; ++i) {
	marker(im, floor(line_x(&segs[i].left, 0)+0.5), 
	       floor(line_y(&segs[i].left, 0)+0.5), &white);
	marker(im, floor(line_x(&segs[i].left, 1)+0.5), 
	       floor(line_y(&segs[i].left, 1)+0.5), &white);

	marker(im, floor(line_x(&segs[i].right, 0)+0.5), 
	       floor(line_y(&segs[i].right, 0)+0.5), &white);
	marker(im, floor(line_x(&segs[i].right, 1)+0.5), 
	       floor(line_y(&segs[i].right, 1)+0.5), &white);
      }
    }
#endif

#if 1
  if (seg_count > 1)
    intersect_segs(thick, segs, seg_count, line->closed);
#endif

#if 0
  {
    int i;
    i_color blue, green;
      blue.rgba.r = blue.rgba.a = blue.rgba.g = 128;
      blue.rgba.b = 255;
      green.rgba.r = green.rgba.a = green.rgba.b = 128;
      green.rgba.g = 255;
      
    for (i = 0; i < seg_count; ++i) {
      i_line(im, 
	     line_x(&segs[i].left, segs[i].left.start),
	     line_y(&segs[i].left, segs[i].left.start),
	     line_x(&segs[i].left, segs[i].left.end),
	     line_y(&segs[i].left, segs[i].left.end),
	     &blue, 0);
      i_line(im, 
	     line_x(&segs[i].right, segs[i].right.start),
	     line_y(&segs[i].right, segs[i].right.start),
	     line_x(&segs[i].right, segs[i].right.end),
	     line_y(&segs[i].right, segs[i].right.end),
	     &green, 0);
    }
  }
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

  switch (thick->corner) {
  case i_ptc_round:
    poly = make_poly_round(segs, seg_count, line->closed, thick);
    break;

  case i_ptc_cut:
  case i_ptc_30:
  default:
    poly = make_poly_cut(segs, seg_count, line->closed, thick);
    break;
  }

#if 1
  if (poly->point_count) {
#if 0
    {
      i_color red;
      int i;
      double *x = poly->x;
      double *y = poly->y;
      red.rgba.r = red.rgba.a = 255;
      red.rgba.g = red.rgba.b = 0;

      for (i = 0; i < poly->point_count-1; ++i) {
	i_line(im, x[i], y[i], x[i+1], y[i+1], &red, 0);
      }
      i_line(im, x[poly->point_count-1], y[poly->point_count-1], x[0], y[0], &red, 0);
    }
#endif
   
#if 1
    {
      int i;
      i_color white;
      white.rgba.r = white.rgba.g = white.rgba.b = 255;
      for (i = 0; i < poly->point_count; ++i) {
	marker(im, floor(poly->x[i]+0.5), floor(poly->y[i]+0.5), &white);
      }
    }
#endif
  }
#endif
  
  myfree(segs);

  return poly;
}

static int
thick_draw(i_pen_t *pen, i_img *im, int line_count, const i_polyline_t **lines) {
  i_pen_thick_t *thick = (i_pen_thick_t *)pen;
  int i;

  for (i = 0; i < line_count; ++i) {
    i_polyline_t *poly = thick_draw_line(im, thick, lines[i]);
    i_poly_aa_cfill(im, poly->point_count, poly->x, poly->y, thick->fill);
    i_polyline_delete(poly);
  }

  return 1;
}
