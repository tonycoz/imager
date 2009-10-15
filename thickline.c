#include "imager.h"
#include "imager.h"
#include <math.h>
#include "impolyline.h"

#define DEBUG_TL_MARKERS 0
#define DEBUG_TL_TRACE 0
#define DEBUG_TL_TRACE_INITIAL 0
#define DEBUG_TL_TRACE_FINAL 0
#define DEBUG_TL_NOFILL 0

/* cos(150 degrees) */
#define COS_150_DEG -0.866025403784439

/* cos(135 degrees) */
#define COS_135_DEG -0.707106781186547

/* cos(120 degrees) */
#define COS_120_DEG -0.5

typedef struct {
  i_pen_t base;
  i_pen_thick_corner_t corner;
  i_pen_thick_end_t front;
  i_pen_thick_end_t back;
  double thickness;
  i_pte_custom_t custom_front;
  i_pte_custom_t custom_back;
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

static i_point_t simple_arrow_head_pts[] = {
  { -1, 0 },
  { 0, 1 },
  { 1, 0 }
};

static const i_pte_custom_t simple_arrow_head = {
  1.0,
  1.0,
  sizeof(simple_arrow_head_pts) / sizeof(*simple_arrow_head_pts),
  simple_arrow_head_pts
};

static i_point_t pointy_arrow_head_pts[] = {
  { -1.1, -0.3 },
  { 0, 1.8 },
  { 1.1, -0.3 }
};

static const i_pte_custom_t pointy_arrow_head = {
  1.0,
  1.8,
  sizeof(pointy_arrow_head_pts) / sizeof(*pointy_arrow_head_pts),
  pointy_arrow_head_pts
};

static i_point_t simple_arrow_base_pts[] = {
  { -1, 0.5 },
  { -1, 1.5 },
  { 0, 0.5 },
  { 1, 1.5 },
  { 1, 0.5 }
};

static const i_pte_custom_t simple_arrow_base = {
  1.0,
  1.5,
  sizeof(simple_arrow_base_pts) / sizeof(*simple_arrow_base_pts),
  simple_arrow_base_pts
};

static i_pen_t *
i_new_thick_pen_low(double thickness,
		    i_fill_t *fill,
		    i_pen_thick_corner_t corner, 
		    i_pen_thick_end_t front,
		    i_pen_thick_end_t back,
		    i_pte_custom_t *custom_front,
		    i_pte_custom_t *custom_back) {
  i_pen_thick_t *pen = mymalloc(sizeof(i_pen_thick_t));
  int i;

  pen->base.vtable = &thick_vtable;
  pen->corner = corner;
  pen->front = front;
  pen->back = back;
  pen->thickness = thickness;
  pen->fill = fill;
  if (custom_front && front == i_pte_custom) {
    i_point_t *pts;
    pen->custom_front = *custom_front;
    pts = mymalloc(sizeof(i_point_t) * custom_front->pt_count);
    for (i = 0; i < custom_front->pt_count; ++i) {
      pts[i] = custom_front->pts[i];
    }
    pen->custom_front.pts = pts;
  }
  else {
    pen->custom_front.pt_count = 0;
    pen->custom_front.pts = NULL;
    if (pen->front == i_pte_custom)
      pen->front = i_pte_square;
  }

  if (custom_back && back == i_pte_custom) {
    i_point_t *pts;
    pen->custom_back = *custom_back;
    pts = mymalloc(sizeof(i_point_t) * custom_back->pt_count);
    for (i = 0; i < custom_back->pt_count; ++i) {
      pts[i] = custom_back->pts[i];
    }
    pen->custom_back.pts = pts;
  }
  else {
    pen->custom_back.pt_count = 0;
    pen->custom_back.pts = NULL;
    if (pen->back == i_pte_custom)
      pen->back = i_pte_square;
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
		      i_pte_custom_t *custom_front,
		      i_pte_custom_t *custom_back) {
  i_fill_t *fill = i_new_fill_solid(color, combine);

  return i_new_thick_pen_low(thickness, fill, corner, front, back, 
			     custom_front, custom_back);
}
		
i_pen_t *
i_new_thick_pen_fill(double thickness,
		     i_fill_t *fill,
		     i_pen_thick_corner_t corner,
		     i_pen_thick_end_t front,
		     i_pen_thick_end_t back,
		     i_pte_custom_t *custom_front,
		     i_pte_custom_t *custom_back) {
  i_fill_t *cloned = i_fill_clone(fill);

  return i_new_thick_pen_low(thickness, cloned, corner, front, back, 
			     custom_front, custom_back);
}
		
static void
thick_destroy(i_pen_t *pen) {
  i_pen_thick_t *thick = (i_pen_thick_t *)pen;

  if (thick->custom_front.pts)
    myfree((void *)thick->custom_front.pts);
  if (thick->custom_back.pts)
    myfree((void *)thick->custom_back.pts);
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
			      &thick->custom_front,
			      &thick->custom_back);
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

#if DEBUG_TL_TRACE  
  printf("make_seg thick %g (%g, %g) - (%g, %g)\n", thick->thickness, ax, ay, bx, by);
#endif

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

#if DEBUG_TL_TRACE
  printf("seg len %g sin %g cos %g\n", seg->length, seg->sin_slope, seg->cos_slope);
  printf("    left (%g, %g) - (%g, %g)\n", seg->left.a.x, seg->left.a.y, seg->left.b.x, seg->left.b.y);
  printf("   right (%g, %g) - (%g, %g)\n", seg->right.a.x, seg->right.a.y, seg->right.b.x, seg->right.b.y);
#endif

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

#if DEBUG_TL_TRACE
  printf("P1(%g, %g) P2(%g,%g) P3(%g,%g) P4(%g,%g)\n",
	 first->a.x, first->a.y, first->b.x, first->b.y, 
	 next->a.x, next->a.y, next->b.x, next->b.y);
#endif
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

#if DEBUG_TL_TRACE
  printf("intersect -> (%g, %g)\n", ufirst, unext);
#endif

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
line_end_custom(i_polyline_t *poly, thick_seg *seg,
		line_seg *begin, line_seg *end, i_pen_thick_t *pen,
		const i_pte_custom_t *line_end) {
  double scale = pen->thickness;
  /* perhaps these should be passed in */
  double cos_slope = (begin->b.x - begin->a.x) / seg->length;
  double sin_slope = (begin->b.y - begin->a.y) / seg->length;
  double fit_frac;
  int i;
  double scaled_cos, scaled_sin;
  double start_x, start_y;
  double end_x, end_y;
  double base_x, base_y;

  if (scale < line_end->min_scale)
    scale = line_end->min_scale;

  scaled_cos = cos_slope * scale;
  scaled_sin = sin_slope * scale;

  fit_frac = line_end->fit_space * scale / seg->length;
  if (begin->start > 1.0 - fit_frac)
    fit_frac = 1.0 - begin->start;
  if (end->end < fit_frac)
    fit_frac = end->end;

  start_x = line_x(begin, 1.0 - fit_frac);
  start_y = line_y(begin, 1.0 - fit_frac);

  end_x = line_x(end, fit_frac);
  end_y = line_y(end, fit_frac);

  base_x = (start_x + end_x) / 2;
  base_y = (start_y + end_y) / 2;

  i_polyline_add_point_xy(poly, start_x, start_y);

  for (i = 0; i < line_end->pt_count; ++i) {
    double px = line_end->pts[i].x;
    double py = line_end->pts[i].y;
    i_polyline_add_point_xy(poly,
			    base_x + py * scaled_cos - px * scaled_sin,
			    base_y + py * scaled_sin + px * scaled_cos);
  }
  
  i_polyline_add_point_xy(poly, end_x, end_y);
}

static void
line_end(i_polyline_t *poly, thick_seg *seg, 
	 line_seg *begin, line_seg *end, i_pen_thick_end_t end_type,
	 i_pen_thick_t *pen, i_pte_custom_t *end_type_custom) {
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

  case i_pte_sarrowh:
    line_end_custom(poly, seg, begin, end, pen, &simple_arrow_head);
    break;

  case i_pte_parrowh:
    line_end_custom(poly, seg, begin, end, pen, &pointy_arrow_head);
    break;

  case i_pte_sarrowb:
    line_end_custom(poly, seg, begin, end, pen, &simple_arrow_base);
    break;

  case i_pte_custom:
    line_end_custom(poly, seg, begin, end, pen, end_type_custom);
    break;
  }
}

static i_polyline_t *
make_poly_cut30(thick_seg *segs, int seg_count, int closed, 
	      i_pen_thick_t *pen) {
  int side_count = seg_count * 3 + 2; /* just rough */
  int i;
  int add_start = 0;
  i_polyline_t *poly = i_polyline_new_empty(1, side_count);

  /* up one side of the line */
  for (i = 0; i < seg_count; ++i) {
    thick_seg *seg = segs + i;
    double start = seg->left.start < 0 ? 0 : seg->left.start;
    double end = seg->left.end > 1.0 ? 1.0 : seg->left.end;

    if (add_start) {
      i_polyline_add_point_xy(poly, 
			      line_x(&seg->left, start), 
			      line_y(&seg->left, start));
    }

    if (end > start && i < seg_count-1) {
      if (seg->left.end > 1) {
	thick_seg *next = segs + i + 1;
	double cos_x_m_y = 
	  seg->cos_slope * next->cos_slope + seg->sin_slope * next->sin_slope;
        if (cos_x_m_y > COS_150_DEG) {
	  end = seg->left.end;
	}
	else {
	  add_start = 1;
	}
      }
      else 
	add_start = end == 1.0;

      i_polyline_add_point_xy(poly,
			      line_x(&seg->left, end),
			      line_y(&seg->left, end));
    }
    else 
      add_start = end == 1.0;
  }

  if (closed) {
    if (add_start) {
      thick_seg *last_seg = segs + seg_count - 1;
      double cos_x_m_y = 
	last_seg->cos_slope * segs->cos_slope + last_seg->sin_slope * segs->sin_slope;
      if (cos_x_m_y > COS_150_DEG) {
	double end = last_seg->left.end;
	i_polyline_add_point_xy(poly,
				line_x(&last_seg->left, end),
				line_y(&last_seg->left, end));
	i_polyline_add_point_xy(poly,
				line_x(&segs->left, 0),
				line_y(&segs->left, 0));
      }
      else {
	add_start = 1;
      }
    }
  }
  else {
    thick_seg *seg = segs + seg_count - 1;
    line_end(poly, seg, &seg->left, &seg->right, pen->front, pen, &pen->custom_front);
  }

  add_start = 0;
  /* and down the other */
  for (i = seg_count-1; i >= 0; --i) {
    thick_seg *seg = segs + i;
    double start = seg->right.start < 0 ? 0 : seg->right.start;
    double end = seg->right.end > 1.0 ? 1.0 : seg->right.end;
#if DEBUG_TL_TRACE
    printf("  right %g - %g\n", start, end);
#endif

    //add_start=1;
    if (add_start) {
      i_polyline_add_point_xy(poly,
			      line_x(&seg->right, start),
			      line_y(&seg->right, start));
    }
    
    if (end > start && i > 0) {
      if (seg->right.end > 1) {
	thick_seg *next = segs + i - 1;
	double cos_x_m_y = 
	  seg->cos_slope * next->cos_slope + seg->sin_slope * next->sin_slope;
        if (cos_x_m_y > COS_150_DEG) {
	  end = seg->right.end;
	}
	else {
	  add_start = 1;
	}
      }
      else 
	add_start = end == 1.0;
	
      i_polyline_add_point_xy(poly,
			      line_x(&seg->right, end),
			      line_y(&seg->right, end));
    }
    add_start = end == 1.0;
  }

  if (closed) {
    if (add_start) {
      thick_seg *last_seg = segs + seg_count - 1;
      double cos_x_m_y = 
	last_seg->cos_slope * segs->cos_slope + last_seg->sin_slope * segs->sin_slope;
      if (cos_x_m_y > COS_150_DEG) {
	double start = last_seg->right.start;
	i_polyline_add_point_xy(poly,
				line_x(&last_seg->right, start),
				line_y(&last_seg->right, start));
      }
#if 0
      i_polyline_add_point_xy(poly,
			      line_x(&last_seg->right, 0.0),
			      line_y(&last_seg->right, 0.0));
#else
      i_polyline_add_point_xy(poly,
			      last_seg->right.a.x,
			      last_seg->right.a.y);
#endif
    }
  }
  else {
    line_end(poly, segs, &segs->right, &segs->left, pen->back, pen, &pen->custom_back);
  }

  return poly;
}

static i_polyline_t *
make_poly_cut(thick_seg *segs, int seg_count, int closed, 
	      i_pen_thick_t *pen) {
  int side_count = seg_count * 3 + 2; /* just rough */
  int i;
  int add_start = 0;
  i_polyline_t *poly = i_polyline_new_empty(1, side_count);
  double left0x = 0, left0y = 0, right0x = 0, right0y = 0;

  /* up one side of the line */
  /* starting point */
  if (closed) {
    thick_seg *before = segs + seg_count - 1;
    thick_seg *after = segs;
    if (before->left.end > 1.0) {
      i_polyline_add_point_xy(poly, 
			      line_x(&after->left, 0.0),
			      line_y(&after->left, 0.0));
    }
    else {
      i_polyline_add_point_xy(poly, 
			      line_x(&before->left, before->left.end),
			      line_y(&before->left, before->left.end));
    }
  }

  /* for each corner */
  for (i = 0; i < seg_count-1; ++i) {
    thick_seg *before = segs + i;
    thick_seg *after = before + 1;
    if (before->left.end > 1.0) {
      /* outside corner */
      i_polyline_add_point_xy(poly, 
			      line_x(&before->left, 1.0),
			      line_y(&before->left, 1.0));
      i_polyline_add_point_xy(poly, 
			      line_x(&after->left, 0.0),
			      line_y(&after->left, 0.0));
    }
    else {
      /* inside corner */
      i_polyline_add_point_xy(poly, 
			      line_x(&before->left, before->left.end),
			      line_y(&before->left, before->left.end));
    }
  }

  if (closed) {
    thick_seg *before = segs + seg_count - 1;
    thick_seg *after = segs;
    if (before->left.end > 1.0) {
      /* outside corner */
      i_polyline_add_point_xy(poly, 
			      line_x(&before->left, 1.0),
			      line_y(&before->left, 1.0));
      i_polyline_add_point_xy(poly, 
			      line_x(&after->left, 0.0),
			      line_y(&after->left, 0.0));
    }
    else {
      /* inside corner */
      i_polyline_add_point_xy(poly, 
			      line_x(&before->left, before->left.end),
			      line_y(&before->left, before->left.end));
    }
  }
  else {
    thick_seg *seg = segs + seg_count - 1;
    line_end(poly, seg, &seg->left, &seg->right, pen->front, pen,
	     &pen->custom_front);
  }
  /* in reverse, for the right side */
  if (closed) {
    thick_seg *after = segs + seg_count - 1;
    thick_seg *before = segs;
    if (before->right.end > 1.0) {
      i_polyline_add_point_xy(poly,
			      line_x(&after->right, 0.0),
			      line_y(&after->right, 0.0));
    }
    else {
      i_polyline_add_point_xy(poly,
			      line_x(&after->right, after->right.start),
			      line_y(&after->right, after->right.start));
    }
  }
  for (i = seg_count-2; i >= 0; --i) {
    thick_seg *after = segs + i;
    thick_seg *before = after + 1;
    if (before->right.end > 1.0) {
      /* outside corner */
      i_polyline_add_point_xy(poly,
			      line_x(&before->right, 1.0),
			      line_y(&before->right, 1.0));
      i_polyline_add_point_xy(poly,
			      line_x(&after->right, 0.0),
			      line_y(&after->right, 0.0));
    }
    else {
      /* inside corner */
      i_polyline_add_point_xy(poly,
			      line_x(&before->right, before->right.end),
			      line_y(&before->right, before->right.end));
    }
  }

  if (closed) {
    thick_seg *before = segs;
    thick_seg *after = segs + seg_count - 1;
    if (before->right.end > 1.0) {
      /* outside corner */
      i_polyline_add_point_xy(poly,
			      line_x(&before->right, 1.0),
			      line_y(&before->right, 1.0));
      i_polyline_add_point_xy(poly,
			      line_x(&after->right, 0.0),
			      line_y(&after->right, 0.0));
    }
    else {
      /* inside corner */
      i_polyline_add_point_xy(poly,
			      line_x(&before->right, before->right.end),
			      line_y(&before->right, before->right.end));
    }
  }
  else {
    line_end(poly, segs, &segs->right, &segs->left, pen->back, pen,
	     &pen->custom_back);
  }

  return poly;
}

static void
corner_curve(i_polyline_t *poly, const thick_seg *from, const thick_seg *to, double step, double radius) {
  /* curve to the new point 
     start_angle is the line angle - 90 degrees
     sin(start+90) = sin(start).cos(-90) + cos(start).sin(-90)
     = -cos(start)
     cos(start+90) = cos(start).cos(-90) - sin(start).sin(-90)
     = sin(start)
     same for the end angle, but on the other seg
  */
  double start_angle = atan2(-from->cos_slope, from->sin_slope);
  double end_angle = atan2(-to->cos_slope, to->sin_slope);
  double angle;
  double cx = (to->left.a.x + to->right.b.x) / 2.0;
  double cy = (to->left.a.y + to->right.b.y) / 2.0;
  
#if DEBUG_TL_TRACE
  fprintf(stderr, "start %g end %g\n", start_angle, end_angle);
#endif
  
  if (end_angle < start_angle) 
    end_angle += 2 * PI;
#if DEBUG_TL_TRACE
  printf("drawing round corners start %g end %g step %g\n", start_angle * 180/PI, end_angle * 180 / PI, step * 180 / PI);
#endif
  for (angle = start_angle + step; angle < end_angle; angle += step) {
    i_polyline_add_point_xy(poly,
			    cx + radius * cos(angle),
			    cy + radius * sin(angle));
#if DEBUG_TL_TRACE
    printf("pt %g %g\n", cx + radius * cos(angle), cy + radius * sin(angle));
#endif
  }
}

static void
corner_curve2(i_polyline_t *poly, const thick_seg *from, const thick_seg *to, double step, double radius) {
  /* curve to the new point 
     start_angle is the line angle - 90 degrees
     sin(start+90) = sin(start).cos(-90) + cos(start).sin(-90)
     = -cos(start)
     cos(start+90) = cos(start).cos(-90) - sin(start).sin(-90)
     = sin(start)
     same for the end angle, but on the other seg
  */
  double start_angle = atan2(from->cos_slope, -from->sin_slope);
  double end_angle = atan2(to->cos_slope, -to->sin_slope);
  double angle;
  double cx = (to->right.a.x + to->left.b.x) / 2.0;
  double cy = (to->right.a.y + to->left.b.y) / 2.0;
  
#if DEBUG_TL_TRACE
  fprintf(stderr, "start %g end %g\n", start_angle, end_angle);
#endif
  
  if (end_angle < start_angle) 
    end_angle += 2 * PI;
#if DEBUG_TL_TRACE
  printf("drawing round corners start %g end %g step %g\n", start_angle * 180/PI, end_angle * 180 / PI, step * 180 / PI);
#endif
  for (angle = start_angle + step; angle < end_angle; angle += step) {
    i_polyline_add_point_xy(poly,
			    cx + radius * cos(angle),
			    cy + radius * sin(angle));
#if DEBUG_TL_TRACE
    printf("pt %g %g\n", cx + radius * cos(angle), cy + radius * sin(angle));
#endif
  }
}

static i_polyline_t *
make_poly_round(thick_seg *segs, int seg_count, int closed, 
	      i_pen_thick_t *pen) {
  int side_count = seg_count * 3 + 2; /* just rough */
  int i;
  int add_start = 0;
  i_polyline_t *poly = i_polyline_new_empty(1, side_count);
  double radius = pen->thickness / 2.0;
  int circle_steps = pen->thickness > 8 ? pen->thickness : 8;
  double step = PI / circle_steps * 2;

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
	corner_curve(poly, segs+i-1, segs+i, step, radius);
      }
#endif
      i_polyline_add_point_xy(poly, startx, starty);
    }
    
    if (end > start && i != seg_count-1) {
      i_polyline_add_point_xy(poly,
			      line_x(&seg->left, end),
			      line_y(&seg->left, end));
    }

    add_start = end == 1.0;
  }

  if (closed) {
    /* continue the curve around */
    if (add_start) {
      corner_curve(poly, segs+seg_count-1, segs, step, radius);
      i_polyline_add_point_xy(poly, segs[0].left.a.x, segs[0].left.a.y);
    }
  }
  else {
    thick_seg *seg = segs + seg_count - 1;
    line_end(poly, seg, &seg->left, &seg->right, pen->front, pen,
	     &pen->custom_front);
  }

  add_start = 0;
  /* and down the other */
  for (i = seg_count-1; i >= 0; --i) {
    thick_seg *seg = segs + i;
    double start = seg->right.start < 0 ? 0 : seg->right.start;
    double end = seg->right.end > 1.0 ? 1.0 : seg->right.end;
#if DEBUG_TL_TRACE
    printf("  right %g - %g\n", start, end);
#endif

    if (add_start) {
      double startx = line_x(&seg->right, start);
      double starty = line_y(&seg->right, start);
#if 1
      if (i != seg_count-1) { /* if not the first point */
	corner_curve2(poly, segs+i+1, segs+i, step, radius);
      }
#endif
      i_polyline_add_point_xy(poly, startx, starty);
    }
#if 0
    if (add_start) {
      i_polyline_add_point_xy(poly,
			      line_x(&seg->right, start),
			      line_y(&seg->right, start));
    }
#endif
    
    if (end > start && i != 0) {
      i_polyline_add_point_xy(poly,
			      line_x(&seg->right, end),
			      line_y(&seg->right, end));
    }
    add_start = end == 1.0;
  }

  if (closed) {
    if (add_start) {
      corner_curve2(poly, segs, segs+seg_count-1, step, radius);
      i_polyline_add_point_xy(poly, segs[seg_count-1].right.a.x, segs[seg_count-1].right.a.y);
    }
  }
  else {
    line_end(poly, segs, &segs->right, &segs->left, pen->back, pen,
	     &pen->custom_back);
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

#if DEBUG_TL_TRACE
  {
    int i;
    for (i = 0; i < line->point_count; ++i) {
      printf("%d: (%g, %g)\n", i, line->x[i], line->y[i]);
    }
  }
#endif
#if DEBUG_TL_MARKERS
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

#if DEBUG_TL_TRACE
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

#if DEBUG_TL_TRACE_INITIAL
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

  case i_ptc_30:
    poly = make_poly_cut30(segs, seg_count, line->closed, thick);
    break;

  case i_ptc_cut:
  default:
    poly = make_poly_cut(segs, seg_count, line->closed, thick);
    break;
  }

#if DEBUG_TL_TRACE_FINAL
  if (poly->point_count) {
#if 1
    {
      i_color red;
      int i;
      double *x = poly->x;
      double *y = poly->y;
      red.rgba.r = red.rgba.a = 255;
      red.rgba.g = red.rgba.b = 0;

      for (i = 0; i < poly->point_count-1; ++i) {
	i_line(im, x[i]+0.5, y[i]+0.5, x[i+1]+0.5, y[i+1]+0.5, &red, 0);
      }
      i_line(im, x[poly->point_count-1]+0.5, y[poly->point_count-1]+0.5, x[0]+0.5, y[0]+0.5, &red, 0);
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
#if 0
  {
      i_color white;
      white.rgba.r = white.rgba.g = white.rgba.b = 255;
    marker(im, floor(poly->x[0]+0.5), floor(poly->y[0]+0.5), &white);
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
    //i_polyline_dump(poly);
#if !DEBUG_TL_NOFILL
    i_poly_aa_cfill(im, poly->point_count, poly->x, poly->y, thick->fill);
#endif
    i_polyline_delete(poly);
  }

  return 1;
}
