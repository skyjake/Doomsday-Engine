/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * rend_decor.h: Decorations
 */

#ifndef __DOOMSDAY_RENDER_DECOR_H__
#define __DOOMSDAY_RENDER_DECOR_H__

extern boolean	useDecorations;
extern float	decorWallMaxDist; // No decorations are visible beyond this.
extern float	decorPlaneMaxDist;
extern float	decorWallFactor;
extern float	decorPlaneFactor;
extern float	decorFadeAngle;

void	Rend_InitDecorationsForFrame(void);
void	Rend_ProjectDecorations(void);

#endif 
