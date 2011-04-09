/**\file texture.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBDENG_GL_TEXTURE_H
#define LIBDENG_GL_TEXTURE_H

struct texturevariant_s;

typedef enum {
    TEXTURE_ANALYSIS_FIRST = 0,
    TA_SPRITE_AUTOLIGHT = TEXTURE_ANALYSIS_FIRST,
    TA_MAP_AMBIENTLIGHT,
    TA_SKY_SPHEREFADEOUT,
    TEXTURE_ANALYSIS_COUNT
} texture_analysisid_t;

#define VALID_TEXTURE_ANALYSISID(id) (\
    (id) >= TEXTURE_ANALYSIS_FIRST && (id) < TEXTURE_ANALYSIS_COUNT)

/**
 * Texture
 *
 * Presents an abstract interface to all supported texture types so that
 * they may be managed transparently.
 */
typedef struct texture_s {
    /// Unique identifier.
    textureid_t _id;

    /// Type specific index (e.g., if _texNamespace=TN_FLATS this is a flat index).
    int _index;

    /// Dimensions in logical pixels (not necessarily the same as pixel dimensions).
    int _width, _height;

    /// Unique Texture Namespace Identifier.
    /// \todo make external.
    texturenamespaceid_t _texNamespace;

    /// List of variants (e.g., color translations).
    struct texture_variantlist_node_s* _variants;

    /// Table of analyses object ptrs, used for various purposes depending
    /// on the variant specification.
    void* _analyses[TEXTURE_ANALYSIS_COUNT];

    /// Symbolic name.
    char _name[9];
} texture_t;

texture_t* Texture_Construct(textureid_t id, const char name[9],
    texturenamespaceid_t texNamespace, int index);
texture_t* Texture_Construct2(textureid_t id, const char name[9],
    texturenamespaceid_t texNamespace, int index, int width, int height);

void Texture_Destruct(texture_t* tex);

/**
 * Add a new prepared variant to the list of resources for this Texture.
 * Texture takes ownership of the variant.
 *
 * @param variant  Variant instance to add to the resource list.
 */
struct texturevariant_s* Texture_AddVariant(texture_t* tex, struct texturevariant_s* variant);

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
int Texture_IterateVariants(texture_t* tex,
    int (*callback)(struct texturevariant_s* instance, void* paramaters),
    void* paramaters);

/**
 * Attach new analysis data. If data is already present it will be replaced.
 * Ownership is given to the Texture.
 *
 * @param analysis  Identifier of the data being attached.
 * @param data  Data to be attached.
 */
void Texture_AttachAnalysis(texture_t* tex, texture_analysisid_t analysis, void* data);

/**
 * Detach any associated analysis data. Ownership is relinquished to caller.
 *
 * @return  Associated data for the specified analysis identifier.
 */
void* Texture_DetachAnalysis(texture_t* tex, texture_analysisid_t analysis);

/// @return  Associated data for the specified analysis identifier.
void* Texture_Analysis(const texture_t* tex, texture_analysisid_t analysis);

/// @return  Unique identifier.
textureid_t Texture_Id(const texture_t* tex);

/// @return  Symbolic name.
const char* Texture_Name(const texture_t* tex);

/// @return  @c true iff Texture represents an image loaded from an IWAD.
boolean Texture_IsFromIWAD(const texture_t* tex);

/// Retrieve logical dimensions (not necessarily the same as pixel dimensions).
void Texture_Dimensions(const texture_t* tex, int* width, int* height);

/**
 * Change logical dimensions.
 * @param width  Logical width.
 * @param height  Logical height.
 */
void Texture_SetDimensions(texture_t* tex, int width, int height);

/// @return  Logical width (not necessarily the same as pixel width).
int Texture_Width(const texture_t* tex);

/**
 * Change logical width.
 * @param width  Width in logical pixels.
 */
void Texture_SetWidth(texture_t* tex, int width);

/// @return  Logical height (not necessarily the same as pixel height).
int Texture_Height(const texture_t* tex);

/**
 * Change logical height.
 * @param height  Height in logical pixels.
 */
void Texture_SetHeight(texture_t* tex, int height);

/// @return  Type-specific index of the wrapped image object.
int Texture_TypeIndex(const texture_t* tex);

/// @return  Texture namespace identifier.
texturenamespaceid_t Texture_Namespace(const texture_t* tex);

#endif /* LIBDENG_GL_TEXTURE_H */
