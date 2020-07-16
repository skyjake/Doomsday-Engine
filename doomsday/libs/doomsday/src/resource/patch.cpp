/** @file patch.cpp Patch Image Format.
 *
 * @authors Copyright &copy; 1999-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/res/patch.h"

#include <de/list.h>
#include <de/rectangle.h>
#include <de/libcore.h>
#include <de/log.h>
#include <de/math.h>

using namespace de;

namespace res {

/**
 * Serialized format header.
 */
struct Header : public IReadable
{
    /// Logical dimensions of the patch in pixels.
    dint16 dimensions[2];

    /// Origin offset (top left) in world coordinate space units.
    dint16 origin[2];

    /// Implements IReadable.
    void operator << (Reader &from) {
        from >> dimensions[0] >> dimensions[1]
             >> origin[0] >> origin[1];
    }
};

/**
 * A @em Post is a run of one or more non-masked pixels.
 */
struct Post : public IReadable
{
    /// Y-Offset to the start of the run in texture space (0-base).
    dbyte topOffset;

    /// Length of the run in pixels (inclusive).
    dbyte length;

    /// Offset to the first pixel palette index in the source data.
    size_t firstPixel;

    /// Implements IReadable.
    void operator << (Reader &from)
    {
        from >> topOffset >> length;
        firstPixel = from.offset() + 1 /*unused junk*/;
    }
};

/// A @em Column is a list of 0 or more posts.
typedef List<Post> Posts;
typedef Posts Column;
typedef List<Column> Columns;

/// Offsets to columns from the start of the source data.
typedef List<dint32> ColumnOffsets;

/**
 * Attempt to read another @a post from the @a reader.
 * @return  @c true if another post was read; otherwise @c false.
 */
static bool readNextPost(Post &post, Reader &reader)
{
    static const int END_OF_POSTS = 0xff;

    // Any more?
    reader.mark();
    dbyte nextByte; reader >> nextByte;
    reader.rewind();
    if (nextByte == END_OF_POSTS) return false;

    // Another post begins.
    reader >> post;
    return true;
}

/**
 * Visit each of @a offsets producing a column => post map.
 */
static Columns readPosts(const ColumnOffsets &offsets, Reader &reader)
{
    Columns columns;
    Post post;
    DE_FOR_EACH_CONST(ColumnOffsets, i, offsets)
    {
        reader.setOffset(*i);

        // A new column begins.
        columns.push_back(Column());
        Column &column = columns.back();

        // Read all posts.
        while (readNextPost(post, reader))
        {
            column.push_back(post);

            // Skip to the next post.
            reader.seek(1 + post.length + 1); // A byte of unsed junk either side of the pixel palette indices.
        }
    }
    return columns;
}

/**
 * Read @a width column offsets from the @a reader.
 */
static ColumnOffsets readColumnOffsets(int width, Reader &reader)
{
    ColumnOffsets offsets;
    for (int col = 0; col < width; ++col)
    {
        dint32 offset; reader >> offset;
        offsets.push_back(offset);
    }
    return offsets;
}

static inline Columns readColumns(int width, Reader &reader)
{
    return readPosts(readColumnOffsets(width, reader), reader);
}

/**
 * Process @a columns to calculate the "real" pixel height of the image.
 */
static int calcRealHeight(const Columns &columns)
{
    auto geom = Rectanglei::fromSize(Vec2ui(1, 0));

    DE_FOR_EACH_CONST(Columns, colIt, columns)
    {
        int tallTop = -1; // Keep track of pos (clipping).

        DE_FOR_EACH_CONST(Posts, postIt, *colIt)
        {
            const Post &post = *postIt;

            // Does this post extend the previous one? (a so-called "tall-patch").
            if (post.topOffset <= tallTop)
                tallTop += post.topOffset;
            else
                tallTop = post.topOffset;

            // Skip invalid posts.
            if (post.length <= 0) continue;

            // Unite the geometry of the post.
            geom |= Rectanglei::fromSize(Vec2i(0, tallTop), Vec2ui(1, post.length));
        }
    }

    return geom.height();
}

static Block compositeImage(Reader &                       reader,
                            const ColorPaletteTranslation *xlatTable,
                            const Columns &                columns,
                            const Patch::Metadata &        meta,
                            Flags                          flags)
{
    const bool maskZero                = flags.testFlag(Patch::MaskZero);
    const bool clipToLogicalDimensions = flags.testFlag(Patch::ClipToLogicalDimensions);

#ifdef DE_DEBUG
    // Is the "logical" height of the image equal to the actual height of the
    // composited pixel posts?
    if (meta.logicalDimensions.y != meta.dimensions.y)
    {
        int postCount = 0;
        DE_FOR_EACH_CONST(Columns, i, columns)
        {
            postCount += i->count();
        }

        LOGDEV_RES_NOTE("Inequal heights, logical: %i != actual: %i (%i %s)")
            << meta.logicalDimensions.y << meta.dimensions.y
            << postCount << (postCount == 1? "post" : "posts");
    }
#endif

    // Determine the dimensions of the output buffer.
    const Vec2ui &dimensions = clipToLogicalDimensions? meta.logicalDimensions : meta.dimensions;
    const int w = dimensions.x;
    const int h = dimensions.y;
    const size_t pels = w * h;

    // Create the output buffer and fill with default color (black) and alpha (transparent).
    Block output{2 * pels};
    output.fill(0);

    // Map the pixel color channels in the output buffer.
    dbyte *top      = output.data();
    dbyte *topAlpha = output.data() + pels;

    // Composite the patch into the output buffer.
    DE_FOR_EACH_CONST(Columns, colIt, columns)
    {
        int tallTop = -1; // Keep track of pos (clipping).

        DE_FOR_EACH_CONST(Posts, postIt, *colIt)
        {
            const Post &post = *postIt;

            // Does this post extend the previous one? (a so-called "tall-patch").
            if (post.topOffset <= tallTop)
                tallTop += post.topOffset;
            else
                tallTop = post.topOffset;

            // Skip invalid posts.
            if (post.length <= 0) continue;

            // Determine the destination height range.
            int y = tallTop;
            int length = post.length;

            // Clamp height range within bounds.
            if (y + length > h)
                length = h - y;

            int offset = 0;
            if (y < 0)
            {
                offset = de::min(-y, length);
                y = 0;
            }

            length = de::max(0, length - offset);

            // Skip empty ranges.
            if (!length) continue;

            // Find the start of the pixel data for the post.
            reader.setOffset(post.firstPixel + offset);

            dbyte *out      = top      + tallTop * w;
            dbyte *outAlpha = topAlpha + tallTop * w;

            // Composite pixels from the post to the output buffer.
            while (length--)
            {
                // Read the next palette index.
                dbyte palIdx;
                reader >> palIdx;

                // Is palette index translation in effect?
                if (xlatTable)
                {
                    palIdx = dbyte(xlatTable->at(palIdx));
                }

                if (!maskZero || palIdx)
                {
                    *out = palIdx;
                }

                if (maskZero)
                    *outAlpha = (palIdx? 0xff : 0);
                else
                    *outAlpha = 0xff;

                // Move one row down.
                out      += w;
                outAlpha += w;
            }
        }

        // Move one column right.
        top++;
        topAlpha++;
    }

    return output;
}

static Patch::Metadata prepareMetadata(const Header &hdr, int realHeight)
{
    Patch::Metadata meta;
    meta.dimensions        = Vec2ui(hdr.dimensions[0], realHeight);
    meta.logicalDimensions = Vec2ui(hdr.dimensions[0], hdr.dimensions[1]);
    meta.origin            = Vec2i (hdr.origin[0], hdr.origin[1]);
    return meta;
}

Patch::Metadata Patch::loadMetadata(const IByteArray &data)
{
    LOG_AS("Patch::loadMetadata");
    Reader reader = Reader(data);

    Header hdr; reader >> hdr;
    Columns columns = readColumns(hdr.dimensions[0], reader);

    return prepareMetadata(hdr, calcRealHeight(columns));
}

Block Patch::load(const IByteArray &data, const ColorPaletteTranslation &xlatTable, Flags flags)
{
    LOG_AS("Patch::load");
    Reader reader = Reader(data);

    Header hdr; reader >> hdr;
    Columns columns = readColumns(hdr.dimensions[0], reader);

    Metadata meta = prepareMetadata(hdr, calcRealHeight(columns));
    return compositeImage(reader, &xlatTable, columns, meta, flags);
}

Block Patch::load(const IByteArray &data, Metadata *loadedMetadata, Flags flags)
{
    LOG_AS("Patch::load");
    Reader reader = Reader(data);

    Header hdr; reader >> hdr;
    Columns columns = readColumns(hdr.dimensions[0], reader);

    Metadata meta = prepareMetadata(hdr, calcRealHeight(columns));
    if (loadedMetadata)
    {
        *loadedMetadata = meta;
    }
    return compositeImage(reader, nullptr /* no translation */, columns, meta, flags);
}

bool Patch::recognize(const IByteArray &data)
{
    try
    {
        // The format has no identification markings.
        // Instead we must rely on heuristic analyses of the column => post map.
        Reader from = Reader(data);
        Header hdr; from >> hdr;

        if (!hdr.dimensions[0] || !hdr.dimensions[1]) return false;

        for (int col = 0; col < hdr.dimensions[0]; ++col)
        {
            duint32 offset;
            from >> offset;
            if (offset >= from.source()->size()) return false;
        }

        // Validated.
        return true;
    }
    catch (const IByteArray::OffsetError &)
    {
        // Invalid!
        return false;
    }
}

} // namespace res
