/** @file lightgrid.cpp  Light Grid (Large-Scale FakeRadio).
 *
 * @authors Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "render/lightgrid.h"

#include <QFlags>
#include <QSet>
#include <QVector>
#include <de/math.h>
#include <de/Log>

#include "con_main.h"

// Cvars:
static int lgEnabled   = false;
static int lgBlockSize = 31;

namespace de {

namespace internal
{
    enum LightBlockFlag
    {
        Changed     = 0x1,  ///< Primary contribution has changed.
        Contributor = 0x2,  ///< Secondary contribution has changed.

        AllFlags = Changed | Contributor
    };
    Q_DECLARE_FLAGS(LightBlockFlags, LightBlockFlag)

    /**
     * Determines if the bit in @a bitfield (assumed 32-bit) is set for the given
     * grid reference @a gref.
     */
    static bool hasIndexBit(LightGrid::Ref const &gref, int gridWidth, uint *bitfield)
    {
        uint const index = gref.x + gref.y * gridWidth;
        return (bitfield[index >> 5] & (1 << (index & 0x1f))) != 0;
    }

    /**
     * Sets the bit in a bitfield (assumed 32-bit) for the given grid reference @a gref.
     * @param count  If set, will be incremented when a zero bit is changed to one.
     */
    static void addIndexBit(LightGrid::Ref const &gref, int gridWidth, uint *bitfield, int *count)
    {
        uint const index = gref.x + gref.y * gridWidth;
        // Are we counting when bits are set?
        if(count && !hasIndexBit(LightGrid::Ref(index, 0), gridWidth, bitfield))
        {
            (*count)++;
        }
        bitfield[index >> 5] |= (1 << (index & 0x1f));
    }
}

Q_DECLARE_OPERATORS_FOR_FLAGS(internal::LightBlockFlags)

using namespace internal;

static Vector4f const black;

DENG2_PIMPL(LightGrid)
{
    Vector2d origin;     ///< Grid origin in map space.
    int blockSize;       ///< In map coordinate space units.
    Vector2i dimensions; ///< Grid dimensions in blocks.

    /**
     * Grid coverage data for a light source.
     */
    struct LightCoverage
    {
        int primaryBlockCount;
        QVector<Index> blocks;
        LightCoverage() : primaryBlockCount(0) {}
    };
    typedef QMap<IBlockLightSource *, LightCoverage> Coverages;
    Coverages coverage;
    bool needUpdateCoverage;

    /**
     * Grid illumination point.
     *
     * Light contributions come from sources of one of two logical types:
     *
     * - @em Primary contributors are the main light source and are linked to the
     *   block directly so that their contribution to neighbors can be tracked.
     *
     * - @em Secondary contributors are additional light sources which contribute
     *   to neighbor blocks. Secondary contributors are not linked to the block as
     *   their contributions can be inferred from primarys at update time.
     */
    struct LightBlock
    {
        LightBlockFlags flags;     ///< Internal state flags.
        char bias;                 ///< If positive the source is shining up from floor.
        IBlockLightSource *source; ///< Primary illumination source (if any).
        Vector3f color;            ///< Accumulated light color (from all sources).
        Vector3f oldColor;         ///< Used if the color has changed and an update is pending.

        /**
         * Construct a new light block using the source specified as the @em primary
         * illumination source for the block.
         *
         * @param primarySource  Primary illumation. Use @c 0 to create a "null-block".
         */
        LightBlock(IBlockLightSource *primarySource = 0)
            : bias(0), source(primarySource)
        {}

        /**
         * Change the flags of the light block.
         *
         * @param flagsToChange  Flags to change the value of.
         * @param operation      Logical operation to perform on the flags.
         */
        void setFlags(LightBlockFlags flagsToChange, FlagOp operation = SetFlags)
        {
            if(!source) return;
            applyFlagOperation(flags, flagsToChange, operation);
        }

        /**
         * Evaluate the ambient color for the light block.
         * @note Blocks with no primary illumination source are always black.
         */
        Vector4f evaluate(/*coord_t height*/) const
        {
            if(!source) return black;

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
                dz = source->visCeiling().height() - height;
            }
            else if(_bias > 0)
            {
                // Calculate Z difference to the floor.
                dz = height - source->visFloor().height();
            }
            dz -= 50;
            if(dz < 0)
                dz = 0;*/

            // If we are awaiting an updated value use the old color.
            Vector4f colorDimmed = flags.testFlag(Changed)? oldColor : color;

            // Biased ambient light causes a dimming on the Z axis.
            /*if(dz && _bias)
            {
                float dimming = 1 - (dz * (float) de::abs(d->bias)) / 35000.0f;
                if(dimming < .5f)
                    dimming = .5f;

                colorDimmed *= dimming;
            }
            */

            // Set the luminance factor.
            colorDimmed.w = (colorDimmed.x + colorDimmed.y + colorDimmed.z) / 3;

            return colorDimmed;
        }

        bool markChanged(bool isContributor = false)
        {
            if(!source) return false;

            if(isContributor)
            {
                // Changes by contributor sectors are simply flagged until an update.
                flags |= Contributor;
                return true;
            }

            // The color will be recalculated.

            if(!(flags & Changed))
            {
                // Remember the color in case we receive any queries before the update.
                oldColor = color;
            }

            flags |= Changed;
            flags |= Contributor;

            // Init to black in preparation for the update.
            color = Vector3f(0, 0, 0);
            return true;
        }

        /**
         * Apply an illumination to the block.
         */
        void applyLightingChanges(Vector4f const &contrib, int sourceBias, float factor)
        {
            if(!source) return;

            // Apply a bias to the light level.
            float level = contrib.w;
            level -= (0.95f - level);
            if(level < 0)
                level = 0;

            level *= factor;

            if(level <= 0)
                return;

            for(int i = 0; i < 3; ++i)
            {
                float c = de::clamp(0.f, contrib[i] * level, 1.f);

                if(color[i] + c > 1)
                {
                    color[i] = 1;
                }
                else
                {
                    color[i] += c;
                }
            }

            // Influenced by the source bias.
            bias = de::clamp(-0x80, int(bias * (1 - factor) + sourceBias * factor), 0x7f);
        }
    };

    /// The One "null" block takes the place of empty blocks in the grid.
    LightBlock nullBlock;

    /// The grid of LightBlocks. All unused point at @var nullBlock.
    typedef QVector<LightBlock *> Blocks;
    Blocks blocks;
    bool needUpdate;

    int numBlocks; ///< Total number of non-null blocks.

    Instance(Public *i)
        : Base(i)
        , blockSize(0)
        , needUpdateCoverage(false)
        , needUpdate(false)
        , numBlocks(0)
    {}

    ~Instance()
    {
        clearBlocks();
    }

    inline LightBlock &block(Index index)     { return *blocks[index]; }
    inline LightBlock &block(Ref const &gref) { return block(self.toIndex(gref)); }

    void clearBlocks()
    {
        for(int i = 0; i < blocks.count(); ++i)
        {
            if(blocks[i] != &nullBlock)
            {
                delete blocks[i];
                blocks[i] = &nullBlock;
            }
        }

        // A grid of null blocks needs no coverage data or future updates.
        coverage.clear();
        needUpdate = needUpdateCoverage = false;
    }

    void resizeAndClearBlocks(Vector2i const &newDimensions)
    {
        dimensions = newDimensions;
        blocks.resize(dimensions.x * dimensions.y);
        clearBlocks();
    }

    // Find the affected and contributed blocks of all light sources.
    void updateCoverageIfNeeded()
    {
        if(!needUpdateCoverage) return;
        needUpdateCoverage = false;

        // Bitfields for marking affected blocks. Make sure each bit is in a word.
        size_t const bitfieldSize = 4 * (31 + dimensions.x * dimensions.y) / 32;
        uint *primaryBitfield     = (uint *) M_Calloc(bitfieldSize);
        uint *contribBitfield     = (uint *) M_Calloc(bitfieldSize);

        // Reset the coverage data for all primary light sources.
        coverage.clear();
        foreach(LightBlock *block, blocks)
        {
            if(block->source && !coverage.contains(block->source))
            {
                coverage.insert(block->source, LightCoverage());
            }
        }

        for(Coverages::iterator it = coverage.begin(); it != coverage.end(); ++it)
        {
            IBlockLightSource *source = it.key();
            LightCoverage &covered    = it.value();

            // Determine blocks for which this is the primary source.
            int primaryCount = 0;
            std::memset(primaryBitfield, 0, bitfieldSize);

            for(int y = 0; y < dimensions.y; ++y)
            for(int x = 0; x < dimensions.x; ++x)
            {
                // Does this block have a different primary source?
                if(source != block(Ref(x, y)).source)
                {
                    continue;
                }

                /// Primary sources affect near neighbors due to smoothing.
                /// @todo Determine min/max a/b before going into the loop.
                for(int b = -2; b <= 2; ++b)
                {
                    if(y + b < 0 || y + b >= dimensions.y)
                        continue;

                    for(int a = -2; a <= 2; ++a)
                    {
                        if(x + a < 0 || x + a >= dimensions.x)
                            continue;

                        addIndexBit(Ref(x + a, y + b), dimensions.x, primaryBitfield, &primaryCount);
                    }
                }
            }

            // Determine blocks for which this is the secondary contributor.
            int contribCount = 0;
            std::memset(contribBitfield, 0, bitfieldSize);

            for(int y = 0; y < dimensions.y; ++y)
            for(int x = 0; x < dimensions.x; ++x)
            {
                if(!hasIndexBit(Ref(x, y), dimensions.x, primaryBitfield))
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

                        if(!hasIndexBit(Ref(x + a, y + b), dimensions.x, primaryBitfield))
                        {
                            addIndexBit(Ref(x + a, y + b), dimensions.x, contribBitfield, &contribCount);
                        }
                    }
                }
            }

            // Remember grid coverage for this illumination source.
            int const blockCount = primaryCount + contribCount;
            covered.primaryBlockCount = primaryCount;
            covered.blocks.resize(blockCount);

            if(blockCount > 0)
            {
                int a = 0, b = primaryCount;
                for(int x = 0; x < dimensions.x * dimensions.y; ++x)
                {
                    if(hasIndexBit(Ref(x, 0), dimensions.x, primaryBitfield))
                    {
                        covered.blocks[a++] = x;
                    }
                    else if(hasIndexBit(Ref(x, 0), dimensions.x, contribBitfield))
                    {
                        covered.blocks[b++] = x;
                    }
                }

                DENG2_ASSERT(a == primaryCount); // sanity check
            }
        }

        M_Free(primaryBitfield);
        M_Free(contribBitfield);

        // A full update is needed after this.
        self.scheduleFullUpdate();
    }
};

LightGrid::LightGrid(Vector2d const &origin, Vector2d const &dimensions)
    : d(new Instance(this))
{
    resizeAndClear(origin, dimensions);
}

void LightGrid::resizeAndClear(Vector2d const &newOrigin, Vector2d const &newDimensions)
{
    d->origin    = newOrigin;
    d->blockSize = lgBlockSize;

    // Determine the dimensions of the grid (in blocks)
    Vector2d const blockDimensions = newDimensions / d->blockSize;

    // (Re)-initialize an empty light grid.
    d->resizeAndClearBlocks(Vector2i(de::round<int>(blockDimensions.x) + 1,
                                     de::round<int>(blockDimensions.y) + 1));
}

Vector4f LightGrid::evaluate(Vector3d const &point)
{
    // If not enabled there is no lighting to evaluate; return black.
    if(!lgEnabled) return black;
    return d->block(toRef(point)).evaluate();
}

void LightGrid::scheduleFullUpdate()
{
    if(d->blocks.isEmpty()) return;

    d->updateCoverageIfNeeded();

    // Mark all non-null blocks.
    foreach(Instance::LightBlock *block, d->blocks)
    {
        block->markChanged();
        block->markChanged(true);
    }
    d->needUpdate = true;
}

void LightGrid::updateIfNeeded()
{
    // Updates are unnecessary if not enabled.
    if(!lgEnabled) return;

    d->updateCoverageIfNeeded();

    // Any work to do?
    if(!d->needUpdate) return;
    d->needUpdate = false;

    static float const factors[5 * 5] =
    {
        .1f,  .2f, .25f, .2f, .1f,
        .2f,  .4f, .5f,  .4f, .2f,
        .25f, .5f, 1.f,  .5f, .25f,
        .2f,  .4f, .5f,  .4f, .2f,
        .1f,  .2f, .25f, .2f, .1f
    };

    for(int y = 0; y < d->dimensions.y; ++y)
    for(int x = 0; x < d->dimensions.x; ++x)
    {
        Instance::LightBlock &blockAtRef = d->block(Ref(x, y));

        // No contribution?
        if(!blockAtRef.flags.testFlag(Contributor))
            continue;

        // Determine the ambient light properties of this block.
        IBlockLightSource &source = *blockAtRef.source;
        Vector4f const color      = Vector4f(source.lightSourceColorf(), source.lightSourceIntensity(Vector3d(0, 0, 0)));
        int const bias            = source.blockLightSourceZBias();

        /// @todo Calculate min/max for a and b.
        for(int a = -2; a <= 2; ++a)
        for(int b = -2; b <= 2; ++b)
        {
            if(x + a < 0 || y + b < 0 ||
               x + a > d->dimensions.x - 1 || y + b > d->dimensions.y - 1)
                continue;

            Instance::LightBlock &other = d->block(Ref(x + a, y + b));
            if(!other.flags.testFlag(Changed))
                continue;

            other.applyLightingChanges(color, bias, factors[(b + 2) * 5 + a + 2] / 8);
        }
    }

    // Clear all changed and contribution flags for all non-null blocks.
    foreach(Instance::LightBlock *block, d->blocks)
    {
        block->setFlags(AllFlags, UnsetFlags);
    }
}

void LightGrid::setPrimarySource(Index index, IBlockLightSource *newSource)
{
    Instance::LightBlock *block = &d->block(index);

    if(newSource == block->source)
        return;

    if(newSource && !block->source)
    {
        // Replace the "null block" with a new light block.
        d->blocks[index] = block = new Instance::LightBlock(newSource);
        d->numBlocks++;
    }
    else if(!newSource && block->source)
    {
        // Replace the existing light block with the "null block".
        delete block;
        d->blocks[index] = block = &d->nullBlock;
        d->numBlocks--;
    }

    block->source = newSource;

    // A full update is needed.
    d->needUpdate = d->needUpdateCoverage = true;
}

LightGrid::IBlockLightSource *LightGrid::primarySource(Index index) const
{
    return d->block(index).source;
}

void LightGrid::primarySourceLightChanged(IBlockLightSource *changed)
{
    // Updates are unnecessary if not enabled.
    if(!lgEnabled) return;

    if(!changed) return;

    d->updateCoverageIfNeeded();

    Instance::Coverages::iterator &covered = d->coverage.find(changed);
    if(covered == d->coverage.end()) return;

    if(covered->blocks.count())
    {
        // Mark primary and contributed blocks.
        for(int i = 0; i < covered->primaryBlockCount; ++i)
        {
            if(d->block(covered->blocks[i]).markChanged())
            {
                d->needUpdate = true;
            }
        }
        for(int i = 0; i < covered->blocks.count(); ++i)
        {
            if(d->block(covered->blocks[i]).markChanged(true /*is contributor*/))
            {
                d->needUpdate = true;
            }
        }
    }
}

LightGrid::Ref LightGrid::toRef(Vector3d const &point)
{
    int x = de::round<int>((point.x - d->origin.x) / d->blockSize);
    int y = de::round<int>((point.y - d->origin.y) / d->blockSize);

    return Ref(de::clamp(1, x, dimensions().x - 2),
               de::clamp(1, y, dimensions().y - 2));
}

int LightGrid::blockSize() const
{
    return d->blockSize;
}

Vector2d const &LightGrid::origin() const
{
    return d->origin;
}

Vector2i const &LightGrid::dimensions() const
{
    return d->dimensions;
}

int LightGrid::numBlocks() const
{
    return d->numBlocks;
}

size_t LightGrid::blockStorageSize() const
{
    return sizeof(Instance::LightBlock) * d->numBlocks;
}

Vector3f const &LightGrid::rawColorRef(Index index) const
{
    return d->block(index).color;
}

void LightGrid::consoleRegister() // static
{
    C_VAR_INT("rend-bias-grid",             &lgEnabled,     0,  0, 1);
    C_VAR_INT("rend-bias-grid-blocksize",   &lgBlockSize,   0,  8, 1024);
}

} // namespace de
