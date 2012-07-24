/**
 * @file id1map_util.cpp @ingroup wadmapconverter
 *
 * Miscellaneous map conversion utility routines.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "wadmapconverter.h"
#include "maplumpinfo.h"

const Str* MapFormatNameForId(mapformatid_t id)
{
    static const de::Str names[1 + NUM_MAPFORMATS] = {
        /* MF_UNKNOWN */ "Unknown",
        /* MF_DOOM    */ "Doom",
        /* MF_HEXEN   */ "Hexen",
        /* MF_DOOM64  */ "Doom64"
    };
    if(VALID_MAPFORMATID(id))
    {
        return names[1 + id];
    }
    return names[0];
}

MapLumpType MapLumpTypeForName(const char* name)
{
    static const struct maplumpinfo_s {
        const char* name;
        MapLumpType type;
    } lumptypeForNameDict[] =
    {
        { "THINGS",     ML_THINGS },
        { "LINEDEFS",   ML_LINEDEFS },
        { "SIDEDEFS",   ML_SIDEDEFS },
        { "VERTEXES",   ML_VERTEXES },
        { "SEGS",       ML_SEGS },
        { "SSECTORS",   ML_SSECTORS },
        { "NODES",      ML_NODES },
        { "SECTORS",    ML_SECTORS },
        { "REJECT",     ML_REJECT },
        { "BLOCKMAP",   ML_BLOCKMAP },
        { "BEHAVIOR",   ML_BEHAVIOR },
        { "SCRIPTS",    ML_SCRIPTS },
        { "LIGHTS",     ML_LIGHTS },
        { "MACROS",     ML_MACROS },
        { "LEAFS",      ML_LEAFS },
        { "GL_VERT",    ML_GLVERT },
        { "GL_SEGS",    ML_GLSEGS },
        { "GL_SSECT",   ML_GLSSECT },
        { "GL_NODES",   ML_GLNODES },
        { "GL_PVS",     ML_GLPVS},
        { NULL,         ML_INVALID }
    };

    DENG_ASSERT(name);

    if(name[0])
    for(int i = 0; lumptypeForNameDict[i].name; ++i)
    {
        if(!strnicmp(lumptypeForNameDict[i].name, name, strlen(lumptypeForNameDict[i].name)))
            return lumptypeForNameDict[i].type;
    }

    return ML_INVALID;
}
