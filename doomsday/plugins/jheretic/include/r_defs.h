/**\file
 *\section Copyright and License Summary
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 * Copyright © 2006 Jaakko Keränen <skyjake@dengine.net>
 * Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/*
 * shared data struct definitions.
 */

#ifndef __R_DEFS__
#define __R_DEFS__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

// Screenwidth.
#include "doomdef.h"

#include "p_xg.h"

// SECTORS do store MObjs anyway.
#include "p_mobj.h"

#ifdef __GNUG__
#pragma interface
#endif

//
// INTERNAL MAP TYPES
//  used by play and refresh
//

//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
typedef struct xsector_s {
    short           special;
    short           tag;

    // 0 = untraversed, 1,2 = sndlines -1
    int             soundtraversed;

    // thing that made a sound (or null)
    struct mobj_s  *soundtarget;

    // thinker_t for reversable actions
    void           *specialdata;

    // stone, metal, heavy, etc...
    byte       seqType;       // NOT USED ATM

    int             origfloor, origceiling, origlight;
    byte            origrgb[3];
    xgsector_t     *xg;

} xsector_t;

typedef struct xline_s {
    // Animation related.
    short           special;
    short           tag;
    // thinker_t for reversable actions
    void           *specialdata;

    // Extended generalized lines.
    xgline_t       *xg;
} xline_t;

extern xsector_t *xsectors;
extern xline_t *xlines;

xline_t*    P_XLine(line_t* line);
xsector_t*  P_XSector(sector_t* sector);
xsector_t*  P_XSectorOfSubsector(subsector_t* sub);
#endif
