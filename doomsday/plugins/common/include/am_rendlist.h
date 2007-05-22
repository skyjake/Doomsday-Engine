/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2007 Daniel Swanson <danij@dengine.net>
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

/*
 * am_rendlist.h : Automap, rendering lists.
 */

#ifndef __COMMON_AUTOMAP_RENDLIST_H__
#define __COMMON_AUTOMAP_RENDLIST_H__

#include "dd_types.h"

extern boolean freezeMapRLs;

void    AM_ListRegister(void);

void    AM_ListInit(void);
void    AM_ListShutdown(void);

void    AM_RenderAllLists(float alpha);
void    AM_ClearAllLists(boolean destroy);


void    AM_AddLine(float x, float y, float x2, float y2, int color,
                   float alpha);
void    AM_AddLine4f(float x, float y, float x2, float y2,
                     float r, float g, float b, float a);
void    AM_AddQuad(float x1, float y1, float x2, float y2,
                   float x3, float y3, float x4, float y4,
                   float tc1st1, float tc1st2,
                   float tc2st1, float tc2st2,
                   float tc3st1, float tc3st2,
                   float tc4st1, float tc4st2,
                   float r, float g, float b, float a,
                   uint tex, boolean texIsPatchLumpNum, blendmode_t blend);

#endif
