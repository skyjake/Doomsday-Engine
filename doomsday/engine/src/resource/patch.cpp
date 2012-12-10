/** @file patch.cpp Patch Image Format.
 *
 * @author Copyright &copy; 1999-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include <QList>
#include <QRect>
#include <de/libdeng2.h>
#include <de/Log>

#include "resource/colorpalettes.h"
#include "resource/patch.h"

namespace de {

namespace internal {

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
typedef QList<Post> Posts;
typedef Posts Column;
typedef QList<Column> Columns;

/// Offsets to columns from the start of the source data.
typedef QList<dint32> ColumnOffsets;

/**
 * Attempt to read another @a post from the @a reader.
 * @return  @c true if another post was read; otherwise @c false.
 */
static bool readNextPost(Post &post, Reader &reader)
{
    static int const END_OF_POSTS = 0xff;

    // Any more?
    reader.mark();
    dbyte nextByte; reader >> nextByte;
    reader.rewind();
    if(nextByte == END_OF_POSTS) return false;

    // Another post begins.
    reader >> post;
    return true;
}

/**
 * Visit each of @a offsets producing a column => post map.
 */
static Columns readPosts(ColumnOffsets const &offsets, Reader &reader)
{
    Columns columns;
#ifdef DENG2_QT_4_7_OR_NEWER
    columns.reserve(offsets.size());
#endif

    Post post;
    DENG2_FOR_EACH_CONST(ColumnOffsets, i, offsets)
    {
        reader.setOffset(*i);

        // A new column begins.
        columns.push_back(Column());
        Column &column = columns.back();

        // Read all posts.
        while(readNextPost(post, reader))
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
#ifdef DENG2_QT_4_7_OR_NEWER
    offsets.reserve(width);
#endif

    for(int col = 0; col < width; ++col)
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
static int calcRealHeight(Columns const &columns)
{
    QRect geom(QPoint(0, 0), QSize(1, 0));

    DENG2_FOR_EACH_CONST(Columns, colIt, columns)
    {
        int tallTop = -1; // Keep track of pos (clipping).

        DENG2_FOR_EACH_CONST(Posts, postIt, *colIt)
        {
            Post const &post = *postIt;

            // Does this post extend the previous one? (a so-called "tall-patch").
            if(post.topOffset <= tallTop)
                tallTop += post.topOffset;
            else
                tallTop = post.topOffset;

            // Skip invalid posts.
            if(post.length <= 0) continue;

            // Unite the geometry of the post.
            geom |= QRect(QPoint(0, tallTop), QSize(1, post.length));
        }
    }

    return geom.height();
}

static Patch::Metadata prepareMetadata(Header const &hdr, int realHeight)
{
    Patch::Metadata meta;
    meta.dimensions         = QSize(hdr.dimensions[0], realHeight);
    meta.logicalDimensions  = QSize(hdr.dimensions[0], hdr.dimensions[1]);
    meta.origin             = QPoint(hdr.origin[0], hdr.origin[1]);
    return meta;
}

static Block load(IByteArray const &data, IByteArray const *xlatTable,
                  bool maskZero, bool clipToLogicalDimensions)
{
    Reader reader = Reader(data);

    // Read the header.
    Header hdr; reader >> hdr;

    // Read and prepare the column => post map.
    Columns columns = readColumns(hdr.dimensions[0], reader);

    // Prepare metadata.
    Patch::Metadata meta = prepareMetadata(hdr, calcRealHeight(columns));

#ifdef DENG_DEBUG
    // Is the "logical" height of the image equal to the actual height of the
    // composited pixel posts?
    if(meta.logicalDimensions.height() != meta.dimensions.height())
    {
        int postCount = 0;
        DENG2_FOR_EACH_CONST(Columns, i, columns)
        {
            postCount += i->count();
        }

        LOG_INFO("Inequal heights, logical: %i != actual: %i (%i %s).")
            << meta.logicalDimensions.height() << meta.dimensions.height()
            << postCount << (postCount == 1? "post" : "posts");
    }
#endif

    // Determine the dimensions of the output buffer.
    QSize const &dimensions = clipToLogicalDimensions? meta.logicalDimensions : meta.dimensions;

    int const       w = dimensions.width();
    int const       h = dimensions.height();
    size_t const pels = w * h;

    Block output = QByteArray(2 * pels, 0);

    // Map the pixel color channels in the output buffer.
    dbyte *top      = output.data();
    dbyte *topAlpha = output.data() + pels;

    // Composite the patch into the output buffer.
    DENG2_FOR_EACH_CONST(Columns, colIt, columns)
    {
        int tallTop = -1; // Keep track of pos (clipping).

        DENG2_FOR_EACH_CONST(Posts, postIt, *colIt)
        {
            Post const &post = *postIt;

            // Does this post extend the previous one? (a so-called "tall-patch").
            if(post.topOffset <= tallTop)
                tallTop += post.topOffset;
            else
                tallTop = post.topOffset;

            // Skip invalid posts.
            if(post.length <= 0) continue;

            // Determine the destination height range.
            int y = tallTop;
            int length = post.length;

            // Clamp height range within bounds.
            if(y + length > h)
                length = h - y;

            int offset = 0;
            if(y < 0)
            {
                offset = MIN_OF(-y, length);
                y = 0;
            }

            length = MAX_OF(0, length - offset);

            // Skip empty ranges.
            if(!length) continue;

            // Find the start of the pixel data for the post.
            reader.setOffset(post.firstPixel + offset);

            dbyte *out      = top      + tallTop * w;
            dbyte *outAlpha = topAlpha + tallTop * w;

            // Composite pixels from the post to the output buffer.
            while(length--)
            {
                // Read the next palette index.
                dbyte palIdx;
                reader >> palIdx;

                // Is palette index translation in effect?
                if(xlatTable)
                {
                    xlatTable->get(palIdx, &palIdx, 1);
                }

                if(!maskZero || palIdx)
                    *out = palIdx;

                if(maskZero)
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

} // internal

Patch::Metadata Patch::loadMetadata(IByteArray const &data)
{
    LOG_AS("Patch::loadMetadata");
    Reader reader = Reader(data);

    internal::Header hdr; reader >> hdr;
    internal::Columns columns = internal::readColumns(hdr.dimensions[0], reader);
    return internal::prepareMetadata(hdr, calcRealHeight(columns));
}

Block Patch::load(IByteArray const &data, IByteArray const &xlatTable,
    bool maskZero, bool clipToLogicalDimensions)
{
    LOG_AS("Patch::load");
    return internal::load(data, &xlatTable, maskZero, clipToLogicalDimensions);
}

Block Patch::load(IByteArray const &data, bool maskZero, bool clipToLogicalDimensions)
{
    LOG_AS("Patch::load");
    return internal::load(data, 0/* no translation */, maskZero, clipToLogicalDimensions);
}

bool Patch::recognize(IByteArray const &data)
{
    try
    {
        // The format has no identification markings.
        // Instead we must rely on heuristic analyses of the column => post map.
        Reader from = Reader(data);
        internal::Header hdr; from >> hdr;

        if(!hdr.dimensions[0] || !hdr.dimensions[1]) return false;

        for(int col = 0; col < hdr.dimensions[0]; ++col)
        {
            dint32 offset;
            from >> offset;
            if(offset < 0 || (unsigned) offset >= from.source()->size()) return false;
        }

        // Validated.
        return true;
    }
    catch(IByteArray::OffsetError const &)
    {} // Ignore this error.
    return false;
}

} // namespace de
