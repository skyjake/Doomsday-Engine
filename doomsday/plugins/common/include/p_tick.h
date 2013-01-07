/**\file p_tick.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * Common top-level tick stuff.
 */

#ifndef LIBCOMMON_P_TICK_H
#define LIBCOMMON_P_TICK_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int mapTime;
extern int actualMapTime;
extern int timerGame;

void    P_RunPlayers(timespan_t ticLength);
boolean P_IsPaused(void);
void    P_DoTick(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_P_TICK_H */
