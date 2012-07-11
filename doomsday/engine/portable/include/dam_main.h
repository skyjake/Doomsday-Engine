/**\file dam_main.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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
 * Doomsday Archived Map (DAM) reader
 */

#ifndef LIBDENG_ARCHIVED_MAP_MAIN_H
#define LIBDENG_ARCHIVED_MAP_MAIN_H

enum {
    ML_INVALID = -1,
    ML_LABEL, // A separator, name, ExMx or MAPxx
    ML_THINGS, // Monsters, items..
    ML_LINEDEFS, // LineDefs, from editing
    ML_SIDEDEFS, // SideDefs, from editing
    ML_VERTEXES, // Vertices, edited and BSP splits generated
    ML_SEGS, // LineSegs, from LineDefs split by BSP
    ML_SSECTORS, // Subsectors (BSP leafs), list of LineSegs
    ML_NODES, // BSP nodes
    ML_SECTORS, // Sectors, from editing
    ML_REJECT, // LUT, sector-sector visibility
    ML_BLOCKMAP, // LUT, motion clipping, walls/grid element
    ML_BEHAVIOR, // ACS Scripts (compiled).
    ML_SCRIPTS, // ACS Scripts (source).
    ML_LIGHTS, // Surface color tints.
    ML_MACROS, // DOOM64 format, macro scripts.
    ML_LEAFS, // DOOM64 format, segs (close subsectors).
    NUM_MAPLUMPS
};

typedef struct maplumpformat_s {
    int         hversion;
    char*       formatName;
    int         lumpClass;
} maplumpformat_t;

typedef struct maplumpinfo_s {
    lumpnum_t   lumpNum;
    maplumpformat_t* format;
    int         lumpClass;
    size_t      startOffset;
    uint        elements;
    size_t      length;
} maplumpinfo_t;

typedef struct archivedmap_s {
    Uri* uri;
    int numLumps;
    lumpnum_t* lumpList;
    ddstring_t cachedMapPath;
    boolean cachedMapFound;
    boolean lastLoadAttemptFailed;
} archivedmap_t;

extern byte mapCache;

/// To be called during init to register the cvars and ccmds for this module.
void DAM_Register(void);

/// Initialize this module.
void DAM_Init(void);

/// Shutdown this module.
void DAM_Shutdown(void);

/**
 * Compose the relative path (relative to the runtime directory) to the directory
 * within the archived map cache where maps from the specified source will reside.
 *
 * @param sourcePath  Path to the primary resource file for the original map data.
 * @return  The composed path. Must be destroyed with Str_Delete().
 */
ddstring_t* DAM_ComposeCacheDir(const char* sourcePath);

boolean DAM_AttemptMapLoad(const Uri* uri);

#endif /* LIBDENG_ARCHIVED_MAP_MAIN_H */
