/** @file thinkerinfo.h  Game save thinker info.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBCOMMON_SAVESTATE_THINKERINFO_H
#define LIBCOMMON_SAVESTATE_THINKERINFO_H

#include "common.h"
#ifdef __cplusplus
#  include "mapstatereader.h"
#  include "mapstatewriter.h"
#endif

/**
 * Original indices must remain unchanged!
 * Added new think classes to the end.
 */
typedef enum thinkclass_e {
    TC_NULL = -1,
    TC_END,
    TC_MOBJ,
    TC_XGMOVER,
    TC_CEILING,
    TC_DOOR,
    TC_FLOOR,
    TC_PLAT,
#if __JHEXEN__
    TC_INTERPRET_ACS,
    TC_FLOOR_WAGGLE,
    TC_LIGHT,
    TC_PHASE,
    TC_BUILD_PILLAR,
    TC_ROTATE_POLY,
    TC_MOVE_POLY,
    TC_POLY_DOOR,
#else
    TC_FLASH,
    TC_STROBE,
# if __JDOOM__ || __JDOOM64__
    TC_GLOW,
    TC_FLICKER,
#  if __JDOOM64__
    TC_BLINK,
#  endif
# else
    TC_GLOW,
# endif
#endif
    TC_MATERIALCHANGER,
    TC_SCROLL,
    NUMTHINKERCLASSES
} thinkerclass_t;

#ifdef __cplusplus
// Thinker Save flags
#define TSF_SERVERONLY          0x01 ///< Only saved by servers.

typedef void (*WriteThinkerFunc)(thinker_t *, MapStateWriter *msw);
typedef int (*ReadThinkerFunc)(thinker_t *, MapStateReader *msr);

struct ThinkerClassInfo
{
    thinkerclass_t thinkclass;
    thinkfunc_t function;
    int flags;
    WriteThinkerFunc writeFunc;
    ReadThinkerFunc readFunc;
    size_t size;
};

/**
 * Returns the info for the specified thinker @a tClass; otherwise @c 0 if not found.
 */
ThinkerClassInfo *SV_ThinkerInfoForClass(thinkerclass_t tClass);

/**
 * Returns the info for the specified thinker; otherwise @c 0 if not found.
 */
ThinkerClassInfo *SV_ThinkerInfo(const thinker_t &thinker);
#endif

#endif // LIBCOMMON_SAVESTATE_THINKERINFO_H
