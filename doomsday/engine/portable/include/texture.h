/**
 * @file texture.h
 * Abstract Texture component used to model a logical texture. @ingroup refresh
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

#ifndef LIBDENG_REFRESH_TEXTURE_H
#define LIBDENG_REFRESH_TEXTURE_H

#include "size.h"
#include "textures.h"

struct texturevariant_s;

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

/**
 * @defgroup textureFlags  Texture Flags.
 */
///@{
#define TXF_CUSTOM              0x1 ///< Texture does not originate from the current game.
///@}

/**
 * Texture
 *
 * Presents an abstract interface to all supported texture types so that
 * they may be managed transparently.
 */
struct texture_s; // The texture instance (opaque).
typedef struct texture_s Texture;

/**
 * Construct a new Texture.
 *
 * @param flags  @ref textureFlags
 * @param bindId  Unique identifier of the primary binding in the owning
 *    collection. Can be @c NOTEXTUREID in which case there is no binding
 *    for the resultant texture.
 * @param size Logical size of the texture. Components can be zero in which
 *    case their value will be inherited from the actual pixel size of the
 *    texture at load time.
 * @param userData  User data to associate with the resultant texture.
 */
Texture* Texture_NewWithSize(int flags, textureid_t bindId, const Size2Raw* size, void* userData);
Texture* Texture_New(int flags, textureid_t bindId, void* userData);

void Texture_Delete(Texture* tex);

textureid_t Texture_PrimaryBind(const Texture* tex);

void Texture_SetPrimaryBind(Texture* tex, textureid_t bindId);

/**
 * Attach new user data. If data is already present it will be replaced.
 * Ownership is given to Texture.
 *
 * @param userData  Data to be attached.
 */
void Texture_AttachUserData(Texture* tex, void* userData);

/**
 * Detach any associated user data. Ownership is relinquished to caller.
 * @return  Associated user data.
 */
void* Texture_DetachUserData(Texture* tex);

/// @return  Associated user data if any else @c NULL.
void* Texture_UserData(const Texture* tex);

/// Destroy all prepared variants owned by this texture.
void Texture_ClearVariants(Texture* tex);

/// @return  Number of variants for this texture.
uint Texture_VariantCount(const Texture* tex);

/**
 * Add a new prepared variant to the list of resources for this Texture.
 * Texture takes ownership of the variant.
 *
 * @param variant  Variant instance to add to the resource list.
 */
struct texturevariant_s* Texture_AddVariant(Texture* tex, struct texturevariant_s* variant);

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
    int (*callback)(struct texturevariant_s* instance, void* paramaters),
    void* paramaters);

/**
 * Attach new analysis data. If data is already present it will be replaced.
 * Ownership is given to the Texture.
 *
 * @param analysis  Identifier of the data being attached.
 * @param data  Data to be attached.
 */
void Texture_AttachAnalysis(Texture* tex, texture_analysisid_t analysis, void* data);

/**
 * Detach any associated analysis data. Ownership is relinquished to caller.
 *
 * @return  Associated data for the specified analysis identifier.
 */
void* Texture_DetachAnalysis(Texture* tex, texture_analysisid_t analysis);

/// @return  Associated data for the specified analysis identifier.
void* Texture_Analysis(const Texture* tex, texture_analysisid_t analysis);

/// @return  @c true iff the data associated with @a tex does not originate from the current game.
boolean Texture_IsCustom(const Texture* tex);

/// @return  @see textureFlags
int Texture_Flags(const Texture* tex);

/**
 * Change the value of the flags property.
 * @param flags  @see textureFlags
 */
void Texture_SetFlags(Texture* tex, int flags);

/// Retrieve logical dimensions (not necessarily the same as pixel dimensions).
const Size2* Texture_Size(const Texture* tex);

/**
 * Change logical pixel dimensions.
 * @param size  New size.
 */
void Texture_SetSize(Texture* tex, const Size2Raw* size);

/// @return  Logical width (not necessarily the same as pixel width).
int Texture_Width(const Texture* tex);

/**
 * Change logical width.
 * @param width  Width in logical pixels.
 */
void Texture_SetWidth(Texture* tex, int width);

/// @return  Logical height (not necessarily the same as pixel height).
int Texture_Height(const Texture* tex);

/**
 * Change logical height.
 * @param height  Height in logical pixels.
 */
void Texture_SetHeight(Texture* tex, int height);

#endif /// LIBDENG_REFRESH_TEXTURE_H
