/**\file p_intercept.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Line/Object Interception
 */

#ifndef LIBDENG_PLAY_INTERCEPT_H
#define LIBDENG_PLAY_INTERCEPT_H

void            P_ClearIntercepts(void);
intercept_t*    P_AddIntercept(float frac, intercepttype_t type, void* ptr);
void            P_CalcInterceptDistances(const divline_t* strace);

int             P_TraverseIntercepts(traverser_t func, float maxfrac);

#endif /* LIBDENG_PLAY_INTERCEPT_H */
