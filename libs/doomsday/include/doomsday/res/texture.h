/** @file texture.h  Logical texture resource.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_RESOURCE_TEXTURE_H
#define LIBDOOMSDAY_RESOURCE_TEXTURE_H

#include "../libdoomsday.h"

#include <de/error.h>
#include <de/observers.h>
#include <de/vector.h>
#include <de/list.h>

namespace res {

using namespace de;

class TextureManifest;

/**
 * Logical texture resource.
 *
 * @ingroup resource
 */
class LIBDOOMSDAY_PUBLIC Texture
{
public:
    DE_DEFINE_AUDIENCE(Deletion,         void textureBeingDeleted      (const Texture &))
    DE_DEFINE_AUDIENCE(DimensionsChange, void textureDimensionsChanged (const Texture &))

    /**
     * Classification/processing flags.
     */
    enum Flag
    {
        /// Texture is not to be drawn.
        NoDraw              = 0x1,

        /// Texture is "custom" (i.e., not an original game resource).
        Custom              = 0x2,

        /// Apply the monochrome filter to the processed image.
        Monochrome          = 0x4,

        /// Apply the upscaleAndSharpen filter to the processed image.
        UpscaleAndSharpen   = 0x8
    };

    /**
     * Image analysis identifiers.
     */
    enum AnalysisId
    {
        /// Color palette info.
        ColorPaletteAnalysis,

        /// Brightest point for automatic light sources.
        BrightPointAnalysis,

        /// Average color.
        AverageColorAnalysis,

        /// Average color amplified (max component ==1).
        AverageColorAmplifiedAnalysis,

        /// Average alpha.
        AverageAlphaAnalysis,

        /// Average top line color.
        AverageTopColorAnalysis,

        /// Average bottom line color.
        AverageBottomColorAnalysis
    };

public:
    /**
     * @param manifest  Manifest derived to yield the texture.
     */
    Texture(TextureManifest &manifest);

    virtual ~Texture();

    /**
     * Returns the TextureManifest derived to yield the texture.
     */
    TextureManifest &manifest() const;

    /**
     * Returns a brief textual description/overview of the texture.
     *
     * @return Human-friendly description/overview of the texture.
     */
    virtual String description() const;

    /**
     * Returns the world dimensions of the texture, in map coordinate space
     * units. The DimensionsChange audience is notified whenever dimensions
     * are changed.
     */
    const Vec2ui &dimensions() const;

    /**
     * Convenient accessor method for returning the X axis size (width) of
     * the world dimensions for the texture, in map coordinate space units.
     *
     * @see dimensions()
     */
    inline int width() const { return int(dimensions().x); }

    /**
     * Convenient accessor method for returning the X axis size (height) of
     * the world dimensions for the texture, in map coordinate space units.
     *
     * @see dimensions()
     */
    inline int height() const { return int(dimensions().y); }

    /**
     * Change the world dimensions of the texture.
     * @param newDimensions  New dimensions in map coordinate space units.
     *
     * @todo Update any Materials (and thus Surfaces) which reference this.
     */
    void setDimensions(const Vec2ui &newDimensions);

    /**
     * Change the world width of the texture.
     * @param newWidth  New width in map coordinate space units.
     *
     * @todo Update any Materials (and thus Surfaces) which reference this.
     */
    void setWidth(duint newWidth);

    /**
     * Change the world height of the texture.
     * @param newHeight  New height in map coordinate space units.
     *
     * @todo Update any Materials (and thus Surfaces) which reference this.
     */
    void setHeight(duint newHeight);

    /**
     * Returns the world origin offset of texture in map coordinate space units.
     */
    const Vec2i &origin() const;

    /**
     * Change the world origin offset of the texture.
     * @param newOrigin  New origin in map coordinate space units.
     */
    void setOrigin(const Vec2i &newOrigin);

    /**
     * Returns @c true if the texture is flagged @a flagsToTest.
     */
    inline bool isFlagged(Flags flagsToTest) const { return !!(flags() & flagsToTest); }

    /**
     * Returns the flags for the texture.
     */
    Flags flags() const;

    /**
     * Change the texture's flags.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(Flags flagsToChange, FlagOp operation = SetFlags);

    /**
     * Release prepared GL-textures for identified variants.
     *
     * @param spec  If non-zero release only for variants derived with this spec.
     */
    virtual void release();

    /**
     * Destroys all analyses for the texture.
     */
    void clearAnalyses();

    /**
     * Retrieve the value of an identified @a analysisId data pointer.
     * @return  Associated data pointer value.
     */
    void *analysisDataPointer(AnalysisId analysisId) const;

    /**
     * Set the value of an identified @a analysisId data pointer. Ownership of
     * the data is not given to this instance.
     *
     * @note If already set the old value will be replaced (so if it points
     *       to some dynamically constructed data/resource it is the caller's
     *       responsibility to release it beforehand).
     *
     * @param analysisId  Identifier of the data being attached.
     * @param data  Data to be attached.
     */
    void setAnalysisDataPointer(AnalysisId analysisId, void *data);

    /**
     * Retrieve the value of the associated user data pointer.
     * @return  Associated data pointer value.
     */
    void *userDataPointer() const;

    /**
     * Set the user data pointer value. Ownership of the data is not given to
     * this instance.
     *
     * @note If already set the old value will be replaced (so if it points
     *       to some dynamically constructed data/resource it is the caller's
     *       responsibility to release it beforehand).
     *
     * @param userData  User data pointer value.
     */
    void setUserDataPointer(void *userData);

public:
    /// Register the console commands, variables, etc..., of this module.
    static void consoleRegister();

private:
    DE_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_TEXTURE_H
