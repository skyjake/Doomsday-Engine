//===========================================================================
// R_UTIL.H
//===========================================================================
#ifndef __DOOMSDAY_REFRESH_UTIL_H__
#define __DOOMSDAY_REFRESH_UTIL_H__

int			R_PointOnSide (fixed_t x, fixed_t y, node_t *node);
int         R_PointOnSegSide (fixed_t x, fixed_t y, seg_t *line);
angle_t		R_PointToAngle (fixed_t x, fixed_t y);
angle_t		R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
fixed_t		R_PointToDist (fixed_t x, fixed_t y);
subsector_t *R_PointInSubsector (fixed_t x, fixed_t y);


#endif 