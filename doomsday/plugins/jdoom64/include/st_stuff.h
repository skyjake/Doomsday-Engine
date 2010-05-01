/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * st_stuff.h: Statusbar code jDoom64 - specific.
 *
 * Does palette indicators as well (red pain/berserk, bright pickup)
 */

#ifndef __ST_STUFF_H__
#define __ST_STUFF_H__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS        (1)
#define STARTBONUSPALS      (9)
#define NUMREDPALS          (8)
#define NUMBONUSPALS        (4)

#define HUDBORDERX          (14)
#define HUDBORDERY          (18)

// Called by startup code.
void    ST_Register(void);
void    ST_Init(void);

void    ST_Ticker(timespan_t ticLength);
void    ST_Drawer(int player);

void    ST_Start(int player);
void    ST_Stop(int player);

// Called when it might be neccessary for the hud to unhide.
void    ST_HUDUnHide(int player, hueevent_t event);

void    ST_HUDSpriteSize(int sprite, int *w, int *h);

#endif
