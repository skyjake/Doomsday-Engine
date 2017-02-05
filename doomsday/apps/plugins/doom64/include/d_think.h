/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * d_think.h: MapObj data.
 *
 * Map Objects or mobjs are actors, entities, thinker, take-your-pick...
 * anything that moves, acts, or suffers state changes of more or less
 * violent nature.
 */

#ifndef __D_THINK__
#define __D_THINK__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

/**
 * Experimental stuff.
 * To compile this as "ANSI C with classes" we will need to handle the
 * various action functions cleanly.
 */
typedef void    (*actionf_v)  ();
typedef void    (*actionf_p1) (void *);
typedef void    (*actionf_p2) (void *, void *);

#define NOPFUNC             ((actionf_v) (-1))

typedef union {
    actionf_p1      acp1;
    actionf_v       acv;
    actionf_p2      acp2;
} actionf_t;

#endif
