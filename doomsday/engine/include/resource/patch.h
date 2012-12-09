/** @file patch.h Patch Image Format.
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

#ifndef LIBDENG_RESOURCE_PATCH_H
#define LIBDENG_RESOURCE_PATCH_H

#include <QSize>
#include <QPoint>
#include <de/Block>
#include <de/IByteArray>
#include <de/IReadable>
#include <de/Reader>

namespace de {

    /**
     * @em Patch is a raster graphic image in the id Tech 1 picture format (Doom).
     * @ingroup resource
     *
     * @see http://doomwiki.org/wiki/Picture_format
     */
    class Patch
    {
    public:
        /**
         * Serialized format header.
         */
        struct Header : public IReadable
        {
            /// Dimensions of the patch in pixels.
            QSize dimensions;

            /// World origin offset (top left) in map coordinate space units.
            /// Used with sprite frames to orient the patch relatively to a Mobj.
            QPoint origin;

            /// Implements IReadable.
            void operator << (Reader &from);
        };

    public:
        /**
         * @param data  Ptr to the data buffer to draw to the dst buffer.
         * @param xlatTable  If not @c NULL, use this translation table when
         *                   compositing final color palette indices.
         * @param maskZero  Used with sky textures.
         */
        static Block load(IByteArray const &data, bool maskZero = false);

        static Block load(IByteArray const &data, IByteArray const &xlatTable,
                          bool maskZero = false);

        /**
         * Determines whether @a data looks like it can be interpreted as a Patch.
         *
         * @param data  Data to check.
         *
         * @return  @c true if the data looks like a patch; otherwise @c false.
         */
        static bool recognize(IByteArray const &data);
    };

} // namespace de

#endif /// LIBDENG_RESOURCE_PATCH_H
