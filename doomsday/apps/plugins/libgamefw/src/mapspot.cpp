/** @file mapspot.cpp  Map spot where a Thing will be spawned.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "gamefw/mapspot.h"

using namespace de;

struct FlagTranslation
{
    gfw_mapspot_flags_t gfwFlag;
    int internalFlag;
};

static FlagTranslation const flagTranslationTable[GFW_GAME_ID_COUNT][12] =
{
    // GFW_DOOM
    {
        { GFW_MAPSPOT_NOT_SINGLE  , 0x0010 },
        { GFW_MAPSPOT_NOT_DM      , 0x0020 },
        { GFW_MAPSPOT_NOT_COOP    , 0x0040 },
        { GFW_MAPSPOT_DEAF        , 0x0008 },
        { GFW_MAPSPOT_MBF_FRIEND  , 0x1000 },
        { GFW_MAPSPOT_TRANSLUCENT , 0x2000 },
        { GFW_MAPSPOT_INVISIBLE   , 0x4000 },
        { GFW_MAPSPOT_STANDING    , 0x8000 },
    },

    // GFW_HERETIC
    {
        { GFW_MAPSPOT_NOT_SINGLE  , 0x0010 },
        { GFW_MAPSPOT_NOT_DM      , 0x0020 },
        { GFW_MAPSPOT_NOT_COOP    , 0x0040 },
        { GFW_MAPSPOT_DEAF        , 0x0008 },
        { GFW_MAPSPOT_MBF_FRIEND  , 0x1000 },
        { GFW_MAPSPOT_TRANSLUCENT , 0x2000 },
        { GFW_MAPSPOT_INVISIBLE   , 0x4000 },
        { GFW_MAPSPOT_STANDING    , 0x8000 },
    },

    // GFW_HEXEN
    {
        { GFW_MAPSPOT_NOT_SINGLE  , 0x0100 },
        { GFW_MAPSPOT_NOT_DM      , 0x0400 },
        { GFW_MAPSPOT_NOT_COOP    , 0x0800 },
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
        { GFW_MAPSPOT_NOT_SINGLE  , 0x0010 },
        { GFW_MAPSPOT_NOT_DM      , 0x0400 },
        { GFW_MAPSPOT_NOT_COOP    , 0x0800 },
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
    int internalFlags = 0;
    for (auto const &xlat : flagTranslationTable[gfw_CurrentGame()])
    {
        if (mapSpotFlags & xlat.gfwFlag)
        {
            internalFlags |= xlat.internalFlag;
        }
    }
    return internalFlags;
}

gfw_mapspot_flags_t gfw_MapSpot_TranslateFlagsFromInternal(int internalFlags)
{
    gfw_mapspot_flags_t mapSpotFlags = 0;
    for (auto const &xlat : flagTranslationTable[gfw_CurrentGame()])
    {
        if (internalFlags & xlat.internalFlag)
        {
            mapSpotFlags |= xlat.gfwFlag;
        }
    }
    return mapSpotFlags;

}
