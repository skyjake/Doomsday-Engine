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
 * doomdata.h: Thing and line attributes.
 */

#ifndef __DOOMDATA__
#define __DOOMDATA__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

// Base plane ids.
enum {
    PLN_FLOOR,
    PLN_CEILING
};

//
// Line attributes.
//

// Blocks monsters only.
#define ML_BLOCKMONSTERS        0x0002

/**
 * If not present on a two-sided line suppress the back sector and instead
 * consider the line as if it were one-sided. For mod compatibility purposes.
 */
#define ML_TWOSIDED             0x0004

// In AutoMap: don't map as two sided: IT'S A SECRET!
#define ML_SECRET               0x0020

// Sound rendering: don't let sound cross two of these.
#define ML_SOUNDBLOCK           0x0040

// Don't draw on the automap at all.
#define ML_DONTDRAW             0x0080

// Set if already seen, thus drawn in automap.
#define ML_MAPPED               0x0100

// Allows a USE action to pass through a line with a special
#define ML_PASSUSE              0x0200

// If set allows any mobj to trigger the line's special
#define ML_ALLTRIGGER           0x0400

#define ML_VALID_MASK           (ML_BLOCKMONSTERS|ML_TWOSIDED|ML_SECRET|ML_SOUNDBLOCK|ML_DONTDRAW|ML_MAPPED|ML_PASSUSE|ML_ALLTRIGGER)

// Special activation types:
#define SPAC_CROSS              0 // Player crosses line.
#define SPAC_USE                1 // Player uses line.
#define SPAC_IMPACT             3 // Projectile hits line.

#endif
