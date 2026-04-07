/** @file mapspot.h  Map spot where a Thing will be spawned.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBGAMEFW_MAPSPOT_H
#define LIBGAMEFW_MAPSPOT_H

#include "doomsday/gamefw/defs.h"

typedef uint32_t gfw_mapspot_flags_t;

/*
 * Universal mapspot flags.
 * - The least significant byte control the presence of the object.
 * - The second byte controls the appearance of the object.
 * - The third byte controls the behavior of the object.
 */

#define GFW_MAPSPOT_SINGLE      0x00000001
#define GFW_MAPSPOT_DM          0x00000002
#define GFW_MAPSPOT_COOP        0x00000004
#define GFW_MAPSPOT_CLASS1      0x00000008
#define GFW_MAPSPOT_CLASS2      0x00000010
#define GFW_MAPSPOT_CLASS3      0x00000020

#define GFW_MAPSPOT_TRANSLUCENT 0x00000100
#define GFW_MAPSPOT_INVISIBLE   0x00000200

#define GFW_MAPSPOT_DEAF        0x00010000
#define GFW_MAPSPOT_DORMANT     0x00020000
#define GFW_MAPSPOT_STANDING    0x00040000
#define GFW_MAPSPOT_MBF_FRIEND  0x00080000
#define GFW_MAPSPOT_STRIFE_ALLY 0x00100000

#ifdef __cplusplus
extern "C" {
#endif

LIBDOOMSDAY_PUBLIC int gfw_MapSpot_TranslateFlagsToInternal(gfw_mapspot_flags_t mapSpotFlags);

LIBDOOMSDAY_PUBLIC gfw_mapspot_flags_t gfw_MapSpot_TranslateFlagsFromInternal(int internalFlags);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBGAMEFW_MAPSPOT_H
