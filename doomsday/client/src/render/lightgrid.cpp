/** @file render/lightgrid.cpp Light Grid (Large-Scale FakeRadio).
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

#include <de/memory.h>
#include <de/memoryzone.h>

#include <QFlags>
#include <QVector>

#include <de/math.h>
#include <de/Log>

#include "de_console.h"

#include "BspLeaf"
#include "Sector"
#include "world/map.h"
#include "world/p_maputil.h" // P_IsPointInBspLeaf
#include "world/p_players.h" // viewPlayer

#include "render/rend_main.h"

#include "render/lightgrid.h"

using namespace de;

static int   lgEnabled   = false;

static int   lgShowDebug = false;
static float lgDebugSize = 1.5f;

static int   lgBlockSize = 31;
static int   lgMXSample  = 1; ///< 5 samples per block

void LightGrid::consoleRegister() // static
{
    C_VAR_INT   ("rend-bias-grid",              &lgEnabled,     0,              0, 1);
    C_VAR_INT   ("rend-bias-grid-debug",        &lgShowDebug,   CVF_NO_ARCHIVE, 0, 1);
    C_VAR_FLOAT ("rend-bias-grid-debug-size",   &lgDebugSize,   0,              .1f, 100);
    C_VAR_INT   ("rend-bias-grid-blocksize",    &lgBlockSize,   0,              8, 1024);
    C_VAR_INT   ("rend-bias-grid-multisample",  &lgMXSample,    0,              0, 7);
}

/**
 * Determines the Z-axis bias scale factor for the given @a sector.
 */
static int biasForSector(Sector const &sector)
{
    int const height = int(sector.ceiling().height() - sector.floor().height());
    bool hasSkyFloor = sector.floorSurface().hasSkyMaskedMaterial();
    bool hasSkyCeil  = sector.ceilingSurface().hasSkyMaskedMaterial();

    if(hasSkyFloor && !hasSkyCeil)
    {
        return -height / 6;
    }
    if(!hasSkyFloor && hasSkyCeil)
    {
        return height / 6;
    }
    if(height > 100)
    {
        return (height - 100) / 2;
    }
    return 0;
}

class LightBlock
{
public:
    enum Flag
    {
        /// Grid block sector light has changed.
        Changed     = 0x1,

        /// Contributes light to some other block.
        Contributor = 0x2,

        AllFlags = Changed | Contributor
    };
    Q_DECLARE_FLAGS(Flags, Flag)

public:
    /**
     * Construct a new light block.
     *
     * @param  Primary lighting contributor for the block. Can be @c 0
     *         (to create a "null-block").
     */
    LightBlock(Sector *sector = 0);

    /**
     * Returns the @em primary sector attributed to the light block
     * (contributing sectors are not linked).
     */
    Sector &sector() const;

    /**
     * Returns a copy of the flags of the light block.
     */
    Flags flags() const;

    /**
     * Change the flags of the light block.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(Flags flagsToChange, FlagOp operation = SetFlags);

    /**
     * Evaluate the ambient color for the light block.
     */
    Vector3f evaluate(/*coord_t height*/) const;

    void markChanged(bool isContributor = false);

    /**
     * Apply the sector's lighting to the block.
     */
    void applySector(Vector3f const &color, float level, int bias, float factor);

    /**
     * Provides immutable access to the "raw" color (i.e., non-biased) for the
     * block. Primarily intended for debugging.
     */
    Vector3f const &rawColorRef() const;

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(LightBlock::Flags)

DENG2_PIMPL_NOREF(LightBlock)
{
    /// Primary sector attributed to this block.
    Sector *sector;

    /// State flags.
    Flags flags;

    /// Positive bias means that the light is shining in the floor of the sector.
    char bias;

    /// Color of the light:
    Vector3f color;

    /// Used instead of @var rgb if the lighting in this block has changed
    /// and a full grid update is needed.
    Vector3f oldColor;

    Instance(Sector *sector)
        : sector(sector), bias(0)
    {}
};

LightBlock::LightBlock(Sector *sector)
    : d(new Instance(sector))
{}

Sector &LightBlock::sector() const
{
    DENG_ASSERT(d->sector != 0);
    return *d->sector;
}

LightBlock::Flags LightBlock::flags() const
{
    return d->flags;
}

void LightBlock::setFlags(LightBlock::Flags flagsToChange, FlagOp operation)
{
    if(!d->sector) return;
    applyFlagOperation(d->flags, flagsToChange, operation);
}

Vector3f LightBlock::evaluate(/*coord_t height*/) const
{
    // If not attributed to a sector, the color is always black.
    if(!d->sector) return Vector3f(0, 0, 0);

    /**
     * Biased light dimming disabled because this does not work well enough.
     * The problem is that two points on a given surface may be determined to
     * be in different blocks and as the height is taken from the block linked
     * sector this results in very uneven lighting.
     *
     * Biasing is a good idea but the plane heights must come from the sector
     * at the exact X|Y coordinates of the sample point, not the "quantized"
     * references in the light grid. -ds
     */

    /*
    coord_t dz = 0;
    if(_bias < 0)
    {
        // Calculate Z difference to the ceiling.
        dz = d->sector->ceiling().height() - height;
    }
    else if(_bias > 0)
    {
        // Calculate Z difference to the floor.
        dz = height - d->sector->floor().height();
    }
    dz -= 50;
    if(dz < 0)
        dz = 0;*/

    // If we are awaiting an updated value use the old color.
    Vector3f color = d->flags.testFlag(Changed)? d->oldColor : d->color;

    // Biased ambient light causes a dimming on the Z axis.
    /*if(dz && _bias)
    {
        float dimming = 1 - (dz * (float) de::abs(d->bias)) / 35000.0f;
        if(dimming < .5f)
            dimming = .5f;

        color *= dimming;
    }
    */

    return color;
}

void LightBlock::markChanged(bool isContributor)
{
    if(!d->sector) return;

    if(isContributor)
    {
        // Changes by contributor sectors are simply flagged until an update.
        d->flags |= Contributor;
        return;
    }

    // The color will be recalculated.
    d->flags |= Changed;
    d->flags |= Contributor;

    if(!(d->flags & Changed))
    {
        // Remember the color in case we receive any queries before the update.
        d->oldColor = d->color;
    }

    // Init to black in preparation for the update.
    d->color = Vector3f(0, 0, 0);
}

void LightBlock::applySector(Vector3f const &color, float level, int bias, float factor)
{
    if(!d->sector) return;

    // Apply a bias to the light level.
    level -= (0.95f - level);
    if(level < 0)
        level = 0;

    level *= factor;

    if(level <= 0)
        return;

    for(int i = 0; i < 3; ++i)
    {
        float c = de::clamp(0.f, color[i] * level, 1.f);

        if(d->color[i] + c > 1)
        {
            d->color[i] = 1;
        }
        else
        {
            d->color[i] += c;
        }
    }

    // Influenced by the source bias.
    d->bias = de::clamp(-0x80, int(d->bias * (1 - factor) + bias * factor), 0x7f);
}

Vector3f const &LightBlock::rawColorRef() const
{
    return d->color;
}

/**
 * Determines if the index for the specified map point is in the bitfield.
 */
static bool hasIndexBit(LightGrid::Ref const &gref, int gridWidth, uint *bitfield)
{
    uint index = gref.x + gref.y * gridWidth;

    // Assume 32-bit uint.
    return (bitfield[index >> 5] & (1 << (index & 0x1f))) != 0;
}

/**
 * Sets the index for the specified map point in the bitfield.
 * Count is incremented when a zero bit is changed to one.
 */
static void addIndexBit(LightGrid::Ref const &gref, int gridWidth, uint *bitfield, int *count)
{
    uint index = gref.x + gref.y * gridWidth;

    // Assume 32-bit uint.
    if(!hasIndexBit(LightGrid::Ref(index, 0), gridWidth, bitfield))
    {
        (*count)++;
    }
    bitfield[index >> 5] |= (1 << (index & 0x1f));
}

typedef QVector<LightBlock *> Blocks;

/// The special null-block.
static LightBlock nullBlock;

/// Returns @c true iff @a block is the special "null-block".
static inline bool isNullBlock(LightBlock const &block) {
    return &block == &nullBlock;
}

DENG2_PIMPL(LightGrid),
DENG2_OBSERVES(Sector, LightColorChange),
DENG2_OBSERVES(Sector, LightLevelChange)
{
    /// Map for which we provide an ambient lighting grid.
    Map &map;

    /// Origin of the grid in the map coordinate space.
    Vector2d origin;

    /// Size of a block (box axes) in map coordinate space units.
    int blockSize;

    /// Dimensions of the grid in blocks.
    Ref dimensions;

    /// The grid of LightBlocks.
    Blocks grid;

    /// Set to @c true when a full update is needed.
    bool needUpdate;

    Instance(Public *i, Map &map)
        : Base(i),
          map(map),
          needUpdate(false)
    {}

    ~Instance()
    {
        foreach(LightBlock *block, grid)
        {
            if(!block) continue;
            Sector &sector = block->sector();
            sector.audienceForLightLevelChange -= this;
            sector.audienceForLightColorChange -= this;
        }

        qDeleteAll(grid);
    }

    /**
     * Convert a light grid reference to a grid index.
     */
    inline Index toIndex(int x, int y) { return y * dimensions.x + x; }

    /// @copydoc toIndex
    inline Index toIndex(Ref const &gref) { return toIndex(gref.x, gref.y); }

    /**
     * Convert a point in the map coordinate space to a light grid reference.
     */
    Ref toRef(Vector3d const &point)
    {
        int x = de::round<int>((point.x - origin.x) / blockSize);
        int y = de::round<int>((point.y - origin.y) / blockSize);

        return Ref(de::clamp(1, x, dimensions.x - 2),
                   de::clamp(1, y, dimensions.y - 2));
    }

    /**
     * Retrieve the block at the specified light grid index. If no block exists
     * at this index the special "null-block" is returned.
     */
    LightBlock &lightBlock(Index idx)
    {
        DENG_ASSERT(idx >= 0 && idx < grid.size());
        LightBlock *block = grid[idx];
        if(block) return *block;
        return nullBlock;
    }

    /**
     * Retrieve the block at the specified light grid reference.
     */
    LightBlock &lightBlock(Ref const &gref) { return lightBlock(toIndex(gref)); }

    /// @copydoc lightBlock
    inline LightBlock &lightBlock(int x, int y) { return lightBlock(Ref(x, y)); }

    /**
     * Same as @ref lightBlock except @a point is in the map coordinate space.
     */
    inline LightBlock &lightBlock(Vector3d const &point)
    {
        return lightBlock(toRef(point));
    }

    /**
     * Fully (re)-initialize the light grid.
     */
    void initialize()
    {
        // Diagonal in maze arrangement of natural numbers.
        // Up to 65 samples per-block(!)
        static int const MSFACTORS = 7;
        static int multisample[] = {1, 5, 9, 17, 25, 37, 49, 65};

        de::Time begunAt;

        LOG_AS("LightGrid::initialize");

        // Determine the origin of the grid in the map coordinate space.
        origin = Vector2d(map.bounds().min);

        // Once initialized the blocksize cannot be changed (requires a full grid
        // update) so remember this value.
        blockSize = lgBlockSize;

        // Determine the dimensions of the grid (in blocks)
        Vector2d mapDimensions = Vector2d(map.bounds().max) - Vector2d(map.bounds().min);
        dimensions = Vector2i(de::round<int>(mapDimensions.x / blockSize) + 1,
                              de::round<int>(mapDimensions.y / blockSize) + 1);

        // Determine how many sector samples per block.
        int numSamples = multisample[de::clamp(0, lgMXSample, MSFACTORS)];

        // Allocate memory for sample points data.
        Vector2d *samplePoints = new Vector2d[numSamples];
        int *sampleResults     = new int[numSamples];

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
        Sector **ssamples = (Sector **) M_Malloc(sizeof(Sector *) * ((dimensions.x * dimensions.y) * numSamples));

        // Determine the size^2 of the samplePoint array plus its center.
        int size = 0, center = 0;
        if(numSamples > 1)
        {
            float f = sqrt(float(numSamples));

            if(std::ceil(f) != std::floor(f))
            {
                size = sqrt(float(numSamples - 1));
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
        // (dimensions.y * dimensions.x) * numSamples
        if(center == 0)
        {
            // Zero is the center so do that first.
            samplePoints[0] = Vector2d(blockSize / 2, blockSize / 2);
        }

        if(numSamples > 1)
        {
            coord_t bSize = (coord_t) blockSize / (size - 1);

            // Is there an offset?
            int n = (center == 0? 1 : 0);

            for(int y = 0; y < size; ++y)
            for(int x = 0; x < size; ++x, ++n)
            {
                samplePoints[n] =
                    Vector2d(de::round<double>(x * bSize), de::round<double>(y * bSize));
            }
        }

        Vector2d samplePoint;

        // Acquire the sectors at ALL the sample points.
        for(int y = 0; y < dimensions.y; ++y)
        for(int x = 0; x < dimensions.x; ++x)
        {
            Index const blk = toIndex(x, y);

            Vector2d off(x * blockSize, y * blockSize);

            int n = 0;
            if(center == 0)
            {
                // Center point is not considered with the term 'size'.
                // Sample this point and place at index 0 (at the start
                // of the samples for this block).
                int idx = blk * (numSamples);

                samplePoint = origin + off + samplePoints[0];

                BspLeaf *bspLeaf = map.bspLeafAtPoint(samplePoint);
                if(P_IsPointInBspLeaf(samplePoint, *bspLeaf))
                    ssamples[idx] = bspLeaf->sectorPtr();
                else
                    ssamples[idx] = 0;

                n++; // Offset the index in the samplePoints array bellow.
            }

            int count = blk * size;
            for(int b = 0; b < size; ++b)
            {
                int i = (b + count) * size;

                for(int a = 0; a < size; ++a, ++n)
                {
                    int idx = a + i + (center == 0? blk + 1 : 0);

                    if(numSamples > 1 && ((x > 0 && a == 0) || (y > 0 && b == 0)))
                    {
                        // We have already sampled this point.
                        // Get the previous result.
                        Ref prev(x, y);
                        Ref prevB(a, b);
                        int prevIdx;

                        if(x > 0 && a == 0)
                        {
                            prevB.x = size -1;
                            prev.x--;
                        }
                        if(y > 0 && b == 0)
                        {
                            prevB.y = size -1;
                            prev.y--;
                        }

                        prevIdx = prevB.x + (prevB.y + toIndex(prev) * size) * size;

                        if(center == 0)
                            prevIdx += toIndex(prev) + 1;

                        ssamples[idx] = ssamples[prevIdx];
                    }
                    else
                    {
                        // We haven't sampled this point yet.
                        samplePoint = origin + off + samplePoints[n];

                        BspLeaf *bspLeaf = map.bspLeafAtPoint(samplePoint);
                        if(P_IsPointInBspLeaf(samplePoint, *bspLeaf))
                            ssamples[idx] = bspLeaf->sectorPtr();
                        else
                            ssamples[idx] = 0;
                    }
                }
            }
        }

        // We're done with the samplePoints block.
        delete[] samplePoints; samplePoints = 0;

        // Bitfields for marking affected blocks. Make sure each bit is in a word.
        size_t bitfieldSize = 4 * (31 + dimensions.x * dimensions.y) / 32;

        uint *indexBitfield       = (uint *) M_Calloc(bitfieldSize);
        uint *contributorBitfield = (uint *) M_Calloc(bitfieldSize);

        // Allocate memory used for the collection of the sample results.
        Sector **blkSampleSectors = (Sector **) M_Malloc(sizeof(Sector *) * numSamples);

        /*
         * Initialize the light block grid.
         */
        grid.fill(NULL, dimensions.x * dimensions.y);
        int numBlocks = 0;

        for(int y = 0; y < dimensions.y; ++y)
        for(int x = 0; x < dimensions.x; ++x)
        {
            /**
             * Pick the sector at each of the sample points.
             * @todo We don't actually need the blkSampleSectors array
             * anymore. Now that ssamples stores the results consecutively
             * a simple index into ssamples would suffice.
             * However if the optimization to save memory is implemented as
             * described in the comments above we WOULD still require it.
             * Therefore, for now I'm making use of it to clarify the code.
             */
            Index idx = toIndex(x, y) * numSamples;
            for(int i = 0; i < numSamples; ++i)
            {
                blkSampleSectors[i] = ssamples[i + idx];
            }

            Sector *sector = 0;

            if(numSamples == 1)
            {
                sector = blkSampleSectors[center];
            }
            else
            {
                // Pick the sector which had the most hits.
                int best = -1;
                std::memset(sampleResults, 0, sizeof(int) * numSamples);

                for(int i = 0; i < numSamples; ++i)
                {
                    if(!blkSampleSectors[i]) continue;

                    for(int k = 0; k < numSamples; ++k)
                    {
                        if(blkSampleSectors[k] == blkSampleSectors[i] && blkSampleSectors[k])
                        {
                            sampleResults[k]++;
                            if(sampleResults[k] > best)
                                best = i;
                        }
                    }
                }

                if(best != -1)
                {
                    // Favour the center sample if its a draw.
                    if(sampleResults[best] == sampleResults[center] &&
                       blkSampleSectors[center])
                        sector = blkSampleSectors[center];
                    else
                        sector = blkSampleSectors[best];
                }
            }

            if(!sector)
                continue;

            // Insert a new light block in the grid.
            grid[toIndex(x, y)] = new LightBlock(sector);

            // There is now one more block.
            numBlocks++;

            // We want notification when the sector light properties change.
            sector->audienceForLightLevelChange += this;
            sector->audienceForLightColorChange += this;
        }

        LOG_INFO("%i light blocks (%u bytes).")
            << numBlocks << (sizeof(LightBlock) * numBlocks);

        // We're done with sector samples completely.
        M_Free(ssamples);
        // We're done with the sample results arrays.
        M_Free(blkSampleSectors);

        delete[] sampleResults;

        // Find the blocks of all sectors.
        foreach(Sector *sector, theMap->sectors())
        {
            int count = 0, changedCount = 0;

            if(sector->sideCount())
            {
                // Clear the bitfields.
                std::memset(indexBitfield, 0, bitfieldSize);
                std::memset(contributorBitfield, 0, bitfieldSize);

                for(int y = 0; y < dimensions.y; ++y)
                for(int x = 0; x < dimensions.x; ++x)
                {
                    LightBlock &block = lightBlock(x, y);
                    if(isNullBlock(block) || &block.sector() != sector)
                        continue;

                    /// @todo Determine min/max a/b before going into the loop.
                    for(int b = -2; b <= 2; ++b)
                    {
                        if(y + b < 0 || y + b >= dimensions.y)
                            continue;

                        for(int a = -2; a <= 2; ++a)
                        {
                            if(x + a < 0 || x + a >= dimensions.x)
                                continue;

                            addIndexBit(Ref(x + a, y + b), dimensions.x,
                                        indexBitfield, &changedCount);
                        }
                    }
                }

                // Determine contributor blocks. Contributors are the blocks that are
                // close enough to contribute light to affected blocks.
                for(int y = 0; y < dimensions.y; ++y)
                for(int x = 0; x < dimensions.x; ++x)
                {
                    if(!hasIndexBit(Ref(x, y), dimensions.x, indexBitfield))
                        continue;

                    // Add the contributor blocks.
                    for(int b = -2; b <= 2; ++b)
                    {
                        if(y + b < 0 || y + b >= dimensions.y)
                            continue;

                        for(int a = -2; a <= 2; ++a)
                        {
                            if(x + a < 0 || x + a >= dimensions.x)
                                continue;

                            if(!hasIndexBit(Ref(x + a, y + b), dimensions.x, indexBitfield))
                            {
                                addIndexBit(Ref(x + a, y + b), dimensions.x, contributorBitfield, &count);
                            }
                        }
                    }
                }
            }

            // LOG_DEBUG("  Sector %i: %i / %i") << theMap->sectorIndex(s) << changedCount << count;

            Sector::LightGridData &lgData = sector->lightGridData();
            lgData.changedBlockCount = changedCount;
            lgData.blockCount = changedCount + count;

            if(lgData.blockCount > 0)
            {
                lgData.blocks = (LightGrid::Index *)
                    Z_Malloc(sizeof(*lgData.blocks) * lgData.blockCount, PU_MAPSTATIC, 0);

                int a = 0, b = changedCount;
                for(int x = 0; x < dimensions.x * dimensions.y; ++x)
                {
                    if(hasIndexBit(Ref(x, 0), dimensions.x, indexBitfield))
                        lgData.blocks[a++] = x;
                    else if(hasIndexBit(Ref(x, 0), dimensions.x, contributorBitfield))
                        lgData.blocks[b++] = x;
                }

                DENG_ASSERT(a == changedCount);
                //DENG_ASSERT(b == blockCount);
            }
        }

        M_Free(indexBitfield);
        M_Free(contributorBitfield);

        self.markAllForUpdate();

        // How much time did we spend?
        LOG_INFO(String("Done in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
    }

    /**
     * To be called when the ambient lighting properties in the sector change.
     */
    void sectorChanged(Sector &sector)
    {
        // Do not update if not enabled.
        /// @todo We could dynamically join/leave the relevant audiences.
        if(!lgEnabled) return;

        Sector::LightGridData &lgData = sector.lightGridData();
        if(!lgData.changedBlockCount && !lgData.blockCount)
            return;

        // Mark changed blocks and contributors.
        for(uint i = 0; i < lgData.changedBlockCount; ++i)
        {
            lightBlock(lgData.blocks[i]).markChanged();
        }

        for(uint i = 0; i < lgData.blockCount; ++i)
        {
            lightBlock(lgData.blocks[i]).markChanged(true /* is-contributor */);
        }

        needUpdate = true;
    }

    /// Observes Sector LightLevelChange.
    void sectorLightLevelChanged(Sector &sector, float /*oldLightLevel*/)
    {
        sectorChanged(sector);
    }

    /// Observes Sector LightColorChange.
    void sectorLightColorChanged(Sector &sector, Vector3f const & /*oldLightColor*/,
                                 int /*changedComponents*/)
    {
        sectorChanged(sector);
    }
};

LightGrid::LightGrid(Map &map)
    : d(new Instance(this, map))
{
    d->initialize();
}

Vector2i const &LightGrid::dimensions() const
{
    return d->dimensions;
}

coord_t LightGrid::blockSize() const
{
    return d->blockSize;
}

Vector3f LightGrid::evaluate(Vector3d const &point)
{
    // If not enabled the color is black.
    if(!lgEnabled) return Vector3f(0, 0, 0);

    LightBlock &block = d->lightBlock(point);
    Vector3f color = block.evaluate();

    // Apply light range compression.
    for(int i = 0; i < 3; ++i)
    {
        color[i] += Rend_LightAdaptationDelta(color[i]);
    }

    return color;
}

float LightGrid::evaluateLightLevel(Vector3d const &point)
{
    Vector3f color = evaluate(point);
    /// @todo Do not do this at evaluation time; store into another grid.
    return (color.x + color.y + color.z) / 3;
}

void LightGrid::markAllForUpdate()
{
    // Updates are unnecessary if not enabled.
    if(!lgEnabled) return;

    // Mark all blocks and contributors.
    foreach(Sector *sector, d->map.sectors())
    {
        d->sectorChanged(*sector);
    }
}

void LightGrid::update()
{
    static float const factors[5 * 5] =
    {
        .1f,  .2f, .25f, .2f, .1f,
        .2f,  .4f, .5f,  .4f, .2f,
        .25f, .5f, 1.f,  .5f, .25f,
        .2f,  .4f, .5f,  .4f, .2f,
        .1f,  .2f, .25f, .2f, .1f
    };

    // Updates are unnecessary if not enabled.
    if(!lgEnabled) return;

    // Any work to do?
    if(!d->needUpdate) return;

    for(int y = 0; y < d->dimensions.y; ++y)
    for(int x = 0; x < d->dimensions.x; ++x)
    {
        LightBlock &block = d->lightBlock(x, y);

        // No contribution?
        if(!block.flags().testFlag(LightBlock::Contributor))
            continue;

        // Determine the ambient light properties of the sector at this block.
        Sector &sector = block.sector();
        Vector3f const &color = Rend_SectorLightColor(sector);
        float const level     = sector.lightLevel();
        int const bias        = biasForSector(sector);

        /// @todo Calculate min/max for a and b.
        for(int a = -2; a <= 2; ++a)
        for(int b = -2; b <= 2; ++b)
        {
            if(x + a < 0 || y + b < 0 ||
               x + a > d->dimensions.x - 1 || y + b > d->dimensions.y - 1)
                continue;

            LightBlock &other = d->lightBlock(x + a, y + b);

            if(!other.flags().testFlag(LightBlock::Changed))
                continue;

            other.applySector(color, level, bias, factors[(b + 2) * 5 + a + 2] / 8);
        }
    }

    // Clear all changed and contribution flags.
    foreach(LightBlock *block, d->grid)
    {
        if(!block) continue;
        block->setFlags(LightBlock::AllFlags, UnsetFlags);
    }

    d->needUpdate = false;
}

#include "de_graphics.h"
#include "de_render.h"
#include "gl/sys_opengl.h"

void LightGrid::drawDebugVisual()
{
    static Vector3f const red(1, 0, 0);
    static int blink = 0;

    // Disabled?
    if(!lgShowDebug) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Determine the grid reference of the view player.
    Ref viewGRef;
    if(viewPlayer)
    {
        blink++;
        viewGRef = d->toRef(viewPlayer->shared.mo->origin);
    }

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_WINDOW->width(), DENG_WINDOW->height(), 0, -1, 1);

    for(int y = 0; y < d->dimensions.y; ++y)
    {
        glBegin(GL_QUADS);
        for(int x = 0; x < d->dimensions.x; ++x)
        {
            Ref gref(x, d->dimensions.y - 1 - y);
            bool const isViewGRef = (viewPlayer && viewGRef == gref);

            Vector3f const *color;
            if(isViewGRef && (blink & 16))
            {
                color = &red;
            }
            else
            {
                LightBlock &block = d->lightBlock(gref);
                if(isNullBlock(block))
                    continue;

                color = &block.rawColorRef();
            }

            glColor3f(color->x, color->y, color->z);

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
