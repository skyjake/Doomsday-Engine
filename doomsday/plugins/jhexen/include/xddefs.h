/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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

/**
 * xddefs.h:
 */

#ifndef __XDDEFS__
#define __XDDEFS__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

// Base plane ids.
enum {
    PLN_FLOOR,
    PLN_CEILING
};

#define ML_BLOCKMONSTERS    0x0002
#define ML_SECRET           0x0020 // don't map as two sided: IT'S A SECRET!
#define ML_SOUNDBLOCK       0x0040 // don't let sound cross two of these
#define ML_DONTDRAW         0x0080 // don't draw on the automap
#define ML_MAPPED           0x0100 // set if already drawn in automap
#define ML_REPEAT_SPECIAL   0x0200 // special is repeatable
#define ML_SPAC_SHIFT       10
#define ML_SPAC_MASK        0x1c00
#define GET_SPAC(flags) ((flags&ML_SPAC_MASK)>>ML_SPAC_SHIFT)

// Special activation types
#define SPAC_CROSS      0          // when player crosses line
#define SPAC_USE        1          // when player uses line
#define SPAC_MCROSS     2          // when monster crosses line
#define SPAC_IMPACT     3          // when projectile hits line
#define SPAC_PUSH       4          // when player/monster pushes line
#define SPAC_PCROSS     5          // when projectile crosses line

#endif                          // __XDDEFS__
