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
typedef QList<Posts> Columns;

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
        columns.push_back(Posts());
        Posts &posts = columns.back();

        // Read all posts.
        while(readNextPost(post, reader))
        {
            posts.push_back(post);

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

static Columns readColumns(IByteArray const &data, Patch::Header &hdr)
{
    // Read the header.
    Reader from = Reader(data);
    from >> hdr;

    // Column offsets begin immediately following the header.
    return readPosts(readColumnOffsets(hdr.dimensions.width(), from), from);
}

/**
 * Process @a columns to calculate the "real" pixel height of the image.
 */
#ifdef DENG_DEBUG
static int calcRealHeight(Columns const &columns)
{
    QRect geom(QPoint(0, 0), QSize(1, 0));

    DENG2_FOR_EACH_CONST(Columns, colIt, columns)
    {
        int tallTop = -1; // Keep track of pos (clipping).

        DENG2_FOR_EACH_CONST(Posts, postIt, *colIt)
        {
            Post const &post = *postIt;

            // Does this post extend the previous one (a so-called "tall-patch").
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
#endif

static Block load(IByteArray const &data, IByteArray const *xlatTable, bool maskZero)
{
    // Prepare the column => post map.
    Patch::Header hdr;
    Columns columns = readColumns(data, hdr);

#ifdef DENG_DEBUG
    // Is the declared height of the image equal to the "real" height of the
    // composited pixel posts?
    int const realHeight = calcRealHeight(columns);
    if(realHeight != hdr.dimensions.height())
    {
        int postCount = 0;
        DENG2_FOR_EACH_CONST(Columns, i, columns)
        {
            postCount += i->count();
        }

        LOG_INFO("Inequal heights, declared: %i != real: %i (%i %s).")
            << hdr.dimensions.height() << realHeight
            << postCount << (postCount == 1? "post" : "posts");
    }
#endif

    // Determine the dimensions of the output buffer.
    int const       w = hdr.dimensions.width();
    int const       h = hdr.dimensions.height();
    size_t const pels = w * h;

    Block result = QByteArray(2 * pels, 0);

    // Map the destination pixel color channels in the output buffer.
    dbyte *destTop      = result.data();
    dbyte *destAlphaTop = result.data() + pels;

    // Composite the patch into the output buffer.
    Reader reader = Reader(data);

    int x = 0;
    for(int col = 0; col < w; ++x, ++col, destTop++, destAlphaTop++)
    {
        int tallTop = -1; // Keep track of pos (clipping).

        // Step through the posts in a column.
        DENG2_FOR_EACH_CONST(Posts, i, columns[col])
        {
            Post const &post = *i;

            // Does this post extend the previous one (a so-called "tall-patch").
            if(post.topOffset <= tallTop)
                tallTop += post.topOffset;
            else
                tallTop = post.topOffset;

            // Skip invalid posts.
            if(post.length <= 0) continue;

            // Find the start of the pixel data for the post.
            reader.setOffset(post.firstPixel);

            int y            = tallTop;
            dbyte *dest      = destTop      + y * w;
            dbyte *destAlpha = destAlphaTop + y * w;

            // Composite pixels from the post to the output buffer.
            int length = post.length;
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
                    *dest = palIdx;

                if(maskZero)
                    *destAlpha = (palIdx? 0xff : 0);
                else
                    *destAlpha = 0xff;

                // One row down.
                dest      += w;
                destAlpha += w;
            }
        }
    }

    return result;
}

} // internal

Block Patch::load(IByteArray const &data, IByteArray const &xlatTable, bool maskZero)
{
    LOG_AS("Patch::load");
    return internal::load(data, &xlatTable, maskZero);
}

Block Patch::load(IByteArray const &data, bool maskZero)
{
    LOG_AS("Patch::load");
    return internal::load(data, 0/* no translation */, maskZero);
}

bool Patch::recognize(IByteArray const &data)
{
    try
    {
        // The format has no identification markings.
        // Instead we must rely on heuristic analyses of the column => post map.
        Reader from = Reader(data);
        Header hdr; from >> hdr;

        if(hdr.dimensions.isEmpty()) return false;

        // Check the column offset map.
        for(int col = 0; col < hdr.dimensions.width(); ++col)
        {
            dint32 offset;
            from >> offset;
            if(offset < 0 || (unsigned) offset >= from.source()->size()) return false;

            /// @todo Check post run lengths too?
        }

        // Validated.
        return true;
    }
    catch(IByteArray::OffsetError const &)
    {} // Ignore this error.
    return false;
}

void Patch::Header::operator << (Reader &from)
{
    dint16 width, height;
    from >> width >> height;
    dimensions.setWidth(width);
    dimensions.setHeight(height);

    dint16 xOrigin, yOrigin;
    from >> xOrigin >> yOrigin;
    origin.setX(xOrigin);
    origin.setY(yOrigin);
}

} // namespace de
