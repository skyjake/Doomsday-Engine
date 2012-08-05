/**
 * @file texture.h
 * Abstract Texture component used to model a logical texture. @ingroup resource
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include "size.h"
#include "textures.h"
#include "texturevariant.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
namespace de {

#include <list>

/**
 * Texture
 *
 * Presents an abstract interface to all supported texture types so that
 * they may be managed transparently.
 */
class Texture
{
public:
    enum Flag
    {
        /// Texture is "custom" (i.e., not an original game resource).
        Custom = 0x1
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    typedef std::list<de::TextureVariant*> Variants;

public:
    /**
     * @param bindId  Unique identifier of the primary binding in the owning
     *    collection. Can be @c NOTEXTUREID in which case there is no binding
     *    for the resultant texture.
     * @param size Logical size of the texture. Components can be zero in which
     *    case their value will be inherited from the actual pixel size of the
     *    image at load time.
     * @param userData  User data to associate with the resultant texture.
     */
    Texture(textureid_t bindId, void* userData=NULL);
    Texture(textureid_t bindId, const Size2Raw& size, void* userData=NULL);

    ~Texture();

    textureid_t primaryBind() const { return primaryBindId; }

    void setPrimaryBind(textureid_t bindId);

    /**
     * Retrieve the value of the associated user data pointer.
     * @return  Associated data pointer value.
     **/
    void* userDataPointer() const;

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
    void setUserDataPointer(void* userData);

    /// Destroy all prepared variants owned by this texture.
    void clearVariants();

    /// @return  Number of variants for this texture.
    uint variantCount() const;

    /**
     * Add a new prepared variant to the list of resources for this Texture.
     * Texture takes ownership of the variant.
     *
     * @param variant  Variant instance to add to the resource list.
     */
    de::TextureVariant& addVariant(de::TextureVariant& variant);

    /**
     * Retrieve the value of an identified @a analysis data pointer.
     * @return  Associated data pointer value.
     **/
    void* analysis(texture_analysisid_t analysis) const;

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
    void setAnalysisDataPointer(texture_analysisid_t analysis, void* data);

    /// @return  @c true iff this texture instance is flagged as "custom".
    bool isCustom() const { return !!(flags & Custom); }

    void flagCustom(bool yes);

    /// Retrieve logical dimensions (not necessarily the same as pixel dimensions).
    const Size2Raw& size() const { return dimensions; }

    /**
     * Change logical pixel dimensions.
     * @param size  New size.
     */
    void setSize(const Size2Raw& size);

    /// @return  Logical width (not necessarily the same as pixel width).
    int width() const;

    /**
     * Change logical width.
     * @param width  Width in logical pixels.
     */
    void setWidth(int width);

    /// @return  Logical height (not necessarily the same as pixel height).
    int height() const;

    /**
     * Change logical height.
     * @param height  Height in logical pixels.
     */
    void setHeight(int height);

    /**
     * Provides access to the list of variant textures for efficent traversals.
     */
    const Variants& variantList() const { return variants; }

private:
    Flags flags;

    /// Unique identifier of the primary binding in the owning collection.
    textureid_t primaryBindId;

    /// List of variants (e.g., color translations).
    Variants variants;

    /// User data associated with this texture.
    void* userData;

    /// Dimensions in logical pixels (not necessarily the same as pixel dimensions).
    Size2Raw dimensions;

    /// Table of analyses object ptrs, used for various purposes depending
    /// on the variant specification.
    void* analyses[TEXTURE_ANALYSIS_COUNT];
};

} // namespace de

extern "C" {
#endif // __cplusplus

/**
 * C wrapper API:
 */

struct texture_s; // The texture instance (opaque).
typedef struct texture_s Texture;

Texture* Texture_NewWithSize(textureid_t bindId, const Size2Raw* size, void* userData);
Texture* Texture_New(textureid_t bindId, void* userData);
void Texture_Delete(Texture* tex);

textureid_t Texture_PrimaryBind(const Texture* tex);
void Texture_SetPrimaryBind(Texture* tex, textureid_t bindId);

void* Texture_UserDataPointer(const Texture* tex);
void Texture_SetUserDataPointer(Texture* tex, void* userData);

void Texture_ClearVariants(Texture* tex);
uint Texture_VariantCount(const Texture* tex);
struct texturevariant_s* Texture_AddVariant(Texture* tex, struct texturevariant_s* variant);

void* Texture_AnalysisDataPointer(const Texture* tex, texture_analysisid_t analysis);
void Texture_SetAnalysisDataPointer(Texture* tex, texture_analysisid_t analysis, void* data);

boolean Texture_IsCustom(const Texture* tex);
void Texture_FlagCustom(Texture* tex, boolean yes);

void Texture_SetSize(Texture* tex, const Size2Raw* size);
int Texture_Width(const Texture* tex);
void Texture_SetWidth(Texture* tex, int width);
int Texture_Height(const Texture* tex);
void Texture_SetHeight(Texture* tex, int height);

/**
 * Iterate over all derived TextureVariants, making a callback for each.
 * Iteration ends once all variants have been visited, or immediately upon
 * a callback returning non-zero.
 *
 * @param callback  Callback to make for each processed variant.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Texture_IterateVariants(Texture* tex,
    int (*callback)(struct texturevariant_s* instance, void* parameters), void* parameters);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_RESOURCE_TEXTURE_H
