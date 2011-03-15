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
struct texture_variantlist_node_s;

/**
 * Texture
 *
 * Presents an abstract interface to all supported texture types so that
 * they may be managed transparently.
 */
typedef struct texture_s {
    textureid_t id;
    char name[9];
    gltexture_type_t type;
    /// Per-type index (e.g., if type=GLT_FLAT this is a flat index).
    int index;
    struct texture_variantlist_node_s* variants;
    uint hashNext; // 1-based index
} texture_t;

void Texture_Destruct(texture_t* tex);

/**
 * Add a new prepared variant to the list of resources for this Texture.
 * Texture takes ownership of the variant.
 *
 * @param variant  Variant instance to add to the resource list.
 */
void Texture_AddVariant(texture_t* tex, struct texturevariant_s* variant);

const char* Texture_Name(const texture_t* tex);

float Texture_GetWidth(const texture_t* tex);

float Texture_GetHeight(const texture_t* tex);

boolean Texture_IsFromIWAD(const texture_t* tex);

/**
 * Sets the minification mode of the specified texture.
 *
 * @param tex  The texture to be updated.
 * @param minMode  The GL minification mode to set.
 */
void Texture_SetMinMode(texture_t* tex, int minMode);

/**
 * Deletes all GL texture instances for the specified texture.
 */
void Texture_ReleaseTextures(texture_t* tex);

int Texture_IterateInstances(texture_t* tex,
    int (*callback)(struct texturevariant_s* instance, void* paramaters),
    void* paramaters);

struct texturevariant_s* Texture_Prepare(texture_t* tex, void* context,
    byte* result);

#endif /* LIBDENG_GL_TEXTURE_H */
