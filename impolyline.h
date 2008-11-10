#ifndef IMPOLYLINE_H_
#define IMPOLYLINE_H_

#include "imdatatypes.h"
#include <stddef.h>

extern i_polyline_t *
i_polyline_new(int closed, int point_count, const i_point_t *points);
extern i_polyline_t *
i_polyline_new_empty(int closed, int alloc);
extern void
i_polyline_delete(i_polyline_t *poly);
extern int
i_polyline_add_points(i_polyline_t *poly, int point_count, 
		      const i_point_t *points);
extern int
i_polyline_add_point(i_polyline_t *poly, const i_point_t *point);
extern int
i_polyline_add_point_xy(i_polyline_t *poly, double x, double y);
extern void
i_polyline_dump(const i_polyline_t *poly);

#endif
