/** @file mapconverter_loadblockmap.cpp  Blockmap data converter for id Tech 1 format maps.
 *
 * @ingroup idtech1converter
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "importidtech1.h"
#include <de/log.h>

using namespace de;

/**
 * Attempt to load the BLOCKMAP data resource.
 *
 * If the map is too large (would overflow the size limit of the BLOCKMAP lump
 * in an id tech 1 format map; therefore truncated), it's of zero-length or we
 * are forcing a rebuild - we'll have to generate the blockmap data ourselves.
 */
#if 0 // Needs updating.
bool LoadBlockmap(MapLumpInfo* lumpInfo)
{
#define MAPBLOCKUNITS       128

    bool generateBMap = (createBMap == 2);

    LOG_AS("LoadBlockmap");
    LOGDEV_MAP_VERBOSE("Processing BLOCKMAP...");

    // Do we have a lump to process?
    if(lumpInfo->lump == -1 || lumpInfo->length == 0)
        generateBMap = true; // We'll HAVE to generate it.

    // Are we generating new blockmap data?
    if(generateBMap)
    {
        // Only announce if the user has choosen to always generate
        // new data (we will have already announced it if the lump
        // was missing).
        if(lumpInfo->lump != -1)
        {
            LOG_MAP_MSG("Generating new blockmap data...");
        }
        return true;
    }

    // No, the existing data is valid - so load it in.
    uint startTime;
    blockmap_t* blockmap;
    uint x, y, width, height;
    float v[2];
    vec2f_t bounds[2];
    long* lineListOffsets, i, n, numBlocks, blockIdx;
    short* blockmapLump;

    LOG_MAP_VERBOSE("Converting blockmap...");

    startTime = Timer_RealMilliseconds();

    blockmapLump = (short*) W_CacheLump(lumpInfo->lump, PU_GAMESTATIC);

    v[VX] = (float) DD_SHORT(blockmapLump[0]);
    v[VY] = (float) DD_SHORT(blockmapLump[1]);
    width  = ((DD_SHORT(blockmapLump[2])) & 0xffff);
    height = ((DD_SHORT(blockmapLump[3])) & 0xffff);

    numBlocks = (long) width * (long) height;

    /**
     * Expand WAD blockmap into a larger one, by treating all
     * offsets except -1 as unsigned and zero-extending them.
     * This potentially doubles the size of blockmaps allowed
     * because DOOM originally considered the offsets as always
     * signed.
     */

    lineListOffsets = M_Malloc(sizeof(long) * numBlocks);
    n = 4;
    for(i = 0; i < numBlocks; ++i)
    {
        short t = DD_SHORT(blockmapLump[n++]);
        lineListOffsets[i] = (t == -1? -1 : (long) t & 0xffff);
    }

    /**
     * Finally, convert the blockmap into our internal representation.
     * We'll ensure the blockmap is formed correctly as we go.
     *
     * @todo We could gracefully handle malformed blockmaps by by cleaning
     *       up and then generating our own.
     */

    V2f_Set(bounds[0], v[VX], v[VY]);
    v[VX] += (float) (width * MAPBLOCKUNITS);
    v[VY] += (float) (height * MAPBLOCKUNITS);
    V2f_Set(bounds[1], v[VX], v[VY]);

    blockmap = P_BlockmapCreate(bounds[0], bounds[1], width, height);

    blockIdx = 0;
    for(y = 0; y < height; ++y)
        for(x = 0; x < width; ++x)
        {
            long offset = lineListOffsets[blockIdx];
            long idx;
            uint count;

#if defined (DE_DEBUG)
            if(DD_SHORT(blockmapLump[offset]) != 0)
            {
                throw de::Error(
                                "IdTech1Converter::loadBlockmap",
                                stringf("Offset (%ld) for block %ld [%u, %u] does not index the "
                                        "beginning of a line list!",
                                        offset,
                                        blockIdx,
                                        x,
                                        y));
            }
#endif

            // Count the number of lines in this block.
            count = 0;
            while((idx = DD_SHORT(blockmapLump[offset + 1 + count])) != -1)
            {
                count++;
            }

            if(count > 0)
            {
                linedef_t** lines, **ptr;

                // A NULL-terminated array of pointers to lines.
                lines = Z_Malloc((count + 1) * sizeof(linedef_t*), PU_MAPSTATIC, NULL);

                // Copy pointers to the array, delete the nodes.
                ptr = lines;
                count = 0;
                while((idx = DD_SHORT(blockmapLump[offset + 1 + count])) != -1)
                {
#if defined (DE_DEBUG)
                    if(idx < 0 || idx >= (long) map->numLines)
                    {
                        throw de::Error("IdTech1Converter::loadBlockmap",
                                        stringf("Invalid linedef index #%ld.", idx));
                    }
#endif
                    *ptr++ = &map->lines[idx];
                    count++;
                }
                // Terminate.
                *ptr = NULL;

                // Link it into the BlockMap.
                P_BlockmapSetBlock(blockmap, x, y, lines, NULL);
            }

            blockIdx++;
        }

    // Don't need this anymore.
    M_Free(lineListOffsets);

    map->blockMap = blockmap;

    // How much time did we spend?
    LOGDEV_MAP_VERBOSE("Completed in %.2f seconds") << ((Timer_RealMilliseconds() - startTime) / 1000.0f);

    return true;

#undef MAPBLOCKUNITS
}
#endif
