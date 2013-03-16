/** @file r_lgrid.cpp
 *
 * @authors Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

/**
 * Light Grid (Large-Scale FakeRadio).
 *
 * Very simple global illumination method utilizing a 2D grid of light levels.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <assert.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"

#include "gl/sys_opengl.h"

// MACROS ------------------------------------------------------------------

#define GRID_BLOCK(x, y)        (&grid[(y)*lgBlockWidth + (x)])

#define GBF_CHANGED     0x1     // Grid block sector light has changed.
#define GBF_CONTRIBUTOR 0x2     // Contributes light to a changed block.

BEGIN_PROF_TIMERS()
  PROF_GRID_UPDATE
END_PROF_TIMERS()

// TYPES -------------------------------------------------------------------

typedef struct gridblock_s {
    Sector *sector;

    byte        flags;

    // Positive bias means that the light is shining in the floor of
    // the sector.
    char        bias;

    // Color of the light:
    float       rgb[3];
    float       oldRGB[3];  // Used instead of rgb if the lighting in this
                            // block has changed and we haven't yet done a
                            // a full grid update.
} gridblock_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

int             lgEnabled = false;
static boolean  lgInited;
static boolean  needsUpdate = true;

static int      lgShowDebug = false;
static float    lgDebugSize = 1.5f;

static int      lgBlockSize = 31;
static coord_t  lgOrigin[3];
static int      lgBlockWidth;
static int      lgBlockHeight;
static gridblock_t *grid;

static int      lgMXSample = 1; // Default is mode 1 (5 samples per block)

// CODE --------------------------------------------------------------------

/**
 * Registers console variables.
 */
void LG_Register(void)
{
    C_VAR_INT("rend-bias-grid", &lgEnabled, 0, 0, 1);

    C_VAR_INT("rend-bias-grid-debug", &lgShowDebug, CVF_NO_ARCHIVE, 0, 1);

    C_VAR_FLOAT("rend-bias-grid-debug-size", &lgDebugSize, 0, .1f, 100);

    C_VAR_INT("rend-bias-grid-blocksize", &lgBlockSize, 0, 8, 1024);

    C_VAR_INT("rend-bias-grid-multisample", &lgMXSample, 0, 0, 7);
}

/**
 * Determines if the index (x, y) is in the bitfield.
 */
static boolean HasIndexBit(int x, int y, uint *bitfield)
{
    uint index = x + y * lgBlockWidth;

    // Assume 32-bit uint.
    return (bitfield[index >> 5] & (1 << (index & 0x1f))) != 0;
}

/**
 * Sets the index (x, y) in the bitfield.
 * Count is incremented when a zero bit is changed to one.
 */
static void AddIndexBit(int x, int y, uint *bitfield, int *count)
{
    uint index = x + y * lgBlockWidth;

    // Assume 32-bit uint.
    if(!HasIndexBit(index, 0, bitfield))
    {
        (*count)++;
    }
    bitfield[index >> 5] |= (1 << (index & 0x1f));
}

/**
 * Initialize the light grid for the current map.
 */
void LG_InitForMap(void)
{
    uint        startTime = Timer_RealMilliseconds();

#define MSFACTORS 7
    typedef struct lgsamplepoint_s {
        coord_t origin[3];
    } lgsamplepoint_t;
    // Diagonal in maze arrangement of natural numbers.
    // Up to 65 samples per-block(!)
    static int  multisample[] = {1, 5, 9, 17, 25, 37, 49, 65};

    coord_t     max[3];
    coord_t     width, height;
    int         i = 0;
    int         a, b, x, y;
    int         count;
    int         changedCount;
    size_t      bitfieldSize;
    uint       *indexBitfield = 0;
    uint       *contributorBitfield = 0;
    gridblock_t *block;
    int        *sampleResults = 0;
    int         n, size, numSamples, center, best;
    uint        s;
    coord_t     off[2];
    lgsamplepoint_t *samplePoints = 0, sample;

    Sector **ssamples;
    Sector **blkSampleSectors;
    GameMap *map = theMap;

    if(!lgEnabled || !map)
    {
        lgInited = false;
        return;
    }

    lgInited = true;

    // Allocate the grid.
    GameMap_Bounds(map, &lgOrigin[0], &max[0]);

    width  = max[VX] - lgOrigin[VX];
    height = max[VY] - lgOrigin[VY];

    lgBlockWidth  = ROUND(width / lgBlockSize) + 1;
    lgBlockHeight = ROUND(height / lgBlockSize) + 1;

    // Clamp the multisample factor.
    if(lgMXSample > MSFACTORS)
        lgMXSample = MSFACTORS;
    else if(lgMXSample < 0)
        lgMXSample = 0;

    numSamples = multisample[lgMXSample];

    // Allocate memory for sample points array.
    samplePoints = (lgsamplepoint_t *) M_Malloc(sizeof(lgsamplepoint_t) * numSamples);

    /**
     * It would be possible to only allocate memory for the unique
     * sample results. And then select the appropriate sample in the loop
     * for initializing the grid instead of copying the previous results in
     * the loop for acquiring the sample points.
     *
     * Calculate with the equation (number of unique sample points):
     *
     * ((1 + lgBlockHeight * lgMXSample) * (1 + lgBlockWidth * lgMXSample)) +
     *     (size % 2 == 0? numBlocks : 0)
     * OR
     *
     * We don't actually need to store the ENTIRE sample points array. It
     * would be sufficent to only store the results from the start of the
     * previous row to current col index. This would save a bit of memory.
     *
     * However until lightgrid init is finalized it would be rather silly
     * to optimize this much further.
     */

    // Allocate memory for all the sample results.
    ssamples = (Sector **) M_Malloc(sizeof(Sector*) *
                                    ((lgBlockWidth * lgBlockHeight) * numSamples));

    // Determine the size^2 of the samplePoint array plus its center.
    size = center = 0;
    if(numSamples > 1)
    {
        float       f = sqrt(float(numSamples));

        if(ceil(f) != floor(f))
        {
            size = sqrt(float(numSamples -1));
            center = 0;
        }
        else
        {
            size = (int) f;
            center = size+1;
        }
    }

    // Construct the sample point offset array.
    // This way we can use addition only during calculation of:
    // (lgBlockHeight*lgBlockWidth)*numSamples
    if(center == 0)
    {
        // Zero is the center so do that first.
        samplePoints[0].origin[VX] = lgBlockSize / 2;
        samplePoints[0].origin[VY] = lgBlockSize / 2;
    }

    if(numSamples > 1)
    {
        coord_t bSize = (coord_t) lgBlockSize / (size-1);

        // Is there an offset?
        if(center == 0)
            n = 1;
        else
            n = 0;

        for(y = 0; y < size; ++y)
            for(x = 0; x < size; ++x, ++n)
            {
                samplePoints[n].origin[VX] = ROUND(x * bSize);
                samplePoints[n].origin[VY] = ROUND(y * bSize);
            }
    }

/*
#if _DEBUG
    for(n = 0; n < numSamples; ++n)
        Con_Message(" %i of %i %i(%f %f)",
                    n, numSamples, (n == center)? 1 : 0,
                    samplePoints[n].pos[VX], samplePoints[n].pos[VY]);
#endif
*/

    // Acquire the sectors at ALL the sample points.
    for(y = 0; y < lgBlockHeight; ++y)
    {
        off[VY] = y * lgBlockSize;
        for(x = 0; x < lgBlockWidth; ++x)
        {
            int blk = (x + y * lgBlockWidth);
            int idx;

            off[VX] = x * lgBlockSize;

            n = 0;
            if(center == 0)
            {   // Center point is not considered with the term 'size'.
                // Sample this point and place at index 0 (at the start
                // of the samples for this block).
                idx = blk * (numSamples);

                sample.origin[VX] = lgOrigin[VX] + off[VX] + samplePoints[0].origin[VX];
                sample.origin[VY] = lgOrigin[VY] + off[VY] + samplePoints[0].origin[VY];

                ssamples[idx] = P_BspLeafAtPoint(sample.origin)->sectorPtr();
                if(!P_IsPointInSector(sample.origin, ssamples[idx]))
                   ssamples[idx] = NULL;

                n++; // Offset the index in the samplePoints array bellow.
            }

            count = blk * size;
            for(b = 0; b < size; ++b)
            {
                i = (b + count) * size;
                for(a = 0; a < size; ++a, ++n)
                {
                    idx = a + i;

                    if(center == 0)
                        idx += blk + 1;

                    if(numSamples > 1 && ((x > 0 && a == 0) || (y > 0 && b == 0)))
                    {
                        // We have already sampled this point.
                        // Get the previous result.
                        int prevX, prevY, prevA, prevB;
                        int previdx;

                        prevX = x; prevY = y; prevA = a; prevB = b;
                        if(x > 0 && a == 0)
                        {
                            prevA = size -1;
                            prevX--;
                        }
                        if(y > 0 && b == 0)
                        {
                            prevB = size -1;
                            prevY--;
                        }

                        previdx = prevA + (prevB + (prevX + prevY * lgBlockWidth) * size) * size;

                        if(center == 0)
                            previdx += (prevX + prevY * lgBlockWidth) + 1;

                        ssamples[idx] = ssamples[previdx];
                    }
                    else
                    {
                        // We haven't sampled this point yet.
                        sample.origin[VX] = lgOrigin[VX] + off[VX] + samplePoints[n].origin[VX];
                        sample.origin[VY] = lgOrigin[VY] + off[VY] + samplePoints[n].origin[VY];

                        ssamples[idx] = P_BspLeafAtPoint(sample.origin)->sectorPtr();
                        if(!P_IsPointInSector(sample.origin, ssamples[idx]))
                           ssamples[idx] = NULL;
                    }
                }
            }
        }
    }
    // We're done with the samplePoints block.
    M_Free(samplePoints);

    // Bitfields for marking affected blocks. Make sure each bit is in a word.
    bitfieldSize = 4 * (31 + lgBlockWidth * lgBlockHeight) / 32;
    indexBitfield = (unsigned int *) M_Calloc(bitfieldSize);
    contributorBitfield = (unsigned int *) M_Calloc(bitfieldSize);

    // \todo It would be possible to only allocate memory for the grid
    // blocks that are going to be in use.

    // Allocate memory for the entire grid.
    grid = (gridblock_t* ) Z_Calloc(sizeof(gridblock_t) * lgBlockWidth * lgBlockHeight,
                                    PU_MAPSTATIC, NULL);

    Con_Message("LG_InitForMap: %i x %i grid (%lu bytes).",
                lgBlockWidth, lgBlockHeight,
                (unsigned long) (sizeof(gridblock_t) * lgBlockWidth * lgBlockHeight));

    // Allocate memory used for the collection of the sample results.
    blkSampleSectors = (Sector **) M_Malloc(sizeof(Sector*) * numSamples);
    if(numSamples > 1)
        sampleResults = (int *) M_Calloc(sizeof(int) * numSamples);

    // Initialize the grid.
    for(block = grid, y = 0; y < lgBlockHeight; ++y)
    {
        off[VY] = y * lgBlockSize;
        for(x = 0; x < lgBlockWidth; ++x, ++block)
        {
            off[VX] = x * lgBlockSize;

            /**
             * Pick the sector at each of the sample points.
             * \todo We don't actually need the blkSampleSectors array
             * anymore. Now that ssamples stores the results consecutively
             * a simple index into ssamples would suffice.
             * However if the optimization to save memory is implemented as
             * described in the comments above we WOULD still require it.
             * Therefore, for now I'm making use of it to clarify the code.
             */
            n = (x + y * lgBlockWidth) * numSamples;
            for(i = 0; i < numSamples; ++i)
                blkSampleSectors[i] = ssamples[i + n];

            block->sector = NULL;

            if(numSamples == 1)
            {
                block->sector = blkSampleSectors[center];
            }
            else
            {   // Pick the sector which had the most hits.
                best = -1;
                memset(sampleResults, 0, sizeof(int) * numSamples);
                for(i = 0; i < numSamples; ++i)
                    if(blkSampleSectors[i])
                        for(a = 0; a < numSamples; ++a)
                            if(blkSampleSectors[a] == blkSampleSectors[i] &&
                               blkSampleSectors[a])
                            {
                                sampleResults[a]++;
                                if(sampleResults[a] > best)
                                    best = i;
                            }

                if(best != -1)
                {   // Favour the center sample if its a draw.
                    if(sampleResults[best] == sampleResults[center] &&
                       blkSampleSectors[center])
                        block->sector = blkSampleSectors[center];
                    else
                        block->sector = blkSampleSectors[best];
                }
            }
        }
    }
    // We're done with sector samples completely.
    M_Free(ssamples);
    // We're done with the sample results arrays.
    M_Free(blkSampleSectors);
    if(numSamples > 1)
        M_Free(sampleResults);

    // Find the blocks of all sectors.
    for(s = 0; s < NUM_SECTORS; ++s)
    {
        Sector *sector = SECTOR_PTR(s);

        count = changedCount = 0;

        if(sector->lineCount())
        {
            // Clear the bitfields.
            std::memset(indexBitfield, 0, bitfieldSize);
            std::memset(contributorBitfield, 0, bitfieldSize);

            for(block = grid, y = 0; y < lgBlockHeight; ++y)
            {
                for(x = 0; x < lgBlockWidth; ++x, ++block)
                {
                    if(block->sector == sector)
                    {
                        // \todo Determine min/max a/b before going into the loop.
                        for(b = -2; b <= 2; ++b)
                        {
                            if(y + b < 0 || y + b >= lgBlockHeight)
                                continue;

                            for(a = -2; a <= 2; ++a)
                            {
                                if(x + a < 0 || x + a >= lgBlockWidth)
                                    continue;

                                AddIndexBit(x + a, y + b, indexBitfield,
                                            &changedCount);
                            }
                        }
                    }
                }
            }

            // Determine contributor blocks. Contributors are the blocks that are
            // close enough to contribute light to affected blocks.
            for(y = 0; y < lgBlockHeight; ++y)
            {
                for(x = 0; x < lgBlockWidth; ++x)
                {
                    if(!HasIndexBit(x, y, indexBitfield))
                        continue;

                    // Add the contributor blocks.
                    for(b = -2; b <= 2; ++b)
                    {
                        if(y + b < 0 || y + b >= lgBlockHeight)
                            continue;

                        for(a = -2; a <= 2; ++a)
                        {
                            if(x + a < 0 || x + a >= lgBlockWidth)
                                continue;

                            if(!HasIndexBit(x + a, y + b, indexBitfield))
                            {
                                AddIndexBit(x + a, y + b, contributorBitfield,
                                            &count);
                            }
                        }
                    }
                }
            }
        }

/*if _DEBUG
Con_Message("  Sector %i: %i / %i", s, changedCount, count);
#endif*/

        Sector::LightGridData &lgData = sector->_lightGridData;
        lgData.changedBlockCount = changedCount;
        lgData.blockCount = changedCount + count;

        if(lgData.blockCount > 0)
        {
            lgData.blocks = (ushort *) Z_Malloc(sizeof(*lgData.blocks) * lgData.blockCount, PU_MAPSTATIC, 0);

            for(x = 0, a = 0, b = changedCount; x < lgBlockWidth * lgBlockHeight;
                ++x)
            {
                if(HasIndexBit(x, 0, indexBitfield))
                    lgData.blocks[a++] = x;
                else if(HasIndexBit(x, 0, contributorBitfield))
                    lgData.blocks[b++] = x;
            }

            DENG_ASSERT(a == changedCount);
            //DENG_ASSERT(b == info->blockCount);
        }
    }

    M_Free(indexBitfield);
    M_Free(contributorBitfield);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("LG_InitForMap: Done in %.2f seconds.",
             (Timer_RealMilliseconds() - startTime) / 1000.0f));
}

/**
 * Apply the sector's lighting to the block.
 */
static void LG_ApplySector(gridblock_t *block, const float *color, float level,
                           float factor, int bias)
{
    int                 i;

    // Apply a bias to the light level.
    level -= (0.95f - level);
    if(level < 0)
        level = 0;

    level *= factor;

    if(level <= 0)
        return;

    for(i = 0; i < 3; ++i)
    {
        float           c = color[i] * level;

        c = MINMAX_OF(0, c, 1);

        if(block->rgb[i] + c > 1)
        {
            block->rgb[i] = 1;
        }
        else
        {
            block->rgb[i] += c;
        }
    }

    // Influenced by the source bias.
    i = block->bias * (1 - factor) + bias * factor;
    i = MINMAX_OF(-0x80, i, 0x7f);
    block->bias = i;
}

/**
 * Called when a sector has changed its light level.
 */
void LG_SectorChanged(Sector *sector)
{
    if(!lgInited) return;
    if(!sector) return;

    Sector::LightGridData &lgData = sector->_lightGridData;
    if(!lgData.changedBlockCount && !lgData.blockCount) return;

    // Mark changed blocks and contributors.
    for(uint i = 0; i < lgData.changedBlockCount; ++i)
    {
        ushort n = lgData.blocks[i];

        // The color will be recalculated.
        if(!(grid[n].flags & GBF_CHANGED))
        {
            std::memcpy(grid[n].oldRGB, grid[n].rgb, sizeof(grid[n].oldRGB));
        }

        for(int j = 0; j < 3; ++j)
        {
            grid[n].rgb[j] = 0;
        }

        grid[n].flags |= GBF_CHANGED | GBF_CONTRIBUTOR;
    }

    for(uint i = 0; i < lgData.blockCount; ++i)
    {
        grid[lgData.blocks[i]].flags |= GBF_CONTRIBUTOR;
    }

    needsUpdate = true;
}

void LG_MarkAllForUpdate(void)
{
    if(!lgInited || !theMap)
        return;

    // Mark all blocks and contributors.
    { uint i;
    for(i = 0; i < NUM_SECTORS; ++i)
    {
        LG_SectorChanged(GameMap_Sector(theMap, i));
    }}
}

#if 0
/*
 * Determines whether it is necessary to recalculate the lighting of a
 * grid block.  Updates are needed when there has been a light level
 * or color change in a sector that affects the block.
 */
static boolean LG_BlockNeedsUpdate(int x, int y)
{
    // First check the block itself.
    gridblock_t *block = GRID_BLOCK(x, y);
    Sector *blockSector;
    int     a, b;
    int     limitA[2];
    int     limitB;

    blockSector = block->sector;
    if(!blockSector)
    {
        // The sector needs to be determined.
        return true;
    }

    if(SECT_INFO(blockSector)->flags & SIF_LIGHT_CHANGED)
    {
        return true;
    }

    // Check neighbor blocks as well.
    // Determine neighbor boundaries.  Y coordinate.
    if(y >= 2)
    {
        b = y - 2;
    }
    else
    {
        b = 0;
    }
    if(y <= lgBlockHeight - 3)
    {
        limitB = y + 2;
    }
    else
    {
        limitB = lgBlockHeight - 1;
    }

    // X coordinate.
    if(x >= 2)
    {
        limitA[0] = x - 2;
    }
    else
    {
        limitA[0] = 0;
    }
    if(x <= lgBlockWidth - 3)
    {
        limitA[1] = x + 2;
    }
    else
    {
        limitA[1] = lgBlockWidth - 1;
    }

    // Iterate through neighbors.
    for(; b <= limitB; ++b)
    {
        a = limitA[0];
        block = GRID_BLOCK(a, b);

        for(; a <= limitA[1]; ++a, ++block)
        {
            if(!a && !b) continue;

            if(block->sector == blockSector)
                continue;

            if(SECT_INFO(block->sector)->flags & SIF_LIGHT_CHANGED)
            {
                return true;
            }
        }
    }

    return false;
}
#endif

/**
 * Update the grid by finding the strongest light source in each grid
 * block.
 */
void LG_Update(void)
{
    static const float  factors[5 * 5] =
    {
        .1f, .2f, .25f, .2f, .1f,
        .2f, .4f, .5f, .4f, .2f,
        .25f, .5f, 1.f, .5f, .25f,
        .2f, .4f, .5f, .4f, .2f,
        .1f, .2f, .25f, .2f, .1f
    };

    gridblock_t        *block, *lastBlock, *other;
    int                 x, y, a, b;
    Sector             *sector;
    const float        *color;
    int                 bias;
    int                 height;

#ifdef DD_PROFILE
    static int          i;

    if(++i > 40)
    {
        i = 0;
        PRINT_PROF( PROF_GRID_UPDATE );
    }
#endif

    if(!lgInited || !needsUpdate)
        return;

BEGIN_PROF( PROF_GRID_UPDATE );

#if 0
    for(block = grid, y = 0; y < lgBlockHeight; ++y)
    {
        for(x = 0; x < lgBlockWidth; ++x, block++)
        {
            if(LG_BlockNeedsUpdate(x, y))
            {
                block->flags |= GBF_CHANGED;

                // Clear to zero (will be recalculated).
                memset(block->rgb, 0, sizeof(float) * 3);

                // Mark contributors.
                for(b = -2; b <= 2; ++b)
                {
                    if(y + b < 0 || y + b >= lgBlockHeight)
                        continue;

                    for(a = -2; a <= 2; ++a)
                    {
                        if(x + a < 0 || x + a >= lgBlockWidth)
                            continue;

                        GRID_BLOCK(x + a, y + b)->flags |= GBF_CONTRIBUTOR;
                    }
                }
            }
            else
            {
                block->flags &= ~GBF_CHANGED;
            }
        }
    }
#endif

    for(block = grid, y = 0; y < lgBlockHeight; ++y)
    {
        for(x = 0; x < lgBlockWidth; ++x, ++block)
        {
            // Unused blocks can't contribute.
            if(!(block->flags & GBF_CONTRIBUTOR) || !block->sector)
                continue;

            // Determine the color of the ambient light in this sector.
            sector = block->sector;
            color = R_GetSectorLightColor(sector);
            height = (int) (sector->ceiling().height() - sector->floor().height());

            bool isSkyFloor = sector->ceilingSurface().hasSkyMaskedMaterial();
            bool isSkyCeil  = sector->floorSurface().hasSkyMaskedMaterial();

            if(isSkyFloor && !isSkyCeil)
            {
                bias = -height / 6;
            }
            else if(!isSkyFloor && isSkyCeil)
            {
                bias = height / 6;
            }
            else if(height > 100)
            {
                bias = (height - 100) / 2;
            }
            else
            {
                bias = 0;
            }

            // \todo Calculate min/max for a and b.
            for(a = -2; a <= 2; ++a)
            {
                for(b = -2; b <= 2; ++b)
                {
                    if(x + a < 0 || y + b < 0 || x + a > lgBlockWidth - 1 ||
                       y + b > lgBlockHeight - 1) continue;

                    other = GRID_BLOCK(x + a, y + b);

                    if(other->flags & GBF_CHANGED)
                    {
                        LG_ApplySector(other, color, sector->lightLevel(),
                                       factors[(b + 2)*5 + a + 2]/8, bias);
                    }
                }
            }
        }
    }

    // Clear all changed and contribution flags.
    lastBlock = &grid[lgBlockWidth * lgBlockHeight];
    for(block = grid; block != lastBlock; ++block)
    {
        block->flags = 0;
    }

    needsUpdate = false;

END_PROF( PROF_GRID_UPDATE );
}

void LG_Evaluate(coord_t const point[3], float color[3])
{
    int x, y, i;
    //float dz = 0, dimming;
    gridblock_t* block;

    if(!lgInited)
    {
        memset(color, 0, sizeof(float) * 3);
        return;
    }

    x = ROUND((point[VX] - lgOrigin[VX]) / lgBlockSize);
    y = ROUND((point[VY] - lgOrigin[VY]) / lgBlockSize);
    x = MINMAX_OF(1, x, lgBlockWidth - 2);
    y = MINMAX_OF(1, y, lgBlockHeight - 2);

    block = &grid[y * lgBlockWidth + x];

    /**
     * danij: Biased light dimming disabled because this does not work
     * well enough. The problem is that two points on a given surface
     * may be determined to be in different blocks and as the height is
     * taken from the block-linked sector this results in very uneven
     * lighting.
     *
     * Biasing the dimming is a good idea but the heights must be taken
     * from the BSP Leaf which contains the surface and not the block.
     */
    if(block->sector)
    {
        /*if(block->bias < 0)
        {
            // Calculate Z difference to the ceiling.
            dz = block->sector->ceiling().height() - point[VZ];
        }
        else if(block->bias > 0)
        {
            // Calculate Z difference to the floor.
            dz = point[VZ] - block->sector->floor().height();
        }

        dz -= 50;
        if(dz < 0)
            dz = 0;*/

        if(block->flags & GBF_CHANGED)
        {   // We are waiting for an updated value, for now use the old.
            color[CR] = block->oldRGB[CR];
            color[CG] = block->oldRGB[CG];
            color[CB] = block->oldRGB[CB];
        }
        else
        {
            color[CR] = block->rgb[CR];
            color[CG] = block->rgb[CG];
            color[CB] = block->rgb[CB];
        }
    }
    else
    {   // The block has no sector!?
        // Must be an error in the lightgrid covering sector determination.
        // Init to black.
        color[CR] = color[CG] = color[CB] = 0;
    }

    // Biased ambient light causes a dimming in the Z direction.
    /*if(dz && block->bias)
    {
        if(block->bias < 0)
            dimming = 1 - (dz * (float) -block->bias) / 35000.0f;
        else
            dimming = 1 - (dz * (float) block->bias) / 35000.0f;

        if(dimming < .5f)
            dimming = .5f;

        for(i = 0; i < 3; ++i)
        {
            // Apply the dimming
            color[i] *= dimming;

            // Add the light range compression factor
            color[i] += Rend_LightAdaptationDelta(color[i]);
        }
    }
    else*/
    {
        // Just add the light range compression factor
        for(i = 0; i < 3; ++i)
            color[i] += Rend_LightAdaptationDelta(color[i]);
    }
}

float LG_EvaluateLightLevel(coord_t const point[3])
{
    float color[3];
    LG_Evaluate(point, color);
    /// @todo Do not do this at evaluation time; store into another grid.
    return (color[CR] + color[CG] + color[CB]) / 3;
}

/**
 * Draw the grid in 2D HUD mode.
 */
void LG_Debug(void)
{
    static int          blink = 0;

    gridblock_t*        block;
    int                 x, y;
    int                 vx, vy;
    size_t              vIdx = 0, blockIdx;
    ddplayer_t*         ddpl = (viewPlayer? &viewPlayer->shared : NULL);

    if(!lgInited || !lgShowDebug)
        return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(ddpl)
    {
        blink++;
        vx = ROUND((ddpl->mo->origin[VX] - lgOrigin[VX]) / lgBlockSize);
        vy = ROUND((ddpl->mo->origin[VY] - lgOrigin[VY]) / lgBlockSize);
        vx = MINMAX_OF(1, vx, lgBlockWidth - 2);
        vy = MINMAX_OF(1, vy, lgBlockHeight - 2);
        vIdx = vy * lgBlockWidth + vx;
    }

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Window_Width(theWindow), Window_Height(theWindow), 0, -1, 1);

    for(y = 0; y < lgBlockHeight; ++y)
    {
        glBegin(GL_QUADS);
        for(x = 0; x < lgBlockWidth; ++x, ++block)
        {
            blockIdx = (lgBlockHeight - 1 - y) * lgBlockWidth + x;
            block = &grid[blockIdx];

            if(ddpl && vIdx == blockIdx && (blink & 16))
            {
                glColor3f(1, 0, 0);
            }
            else
            {
                if(!block->sector)
                    continue;

                glColor3fv(block->rgb);
            }

            glVertex2f(x * lgDebugSize, y * lgDebugSize);
            glVertex2f(x * lgDebugSize + lgDebugSize, y * lgDebugSize);
            glVertex2f(x * lgDebugSize + lgDebugSize,
                       y * lgDebugSize + lgDebugSize);
            glVertex2f(x * lgDebugSize, y * lgDebugSize + lgDebugSize);
        }
        glEnd();
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
