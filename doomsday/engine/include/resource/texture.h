/** @file texture.h Logical Texture.
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

#ifndef LIBDENG_RESOURCE_TEXTURE_H
#define LIBDENG_RESOURCE_TEXTURE_H

#include "texturevariant.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup resource
///@{
typedef enum {
    TEXTURE_ANALYSIS_FIRST = 0,
    TA_COLORPALETTE = TEXTURE_ANALYSIS_FIRST,
    TA_SPRITE_AUTOLIGHT,
    TA_COLOR,                  ///< Average.
    TA_COLOR_AMPLIFIED,        ///< Average amplified (max component ==1).
    TA_ALPHA,                  ///< Average.
    TA_LINE_TOP_COLOR,         ///< Average.
    TA_LINE_BOTTOM_COLOR,      ///< Average.
    TEXTURE_ANALYSIS_COUNT
} texture_analysisid_t;

#define VALID_TEXTURE_ANALYSISID(id) (\
    (id) >= TEXTURE_ANALYSIS_FIRST && (id) < TEXTURE_ANALYSIS_COUNT)
///@}

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <list>
#include <QFlag>
#include <QSize>

namespace de {

class TextureManifest;

/**
 * Logical texture object.
 * @ingroup resource
 */
class Texture
{
public:
    /**
     * Classification flags
     */
    enum Flag
    {
        /// Texture is "custom" (i.e., not an original game resource).
        Custom = 0x1
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    typedef std::list<TextureVariant *> Variants;

public:
    /**
     * @param manifest  Manifest derived to yield the texture.
     * @param flags     Texture classification flags.
     * @param userData  User data to associate with the resultant texture.
     */
    Texture(TextureManifest &manifest, Flags flags = 0, void *userData = 0);

    /**
     * @param manifest  Manifest derived to yield the Texture.
     * @param dimensions Logical dimensions of the texture in map space
     *                   coordinates. If width=0 and height=0, their value
     *                  will be inferred from the actual pixel dimensions
     *                  of the image resource at load time.
     * @param flags     Texture classification flags.
     * @param userData  User data to associate with the resultant texture.
     */
    Texture(TextureManifest &manifest, QSize const &dimensions,
            Flags flags = 0, void *userData = 0);

    ~Texture();

    /// @return  @c true iff this texture instance is flagged as "custom".
    bool isCustom() const;

    /// Change the "custom" status of this texture instance.
    void flagCustom(bool yes);

    /**
     * Returns the TextureManifest derived to yield the texture.
     */
    TextureManifest &manifest() const;

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

    /**
     * Add a new prepared variant to the list of resources for this Texture.
     * Texture takes ownership of the variant.
     *
     * @param variant  Variant instance to add to the resource list.
     */
    TextureVariant &addVariant(TextureVariant &variant);

    /// @return  Number of variants for the texture.
    uint variantCount() const;

    /// Destroy all analyses for the texture.
    void clearAnalyses();

    /// Destroy all prepared variants for the texture.
    void clearVariants();

    /**
     * Retrieve the value of an identified @a analysis data pointer.
     * @return  Associated data pointer value.
     **/
    void *analysisDataPointer(texture_analysisid_t analysis) const;

    /**
     * Set the value of an identified @a analysis data pointer. Ownership of
     * the data is not given to this instance.
     *
     * @note If already set the old value will be replaced (so if it points
     *       to some dynamically constructed data/resource it is the caller's
     *       responsibility to release it beforehand).
     *
     * @param analysis  Identifier of the data being attached.
     * @param data  Data to be attached.
     */
    void setAnalysisDataPointer(texture_analysisid_t analysis, void *data);

    /// @return  Logical width (not necessarily the same as pixel width).
    int width() const;

    /// @return  Logical height (not necessarily the same as pixel height).
    int height() const;

    /// Retrieve logical dimensions (not necessarily the same as pixel dimensions).
    QSize const &dimensions() const;

    /**
     * Change logical width.
     * @param width  Width in logical pixels.
     */
    void setWidth(int width);

    /**
     * Change logical height.
     * @param height  Height in logical pixels.
     */
    void setHeight(int height);

    /**
     * Change logical pixel dimensions.
     * @param dimensions  New dimensions.
     */
    void setDimensions(QSize const &dimensions);

    /**
     * Provides access to the list of variant textures for efficent traversals.
     */
    Variants const &variantList() const;

private:
    struct Instance;
    Instance *d;
};

} // namespace de

extern "C" {
#endif // __cplusplus

/**
 * C wrapper API:
 */

struct texture_s; // The texture instance (opaque).
typedef struct texture_s Texture;

void* Texture_UserDataPointer(Texture const *tex);
void Texture_SetUserDataPointer(Texture *tex, void *userData);

void Texture_ClearVariants(Texture *tex);
uint Texture_VariantCount(Texture const *tex);
struct texturevariant_s *Texture_AddVariant(Texture *tex, struct texturevariant_s *variant);

void* Texture_AnalysisDataPointer(Texture const *tex, texture_analysisid_t analysis);
void Texture_SetAnalysisDataPointer(Texture *tex, texture_analysisid_t analysis, void *data);

boolean Texture_IsCustom(Texture const *tex);
void Texture_FlagCustom(Texture *tex, boolean yes);

int Texture_Width(Texture const *tex);
int Texture_Height(Texture const *tex);
void Texture_SetWidth(Texture *tex, int width);
void Texture_SetHeight(Texture *tex, int height);

/**
 * Iterate over all derived TextureVariants, making a callback for each.
 * Iteration ends once all variants have been visited, or immediately upon
 * a callback returning non-zero.
 *
 * @param tex       Texture instance.
 * @param callback  Callback to make for each processed variant.
 * @param parameters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Texture_IterateVariants(Texture *tex,
    int (*callback)(struct texturevariant_s *instance, void *parameters), void *parameters);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_RESOURCE_TEXTURE_H
