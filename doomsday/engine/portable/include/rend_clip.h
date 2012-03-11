/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * rend_clip.h: Clipper
 */

#ifndef __DOOMSDAY_RENDER_CLIP_H__
#define __DOOMSDAY_RENDER_CLIP_H__

#include "m_bams.h"

extern int devNoCulling;

void            C_Init(void);
boolean         C_IsFull(void);
void            C_ClearRanges(void);
void            C_Ranger(void); // Debugging aid.
int             C_SafeAddRange(binangle_t startAngle, binangle_t endAngle);
boolean         C_IsPointVisible(float x, float y, float height);

// Add a segment relative to the current viewpoint.
void            C_AddViewRelSeg(float x1, float y1, float x2, float y2);

// Add an occlusion segment relative to the current viewpoint.
void            C_AddViewRelOcclusion(float *v1, float *v2, float height,
                                      boolean tophalf);

// Check a segment relative to the current viewpoint.
int             C_CheckViewRelSeg(float x1, float y1, float x2, float y2);

// Returns 1 if the specified angle is visible.
int             C_IsAngleVisible(binangle_t bang);

// Returns 1 if the BSP leaf might be visible.
int             C_CheckBspLeaf(BspLeaf* bspLeaf);

#endif
