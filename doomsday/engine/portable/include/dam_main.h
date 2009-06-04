/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * dam_main.h: Doomsday Archived Map (DAM) reader
 */

#ifndef __DOOMSDAY_ARCHIVED_MAP_MAIN_H__
#define __DOOMSDAY_ARCHIVED_MAP_MAIN_H__

enum {
    ML_INVALID = -1,
    ML_LABEL, // A separator, name, ExMx or MAPxx
    ML_THINGS, // Monsters, items..
    ML_LINEDEFS, // LineDefs, from editing
    ML_SIDEDEFS, // SideDefs, from editing
    ML_VERTEXES, // Vertices, edited and BSP splits generated
    ML_SEGS, // LineSegs, from LineDefs split by BSP
    ML_SSECTORS, // SubSectors, list of LineSegs
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
    char       *formatName;
    int         lumpClass;
} maplumpformat_t;

typedef struct maplumpinfo_s {
    int         lumpNum;
    maplumpformat_t *format;
    int         lumpClass;
    int         startOffset;
    uint        elements;
    size_t      length;
} maplumpinfo_t;

typedef struct listnode_s {
    void       *data;
    struct listnode_s *next;
} listnode_t;

typedef struct archivedmap_s {
    char        identifier[9];
    int         numLumps;
    int        *lumpList;
    filename_t  cachedMapDataFile;
    boolean     cachedMapFound;
    boolean     lastLoadAttemptFailed;
} archivedmap_t;

extern byte     mapCache;

void        DAM_Register(void);

void        DAM_Init(void);
void        DAM_Shutdown(void);

void        DAM_GetCachedMapDir(char* dir, int mainLump, size_t len);

boolean     DAM_AttemptMapLoad(const char* mapID);

#endif
