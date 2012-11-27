/**
 * @file compositetexture.h Composite Texture.
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_RESOURCE_COMPOSITETEXTURE_H
#define LIBDENG_RESOURCE_COMPOSITETEXTURE_H

#ifdef __cplusplus

#include "dd_types.h" // For lumpnum_t
#include <QList>
#include <de/String>
#include <de/Reader>
#include <de/point.h>
#include <de/size.h>
#include "patchname.h"

namespace de
{
    /**
     * A logical texture composite of one or more @em component images.
     *
     * The component images are sorted according to the order in which they
     * should be composited, from bottom-most to top-most.
     *
     * @ingroup resource
     */
    class CompositeTexture
    {
    public:
        /**
         * Flags denoting usage traits.
         */
        enum Flag
        {
            NoDraw = 0x1, ///< Texture is not to be drawn.
            Custom = 0x2  /**< The texture does not originate from a definition
                               of the current game. */
        };
        Q_DECLARE_FLAGS(Flags, Flag)

        /**
         * Archived format variants.
         */
        enum ArchiveFormat
        {
            /// Original format used by most id-tech 1 games.
            DoomFormat,

            /// StrifeFormat differs only slightly from DoomFormat (ommits some unused values).
            StrifeFormat
        };

        /**
         * Component image.
         */
        struct Component
        {
        protected:
            explicit Component(int xOrigin = 0, int yOrigin = 0);

        public:
            /// Origin of the top left corner of the component (in texture space units).
            Point2Raw const &origin() const;

            /// X-axis origin of the top left corner of the component (in texture space units).
            inline int xOrigin() const {
                return origin().x;
            }

            /// Y-axis origin of the top left corner of the component (in texture space units).
            inline int yOrigin() const {
                return origin().y;
            }

            /// Returns the number of the lump (file) containing the associated
            /// image; otherwise @c -1 (not found).
            lumpnum_t lumpNum() const;

            friend class CompositeTexture;

        private:
            /// Origin of the top left corner in the texture coordinate space.
            Point2Raw origin_;

            /// Index of the lump containing the associated image.
            lumpnum_t lumpNum_;
        };
        typedef QList<Component> Components;

    public:
        /**
         * Construct a default composite texture.
         */
        explicit CompositeTexture(String percentEncodedName = "", int width = 0,
            int height = 0, Flags _flags = 0);

        /**
         * Construct a composite texture by deserializing an archived id-tech 1
         * format definition from the Reader. Lump numbers will be looked up using
         * @a patchNames and any discrepancies or issues in the texture will be
         * logged about at this time.
         *
         * @param reader        Reader.
         * @param patchNames    List of component image names.
         * @param format        Format of the archived data.
         *
         * @return  The deserialized composite texture. Caller gets ownership.
         */
        static CompositeTexture *constructFrom(Reader &reader, QList<PatchName> patchNames,
                                               ArchiveFormat format = DoomFormat);

        /// Returns the percent-endcoded symbolic name of the texture.
        String percentEncodedName() const;

        /// Returns the percent-endcoded symbolic name of the texture.
        String const &percentEncodedNameRef() const;

        /// Returns the logical dimensions of the texture (in map space units).
        Size2Raw const &dimensions() const;

        /// Returns the logical width of the texture (in map space units).
        inline int width() const {
            return dimensions().width;
        }

        /// Returns the logical width of the texture (in map space units).
        inline int height() const {
            return dimensions().height;
        }

        /// Returns the associated "original index" for the texture.
        int origIndex() const;

        /// Change the "original index" value for the texture.
        void setOrigIndex(int newIndex);

        /// Returns the total number of component images.
        inline int componentCount() const {
            return components().count();
        }

        /**
         * Provides access to the component images of the texture for efficent traversal.
         */
        Components const &components() const;

        /**
         * Provides access to the usage flags for the texture for efficent manipulation.
         */
        Flags &flags();

    private:
        /// Symbolic name of the texture (percent encoded).
        String name;

        /// Flags denoting usage traits.
        Flags flags_;

        /// Logical dimensions of the texture in map coordinate space units.
        Size2Raw dimensions_;

        /// Index of this resource determined by the logic of the indexing algorithm
        /// used by the original game.
        int origIndex_;

        /// Set of component images to be composited.
        Components components_;
    };

    Q_DECLARE_OPERATORS_FOR_FLAGS(CompositeTexture::Flags)

} // namespace de
#endif // __cplusplus

#endif /// LIBDENG_RESOURCE_COMPOSITETEXTURE_H
