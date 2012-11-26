/**
 * @file patchcompositetexture.h Patch Composite Texture.
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

#ifndef LIBDENG_RESOURCE_PATCHCOMPOSITETEXTURE_H
#define LIBDENG_RESOURCE_PATCHCOMPOSITETEXTURE_H

#include "dd_types.h" // For lumpnum_t
#include <de/point.h>
#include <de/size.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lumpnum_t lumpNum;
    short offX; /// block origin (always UL), which has allready
    short offY; /// Accounted for the patch's internal origin
} texpatch_t;

#define TXDF_NODRAW         0x0001 ///< Not to be drawn.
#define TXDF_CUSTOM         0x0002 /**< Definition does not define a texture
                                        that originates from the current game. */

typedef struct {
    ddstring_t name; ///< Percent-encoded.

    /// Size of the texture in logical pixels.
    Size2Raw size;

    short flags;

    /// Index of this resource according to the logic of the original game's indexing algorithm.
    int origIndex;

    short patchCount;

    texpatch_t *patches; // [patchcount] drawn back to front into the cached texture.
} patchcompositetex_t;

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <QList>
#include <de/String>
#include <de/IReadable>
#include <de/Reader>
#include "patchname.h"

namespace de
{
    /**
     * A graphic (texture) composed of one or more patches.
     *
     * @ingroup resource
     */
    class PatchCompositeTexture : public IReadable
    {
    public:
        /**
         * Component Patch-format graphic.
         */
        struct Patch : public IReadable
        {
            /// Origin of the top left corner of the patch in texture space units.
            Point2Raw origin;

            /// Index of the patch in the PNAMEs set.
            int pnamesIndex;

            /// Number of the lump (file) contained the associated patch graphic
            /// ; otherwise @c -1 if not found.
            lumpnum_t lumpNum;

            explicit Patch(int xOrigin = 0, int yOrigin = 0, int _pnamesIndex = -1);

            /// Implements IReadable.
            void operator << (Reader& from);
        };
        typedef QList<Patch> Patches;

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

    public:
        /**
         * Construct a new PatchCompositeTexture.
         */
        explicit PatchCompositeTexture(String percentEncodedName = "", int width = 0,
            int height = 0, Flags _flags = 0);

        /// Returns the percent-endcoded symbolic name of the texture.
        String percentEncodedName() const;

        /// Returns the percent-endcoded symbolic name of the texture.
        String const &percentEncodedNameRef() const;

        /// Returns the logical dimensions of the texture.
        Size2Raw const &dimensions() const;

        /// Returns the usage trait flags for the texture.
        Flags const &flags() const;

        /// Returns the associated "original index" for the texture.
        int origIndex() const;

        /**
         * Perform validation of this texture using the supplied @a patchNames.
         * Patch lump numbers will be looked up and any discrepancies or issues
         * in the texture will be logged about at this time.
         */
        void validate(QList<PatchName> patchNames);

        /**
         * Provides access to the patches of the texture for efficent traversal.
         */
        Patches const &patches() const;

        /**
         * Deserializes a texture from the Doom id-tech1 archived format using
         * reader @a from.
         */
        static PatchCompositeTexture fromDoomFormat(Reader &from);

        /**
         * Deserializes a texture from the Strife id-tech1 archived format using
         * reader @a from.
         */
        static PatchCompositeTexture fromStrifeFormat(Reader &from);

        /**
         * Implements IReadable.
         * @see fromDoomFormat()
         */
        void operator << (Reader &from);

    private:
        /// Symbolic name of the texture (percent encoded).
        String name;

        /// Flags.
        Flags flags_;

        /// Logical dimensions of the texture in map coordinate space units.
        Size2Raw dimensions_;

        /// Set of patches (graphics) to be composited.
        Patches patches_;

        /// Index of this resource determined by the logic of the indexing algorithm
        /// used by the original game.
        int origIndex_;
    };

    Q_DECLARE_OPERATORS_FOR_FLAGS(PatchCompositeTexture::Flags)

} // namespace de
#endif // __cplusplus

#endif /// LIBDENG_RESOURCE_PATCHCOMPOSITETEXTURE_H
