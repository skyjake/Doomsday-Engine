/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * m_vector.h: Vector Math
 */

#ifndef __DOOMSDAY_MISC_VECTOR_H__
#define __DOOMSDAY_MISC_VECTOR_H__

typedef float vectorcomp_t;
typedef vectorcomp_t vec2_t[2];
typedef vectorcomp_t *pvec2_t;
typedef vec2_t *arvec2_t;

void 	V2_Set (pvec2_t vec, vectorcomp_t x, vectorcomp_t y);
void 	V2_SetFixed (pvec2_t vec, fixed_t x, fixed_t y);
float	V2_Length (const pvec2_t vector);
float	V2_Distance (const pvec2_t a, const pvec2_t b);
float	V2_Normalize (pvec2_t vector);
void  	V2_Copy (pvec2_t dest, const pvec2_t src);
void	V2_Scale (pvec2_t vector, float scalar);
void 	V2_Sum (pvec2_t dest, const pvec2_t src1, const pvec2_t src2);
void 	V2_Subtract (pvec2_t dest, const pvec2_t src1, const pvec2_t src2);
float 	V2_DotProduct (const pvec2_t a, const pvec2_t b);
float 	V2_ScalarProject (const pvec2_t a, const pvec2_t b);
void 	V2_Project (pvec2_t dest, const pvec2_t a, const pvec2_t b);
boolean	V2_IsParallel (const pvec2_t a, const pvec2_t b);
boolean	V2_IsZero (const pvec2_t vec);
float 	V2_Intersection (const pvec2_t p1, const pvec2_t delta1,
						 const pvec2_t p2, const pvec2_t delta2,
						 pvec2_t point);
float 	V2_Intercept (const pvec2_t a, const pvec2_t b,
					  const pvec2_t c, const pvec2_t d,
					  pvec2_t point);
boolean V2_Intercept2 (const pvec2_t a, const pvec2_t b,
					   const pvec2_t c, const pvec2_t d,
					   pvec2_t point, float *abFrac, float *cdFrac);
void	V2_Lerp (pvec2_t dest, const pvec2_t a, const pvec2_t b, float c);
void 	V2_InitBox (arvec2_t box, const pvec2_t point);
void 	V2_AddToBox (arvec2_t box, const pvec2_t point);

#endif 
