/** @file mapspot.cpp  Map spot where a Thing will be spawned.
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

#include "doomsday/gamefw/mapspot.h"

using namespace de;

struct FlagTranslation
{
    gfw_mapspot_flags_t gfwFlag;
    int internalFlag;
};

// In the internal flags, the single/dm/coop flags have inversed meaning.

static int const defaultInternalFlags[GFW_GAME_ID_COUNT] =
{
    0x070,  // GFW_DOOM
    0x070,  // GFW_HERETIC
    0x700,  // GFW_HEXEN
    0xc10,  // GFW_DOOM64
    0x0,    // GFW_STRIFE
};

static const gfw_mapspot_flags_t defaultMapSpotFlags =
        GFW_MAPSPOT_SINGLE |
        GFW_MAPSPOT_COOP   |
        GFW_MAPSPOT_DM;

static FlagTranslation const flagTranslationTable[GFW_GAME_ID_COUNT][12] =
{
    // GFW_DOOM
    {
        { GFW_MAPSPOT_SINGLE      , 0x0010 },
        { GFW_MAPSPOT_DM          , 0x0020 },
        { GFW_MAPSPOT_COOP        , 0x0040 },
        { GFW_MAPSPOT_DEAF        , 0x0008 },
        { GFW_MAPSPOT_MBF_FRIEND  , 0x1000 },
        { GFW_MAPSPOT_TRANSLUCENT , 0x2000 },
        { GFW_MAPSPOT_INVISIBLE   , 0x4000 },
        { GFW_MAPSPOT_STANDING    , 0x8000 },
    },

    // GFW_HERETIC
    {
        { GFW_MAPSPOT_SINGLE      , 0x0010 },
        { GFW_MAPSPOT_DM          , 0x0020 },
        { GFW_MAPSPOT_COOP        , 0x0040 },
        { GFW_MAPSPOT_DEAF        , 0x0008 },
        { GFW_MAPSPOT_MBF_FRIEND  , 0x1000 },
        { GFW_MAPSPOT_TRANSLUCENT , 0x2000 },
        { GFW_MAPSPOT_INVISIBLE   , 0x4000 },
        { GFW_MAPSPOT_STANDING    , 0x8000 },
    },

    // GFW_HEXEN
    {
        { GFW_MAPSPOT_SINGLE      , 0x0100 },
        { GFW_MAPSPOT_DM          , 0x0400 },
        { GFW_MAPSPOT_COOP        , 0x0800 },
        { GFW_MAPSPOT_DEAF        , 0x0008 },
        { GFW_MAPSPOT_DORMANT     , 0x0010 },
        { GFW_MAPSPOT_CLASS1      , 0x0020 },
        { GFW_MAPSPOT_CLASS2      , 0x0040 },
        { GFW_MAPSPOT_CLASS3      , 0x0080 },
        { GFW_MAPSPOT_MBF_FRIEND  , 0x1000 },
        { GFW_MAPSPOT_TRANSLUCENT , 0x2000 },
        { GFW_MAPSPOT_INVISIBLE   , 0x4000 },
        { GFW_MAPSPOT_STANDING    , 0x8000 },
    },

    // GFW_DOOM64
    {
        { GFW_MAPSPOT_SINGLE      , 0x0010 },
        { GFW_MAPSPOT_DM          , 0x0400 },
        { GFW_MAPSPOT_COOP        , 0x0800 },
        { GFW_MAPSPOT_DEAF        , 0x0008 },
        { GFW_MAPSPOT_MBF_FRIEND  , 0x1000 },
        { GFW_MAPSPOT_TRANSLUCENT , 0x2000 },
        { GFW_MAPSPOT_INVISIBLE   , 0x4000 },
        { GFW_MAPSPOT_STANDING    , 0x8000 },
    },

    // GFW_STRIFE
    {},
};

int gfw_MapSpot_TranslateFlagsToInternal(gfw_mapspot_flags_t mapSpotFlags)
{
    int internalFlags = defaultInternalFlags[gfw_CurrentGame()];
    for (const auto &x : flagTranslationTable[gfw_CurrentGame()])
    {
        if (mapSpotFlags & x.gfwFlag)
        {
            internalFlags ^= x.internalFlag;
        }
    }
    return internalFlags;
}

gfw_mapspot_flags_t gfw_MapSpot_TranslateFlagsFromInternal(int internalFlags)
{
    gfw_mapspot_flags_t mapSpotFlags = defaultMapSpotFlags;
    for (const auto &x : flagTranslationTable[gfw_CurrentGame()])
    {
        if (internalFlags & x.internalFlag)
        {
            mapSpotFlags ^= x.gfwFlag;
        }
    }
    return mapSpotFlags;

}
