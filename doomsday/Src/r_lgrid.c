/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * r_lgrid.c: Light Grid (Large-Scale FakeRadio)
 *
 * Very simple global illumination method utilizing a 2D grid of light
 * levels.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"

#include <math.h>
#include <assert.h>

// MACROS ------------------------------------------------------------------

#define GRID_BLOCK(x, y)        (&grid[(y)*lgBlockWidth + (x)])

#define GBF_CHANGED     0x1     // Grid block sector light has changed.
#define GBF_CONTRIBUTOR 0x2     // Contributes light to a changed block.

// TYPES -------------------------------------------------------------------

//typedef struct ambientlight_s {
//} ambientlight_t;

typedef struct gridblock_s {
    //struct sector_s *sector;    /* Main source of the light. */
    //ambientlight_t top;
    //ambientlight_t bottom;

    struct sector_s *sector;

    byte flags;

    // Positive bias means that the light is shining in the floor of
    // the sector.
    char bias;

    // Color of the light.
    byte rgb[3];

} gridblock_t;


// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

int             lgEnabled = true;
static boolean  lgInited;
static boolean  needsUpdate = true;

static int      lgShowDebug = false;
static float    lgDebugSize = 1.5f;

static int      blockSize = 31;
static vertex_t lgOrigin;
static int      lgBlockWidth;
static int      lgBlockHeight;
static gridblock_t *grid;

static int      lgMXSample = 1; // Default is mode 1 (5 samples per block)

// CODE --------------------------------------------------------------------

/*
 * Registers console variables.
 */
void LG_Register(void)
{
    C_VAR_INT("rend-bias-grid", &lgEnabled, 0, 0, 1,
              "1=Smooth sector lighting is enabled.");

    C_VAR_INT("rend-bias-grid-debug", &lgShowDebug, 0, 0, 1,
              "1=Show the light grid (for debugging).");

    C_VAR_FLOAT("rend-bias-grid-debug-size", &lgDebugSize, 0, .1f, 100,
                "Size of a grid block in the light grid debug display.");

    C_VAR_INT("rend-bias-grid-blocksize", &blockSize, 0, 8, 1024,
              "Size of a grid block in the light grid (default: 31).");

    C_VAR_INT("rend-bias-grid-multisample", &lgMXSample, 0, 0, 7,
              "Sector to grid block, conversion accuracy multiplier.");
}

/*
 * Determines if the index (x, y) is in the bitfield.
 */
static boolean HasIndexBit(int x, int y, uint *bitfield)
{
    uint index = x + y*lgBlockWidth;

    // Assume 32-bit uint.
    return (bitfield[index >> 5] & (1 << (index & 0x1f))) != 0;
}

/*
 * Sets the index (x, y) in the bitfield.
 * Count is incremented when a zero bit is changed to one.
 */
static void AddIndexBit(int x, int y, uint *bitfield, int *count)
{
    uint index = x + y*lgBlockWidth;

    // Assume 32-bit uint.
    if(!HasIndexBit(index, 0, bitfield))
    {
        (*count)++;
    }
    bitfield[index >> 5] |= (1 << (index & 0x1f));
}

/*
 * Initialize the light grid for the current level.
 */
void LG_Init(void)
{
#define MSFACTORS 7
    // Diagonal in maze arrangement of natural numbers.
    // Up to 65 samples per-block(!)
    static int multisample[] = {1, 5, 9, 17, 25, 37, 49, 65};

    vertex_t max;
    fixed_t width;
    fixed_t height;
    int     i;
    int     a, b, x, y;
    int     count;
    int     changedCount;
    size_t  bitfieldSize;
    uint   *indexBitfield;
    uint   *contributorBitfield;
    gridblock_t *block;
    sector_t **sampleSectors;
    int      *sampleResults;
    int       n, size, numSamples, center, best;
    fixed_t   xOff, yOff;
    vertex_t *samplePoints, sample;

    if(!lgEnabled)
    {
        lgInited = false;
        return;
    }

    lgInited = true;

    // Allocate the grid.
    R_GetMapSize(&lgOrigin, &max);

    width = max.x - lgOrigin.x;
    height = max.y - lgOrigin.y;

    lgBlockWidth = ((width / blockSize) >> FRACBITS) + 1;
    lgBlockHeight = ((height / blockSize) >> FRACBITS) + 1;

    // Bitfields for marking affected blocks. Make sure each bit is in a word.
    bitfieldSize = 4 * (31 + lgBlockWidth * lgBlockHeight) / 32;
    indexBitfield = M_Calloc(bitfieldSize);
    contributorBitfield = M_Calloc(bitfieldSize);

    // TODO: It would be possible to only allocate memory for the grid
    // blocks that are going to be in use.

    // Allocate memory for the entire grid.
    grid = Z_Calloc(sizeof(gridblock_t) * lgBlockWidth * lgBlockHeight,
                    PU_LEVEL, NULL);

    Con_Message("LG_Init: %i x %i grid (%i bytes).\n",
                lgBlockWidth, lgBlockHeight,
                sizeof(gridblock_t) * lgBlockWidth * lgBlockHeight);

    // Clamp the multisample factor.
    if(lgMXSample > MSFACTORS)
        lgMXSample = MSFACTORS;
    else if(lgMXSample < 0)
        lgMXSample = 0;

    numSamples = multisample[lgMXSample];

    // Allocate memory for sector to block sampling.
    sampleSectors = M_Malloc(sizeof(sector_t*) * numSamples);
    samplePoints = M_Malloc(sizeof(vertex_t) * numSamples);
    if(numSamples > 1)
        sampleResults = M_Calloc(sizeof(int) * numSamples);

    // Determine the size^2 of the samplePoint array plus its center.
    size = center = 0;
    if(numSamples > 1)
    {
        float f = sqrt(numSamples);

        if(ceil(f) != floor(f))
        {
            size = sqrt(numSamples -1);
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
    {   // Zero is the center so do that first.
        samplePoints[0].x = (blockSize << (FRACBITS - 1));
        samplePoints[0].y = (blockSize << (FRACBITS - 1));
    }
    if(numSamples > 1)
    {
#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))

        float bSize = (float) blockSize / (size-1);

        // Is there an offset?
        if(center == 0)
            n = 1;
        else
            n = 0;

        for(y = 0; y < size; ++y)
            for(x = 0; x < size; ++x, ++n)
            {
                samplePoints[n].x = FLT2FIX(round(x * bSize));
                samplePoints[n].y = FLT2FIX(round(y * bSize));
            }
    }

/*  // For DEBUG
    for(n = 0; n < numSamples; ++n)
        Con_Message(" %i of %i %i(%f %f)\n",
                    n, numSamples, (n == center)? 1 : 0,
                    FIX2FLT(samplePoints[n].x), FIX2FLT(samplePoints[n].y));
*/

    // Initialize the grid.
    for(block = grid, y = 0; y < lgBlockHeight; ++y)
    {
        yOff = y * (blockSize << FRACBITS);
        for(x = 0; x < lgBlockWidth; ++x, ++block)
        {
            xOff = x * (blockSize << FRACBITS);

            // Find the sector at each of the sample points.
            for(i = 0; i < numSamples; ++i)
            {
                sample.x = lgOrigin.x + xOff + samplePoints[i].x;
                sample.y = lgOrigin.y + yOff + samplePoints[i].y;

                sampleSectors[i] =
                    R_PointInSubsector(sample.x, sample.y)->sector;
                if(!R_IsPointInSector2(sample.x, sample.y, sampleSectors[i]))
                   sampleSectors[i] = NULL;
            }

            block->sector = NULL;

            if(numSamples == 1)
            {
                block->sector = sampleSectors[center];
            }
            else
            {   // Pick the sector which had the most hits.
                best = -1;
                memset(sampleResults, 0, sizeof(int) * numSamples);
                for(i = 0; i < numSamples; ++i)
                    if(sampleSectors[i])
                        for(a = 0; a < numSamples; ++a)
                            if(sampleSectors[a] == sampleSectors[i] &&
                               sampleSectors[a])
                            {
                                sampleResults[a]++;
                                if(sampleResults[a] > best)
                                    best = i;
                            }

                if(best != -1)
                {   // Favour the center sample if its a draw.
                    if(sampleResults[best] == sampleResults[center] &&
                       sampleSectors[center])
                        block->sector = sampleSectors[center];
                    else
                        block->sector = sampleSectors[best];
                }
            }
        }
    }
    M_Free(samplePoints);
    M_Free(sampleSectors);
    if(numSamples > 1)
        M_Free(sampleResults);

    // Find the blocks of all sectors.
    for(i = 0; i < numsectors; ++i)
    {
        sector_t *sector = SECTOR_PTR(i);
        sectorinfo_t *info = SECT_INFO(sector);

        // Clear the bitfields.
        memset(indexBitfield, 0, bitfieldSize);
        memset(contributorBitfield, 0, bitfieldSize);
        count = changedCount = 0;

        for(block = grid, y = 0; y < lgBlockHeight; ++y)
        {
            for(x = 0; x < lgBlockWidth; ++x, ++block)
            {
                if(block->sector == sector)
                {
                    // TODO: Determine min/max a/b before going into the loop.
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

        VERBOSE2(Con_Message("  Sector %i: %i / %i\n", i, changedCount, count));

        info->changedblockcount = changedCount;
        info->blockcount = changedCount + count;

        info->blocks = Z_Malloc(sizeof(unsigned short) * info->blockcount,
                                PU_LEVEL, 0);
        for(x = 0, a = 0, b = changedCount; x < lgBlockWidth * lgBlockHeight;
            ++x)
        {
            if(HasIndexBit(x, 0, indexBitfield))
                info->blocks[a++] = x;
            else if(HasIndexBit(x, 0, contributorBitfield))
                info->blocks[b++] = x;
        }

        assert(a == changedCount);
        assert(b == info->blockcount);
    }

    M_Free(indexBitfield);
    M_Free(contributorBitfield);
}

/*
 * Apply the sector's lighting to the block.
 */
static void LG_ApplySector(gridblock_t *block, const byte *color, int level,
                           float factor, int bias)
{
    int i;

    // Apply a bias to the light level.
    level -= (240 - level);
    if(level < 0)
        level = 0;

    level *= factor;

    if(level <= 0)
        return;

    for(i = 0; i < 3; ++i)
    {
        int c = color[i] * level / 255;
        c = MINMAX_OF(0, c, 255);

        if(block->rgb[i] + c > 255)
        {
            block->rgb[i] = 255;
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

/*
 * Called when a sector has changed its light level.
 */
void LG_SectorChanged(sector_t *sector, sectorinfo_t *info)
{
    int     i, n;

    if(!lgInited)
        return;

    // Mark changed blocks and contributors.
    for(i = 0; i < info->changedblockcount; ++i)
    {
        n = info->blocks[i];
        grid[n].flags |= GBF_CHANGED | GBF_CONTRIBUTOR;
        // The color will be recalculated.
        memset(grid[n].rgb, 0, 3);
    }
    for(; i < info->blockcount; ++i)
    {
        grid[info->blocks[i]].flags |= GBF_CONTRIBUTOR;
    }

    needsUpdate = true;
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
    sector_t *blockSector;
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

/*
 * Update the grid by finding the strongest light source in each grid
 * block.
 */
void LG_Update(void)
{
    gridblock_t *block, *lastBlock, *other;
    int x, y, a, b;
    sector_t *sector;
    const byte *color;
    int bias;
    int height;

    static float factors[5 * 5] =
    {
        .1f, .2f, .25f, .2f, .1f,
        .2f, .4f, .5f, .4f, .2f,
        .25f, .5f, 1.f, .5f, .25f,
        .2f, .4f, .5f, .4f, .2f,
        .1f, .2f, .25f, .2f, .1f
    };

    if(!lgInited || !needsUpdate)
        return;

#if 0
    for(block = grid, y = 0; y < lgBlockHeight; ++y)
    {
        for(x = 0; x < lgBlockWidth; ++x, block++)
        {
            if(LG_BlockNeedsUpdate(x, y))
            {
                block->flags |= GBF_CHANGED;

                // Clear to zero (will be recalculated).
                memset(block->rgb, 0, 3);

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
            height = (sector->SP_ceilheight - sector->SP_floorheight) >> FRACBITS;

            if(sector->SP_ceilpic == skyflatnum)
            {
                bias = -height / 6;
            }
            else if(sector->SP_floorpic == skyflatnum)
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

            // TODO: Calculate min/max for a and b.
            for(a = -2; a <= 2; ++a)
            {
                for(b = -2; b <= 2; ++b)
                {
                    if(x + a < 0 || y + b < 0 || x + a > lgBlockWidth - 1 ||
                       y + b > lgBlockHeight - 1) continue;

                    other = GRID_BLOCK(x + a, y + b);

                    if(other->flags & GBF_CHANGED)
                    {
                        LG_ApplySector(other, color, sector->lightlevel,
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
}

/*
 * Calculate the light level for a 3D point in the world.
 *
 * @param point  3D point.
 * @param color  Evaluated color of the point (return value).
 */
void LG_Evaluate(const float *point, byte *color)
{
    int x, y, i;
    int dz = 0;
    float dimming;
    gridblock_t *block;

    if(!lgInited)
    {
        memset(color, 0, 3);
        return;
    }

    x = ((FLT2FIX(point[VX]) - lgOrigin.x) / blockSize) >> FRACBITS;
    y = ((FLT2FIX(point[VY]) - lgOrigin.y) / blockSize) >> FRACBITS;
    x = MINMAX_OF(1, x, lgBlockWidth - 2);
    y = MINMAX_OF(1, y, lgBlockHeight - 2);

    block = &grid[y * lgBlockWidth + x];

    if(block->sector)
    {
        if(block->bias < 0)
        {
            // Calculate Z difference to the ceiling.
            dz = (block->sector->SP_ceilheight
                  - FLT2FIX(point[VZ])) >> FRACBITS;
        }
        else if(block->bias > 0)
        {
            // Calculate Z difference to the floor.
            dz = (FLT2FIX(point[VZ])
                  - block->sector->SP_floorheight) >> FRACBITS;
        }

        dz -= 50;
        if(dz < 0)
            dz = 0;

        memcpy(color, block->rgb, 3);
    }
#if 0
    else
    {
        // DEBUG:
        // Bright purple if the block doesn't have a sector.
        color[0] = 254;
        color[1] = 0;
        color[2] = 254;
    }
#endif

    // Biased ambient light causes a dimming in the Z direction.
    if(dz && block->bias)
    {
        if(block->bias < 0)
            dimming = 1 - (dz * -block->bias) / 35000.0f;
        else
            dimming = 1 - (dz * block->bias) / 35000.0f;

        if(dimming < .5f)
            dimming = .5f;

        for(i = 0; i < 3; ++i)
        {
            // Add the light range compression factor
            color[i] += Rend_GetLightAdaptVal((int)color[i]);

            // Apply the dimming
            color[i] *= dimming;
        }
    }
    else
    {
        // Just add the light range compression factor
        for(i = 0; i < 3; ++i)
            color[i] += Rend_GetLightAdaptVal((int)color[i]);
    }
}

/*
 * Draw the grid in 2D HUD mode.
 */
void LG_Debug(void)
{
    gridblock_t *block;
    int x, y;

    if(!lgInited || !lgShowDebug)
        return;

    // Go into screen projection mode.
    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);

    gl.Disable(DGL_TEXTURING);

    for(block = grid, y = 0; y < lgBlockHeight; ++y)
    {
        gl.Begin(DGL_QUADS);
        for(x = 0; x < lgBlockWidth; ++x, ++block)
        {
            if(!block->sector)
                continue;

            gl.Color3ubv(block->rgb);

            gl.Vertex2f(x * lgDebugSize, y * lgDebugSize);
            gl.Vertex2f(x * lgDebugSize + lgDebugSize, y * lgDebugSize);
            gl.Vertex2f(x * lgDebugSize + lgDebugSize,
                        y * lgDebugSize + lgDebugSize);
            gl.Vertex2f(x * lgDebugSize, y * lgDebugSize + lgDebugSize);
        }
        gl.End();
    }

    gl.Enable(DGL_TEXTURING);
    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();
}
