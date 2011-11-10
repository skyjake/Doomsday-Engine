/**\file textures.h
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

/**
 * Textures Collection.
 * @ingroup refresh
 */

#ifndef LIBDENG_REFRESH_TEXTURES_H
#define LIBDENG_REFRESH_TEXTURES_H

#include "texture.h"

enum texturenamespaceid_t; // Defined in dd_share.h

/// Components within a Texture path hierarchy are delimited by this character.
#define TEXTURES_PATH_DELIMITER     '/'

/// Unique identifier associated with each Texture instance.
typedef uint textureid_t;

/// Register the console commands, variables, etc..., of this module.
void Textures_Register(void);

/// Initialize this module.
void Textures_Init(void);

/// Shutdown this module.
void Textures_Shutdown(void);

/**
 * Try to interpret a known texture namespace identifier from @a str. If found to match
 * a known namespace name, return the associated identifier. If the reference @a str is
 * not valid (i.e., equal to NULL or is a zero-length string) then the special identifier
 * @c TN_ANY is returned. Otherwise @c TN_INVALID.
 */
texturenamespaceid_t Textures_ParseNamespace(const char* str);

/// @return  Name associated with the identified @a namespaceId else a zero-length string.
const ddstring_t* Textures_NamespaceName(texturenamespaceid_t namespaceId);

/// @return  Total number of unique Textures in the collection.
uint Textures_Size(void);

/// @return  Number of unique Textures in the identified @a namespaceId.
uint Textures_Count(texturenamespaceid_t namespaceId);

/// Clear all textures in all namespaces (and release any acquired GL-textures).
void Textures_Clear(void);

/// Clear all textures flagged 'runtime' (and release any acquired GL-textures).
void Textures_ClearRuntime(void);

/// Clear all textures flagged 'system' (and release any acquired GL-textures).
void Textures_ClearSystem(void);

/**
 * Clear all textures in the identified namespace(s) (and release any acquired GL-textures).
 *
 * @param namespaceId  Unique identifier of the namespace to process or @c TN_ANY
 *     to clear all textures in any namespace.
 */
void Textures_ClearByNamespace(texturenamespaceid_t namespaceId);

/// @return  Texture associated with unique identifier @a textureId else @c NULL.
texture_t* Textures_ToTexture(textureid_t textureId);

/// @return  Unique identifier associated with @a texture else @c 0.
textureid_t Textures_Id(texture_t* texture);

/// @return  Unique identifier of the namespace this texture is in.
texturenamespaceid_t Textures_Namespace(texture_t* texture);

/// @return  Symbolic name/path-to this Texture. Must be destroyed with Str_Delete().
ddstring_t* Textures_ComposePath(texture_t* texture);

/// @return  Unique name/path-to @a Texture. Must be destroyed with Uri_Delete().
Uri* Textures_ComposeUri(texture_t* texture);

/**
 * Search the Textures collection for a Texture associated with @a uri.
 * @return  Found Texture else @c NULL.
 */
texture_t* Textures_TextureForUri2(const Uri* uri, boolean quiet);
texture_t* Textures_TextureForUri(const Uri* uri); /*quiet=!(verbose >= 1)*/

/// Same as Textures::TextureForUri except @a uri is a C-string.
texture_t* Textures_TextureForUriCString2(const char* uri, boolean quiet);
texture_t* Textures_TextureForUriCString(const char* uri); /*quiet=!(verbose >= 1)*/

/**
 * Create a new Texture and insert it into the collection. If a texture with the
 * specified @a uri already exists, it's metadata will be updated with the new values
 * and the existing texture is returned.
 *
 * @param uri  Uri representing the path to the texture in the virtual hierarchy.
 * @param flags  @see textureFlags
 * @param width  Logical width of the texture. Can be zero in which case it will be
 *    inherited from the actual pixel width of the texture at load time.
 * @param height  Logical height of the texture. Can be zero in which case it will be
 *    inherited from the actual pixel height of the texture at load time.
 * @param userData  User data to associate with the resultant texture.
 */
texture_t* Textures_CreateWithDimensions(const Uri* uri, int flags, int width, int height, void* userData);
texture_t* Textures_Create(const Uri* uri, int flags, void* userData); /* width=0, height=0*/

/**
 * Iterate over textures in the collection making a callback for each.
 * Iteration ends when all textures have been visited or a callback returns non-zero.
 *
 * @param namespaceId  If a valid namespace identifier, only consider textures in
 *     this namespace, otherwise, process all textures.
 * @param callback  Callback function ptr.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Textures_Iterate2(texturenamespaceid_t namespaceId, int (*callback)(texture_t* texture, void* paramaters), void* paramaters);
int Textures_Iterate(texturenamespaceid_t namespaceId, int (*callback)(texture_t* texture, void* paramaters)); /*paramaters=NULL*/

#endif /* LIBDENG_REFRESH_TEXTURES_H */
