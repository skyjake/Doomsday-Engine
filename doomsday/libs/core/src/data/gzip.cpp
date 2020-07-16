/** @file gzip.cpp  Process gzip data with zlib
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/gzip.h"

#include <zlib.h>

namespace de {

Block gDecompress(const Block &gzData)
{
    Block result(16384);

    z_stream stream = {};
    stream.next_in   = const_cast<Block::Byte *>(gzData.data());
    stream.avail_in  = uint(gzData.size());
    stream.next_out  = result.data();
    stream.avail_out = uint(result.size());

    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK)
    {
        return {};
    }

    int res = 0;
    do
    {
        res = inflate(&stream, 0);
        switch (res)
        {
        case Z_OK:
            // Allocate more output space, if needed.
            if (stream.avail_out == 0)
            {
                const auto oldSize = result.size();
                result.resize(result.size() * 2);
                stream.next_out = result.data() + oldSize;
                stream.avail_out = uint(result.size() - oldSize);
            }
            continue;

        case Z_STREAM_END:
            break;

        default:
            warning("Error decompressing gzip data: result=%i (%s)", res, stream.msg);
            inflateEnd(&stream);
            return {};
        }
    }
    while (res != Z_STREAM_END);
    DE_ASSERT(stream.avail_in == 0);

    // Truncate the extra space.
    result.resize(result.size() - stream.avail_out);

    inflateEnd(&stream);
    return result;
}

} // namespace de
