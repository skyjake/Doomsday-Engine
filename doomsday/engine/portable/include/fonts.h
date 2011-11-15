/**\file fonts.h
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
 * Fonts collection.
 * @ingroup refresh
 *
 * 'Runtime' fonts are not loaded until precached or actually needed.
 * They may be cleared, in which case they will be reloaded when needed.
 *
 * 'System' fonts are loaded at startup and remain in memory all the time.
 * After clearing they must be manually reloaded.
 */

#ifndef LIBDENG_REFRESH_FONTS_H
#define LIBDENG_REFRESH_FONTS_H

#include "def_data.h"
#include "font.h"
#include "bitmapfont.h"
#include "dd_string.h"
#include "uri.h"

enum fontnamespaceid_t; // Defined in dd_share.h

struct fontbind_s;

/// Components within a Font path hierarchy are delimited by this character.
#define FONTS_PATH_DELIMITER        '/'

/// Register the console commands, variables, etc..., of this module.
void Fonts_Register(void);

/// Initialize this module.
void Fonts_Init(void);

/// Shutdown this module.
void Fonts_Shutdown(void);

/// To be called during a definition database reset to clear all links to defs.
void Fonts_ClearDefinitionLinks(void);

/**
 * Try to interpret a known font namespace identifier from @a str. If found to match
 * a known namespace name, return the associated identifier. If the reference @a str is
 * not valid (i.e., equal to NULL or is a zero-length string) then the special identifier
 * @c FN_ANY is returned. Otherwise @c FN_INVALID.
 */
fontnamespaceid_t Fonts_ParseNamespace(const char* str);

/// @return  Name associated with the identified @a namespaceId else a zero-length string.
const ddstring_t* Fonts_NamespaceName(fontnamespaceid_t namespaceId);

/// @return  Total number of unique Fonts in the collection.
uint Fonts_Size(void);

/// @return  Number of unique Fonts in the identified @a namespaceId.
uint Fonts_Count(fontnamespaceid_t namespaceId);

/// Clear all Fonts in all namespaces (and release any acquired GL-textures).
void Fonts_Clear(void);

/// Clear all Fonts flagged 'runtime' (and release any acquired GL-textures).
void Fonts_ClearRuntime(void);

/// Clear all Fonts flagged 'system' (and release any acquired GL-textures).
void Fonts_ClearSystem(void);

/**
 * Clear all fonts in the identified namespace(s) (and release any acquired GL-textures).
 *
 * @param namespaceId  Unique identifier of the namespace to process or @c FN_ANY
 *     to clear all fonts in any namespace.
 */
void Fonts_ClearNamespace(fontnamespaceid_t namespaceId);

/// @return  Font associated with unique identifier @a fontId else @c NULL.
font_t* Fonts_ToFont(fontid_t fontId);

/// @return  Unique name associated with the specified Font.
fontid_t Fonts_Id(font_t* font);

/// @return  Unique identifier of the namespace this Font is in.
fontnamespaceid_t Fonts_Namespace(struct fontbind_s* bind);

/// @return  Symbolic name/path-to this Font. Must be destroyed with Str_Delete().
ddstring_t* Fonts_ComposePath(font_t* font);

/// @return  Unique name/path-to @a Font. Must be destroyed with Uri_Delete().
Uri* Fonts_ComposeUri(font_t* font);

/**
 * Update the Font according to the supplied definition.
 * To be called after an engine update/reset.
 *
 * @param font  Font to be updated.
 * @param def  font definition to update using.
 */
void Fonts_Rebuild(font_t* font, ded_compositefont_t* def);

/**
 * Search the collection for a Font associated with @a uri.
 * @return  Found Font else @c NULL.
 */
font_t* Fonts_ResolveUri2(const Uri* uri, boolean quiet);
font_t* Fonts_ResolveUri(const Uri* uri); /*quiet=!(verbose >= 1)*/

/// Same as Fonts::ResolveUri except @a uri is a C-string.
font_t* Fonts_ResolveUriCString2(const char* uri, boolean quiet);
font_t* Fonts_ResolveUriCString(const char* uri); /*quiet=!(verbose >= 1)*/

/// Load an external font from a local file.
font_t* Fonts_CreateFromFile(const char* name, const char* filePath);

/// Create a bitmap composite font from @a def.
font_t* Fonts_CreateFromDef(ded_compositefont_t* def);

/**
 * Here follows miscellaneous routines currently awaiting refactoring into the
 * revised resource and texture management APIs.
 */

/**
 * To be called during engine/gl-subsystem reset to release all resources
 * acquired from the GL subsystem (v-buffers, d-lists, textures, etc...)
 * for fonts.
 * \note Called automatically by this subsystem prior to module shutdown.
 */
void Fonts_ReleaseRuntimeGLResources(void);
void Fonts_ReleaseSystemGLResources(void);
void Fonts_ReleaseGLResourcesByNamespace(fontnamespaceid_t namespaceId);

void Fonts_ReleaseRuntimeGLTextures(void);
void Fonts_ReleaseSystemGLTextures(void);
void Fonts_ReleaseGLTexturesByNamespace(fontnamespaceid_t namespaceId);

/// @return  List of collected font names.
ddstring_t** Fonts_CollectNames(int* count);

int Fonts_Ascent(font_t* font);
int Fonts_Descent(font_t* font);
int Fonts_Leading(font_t* font);

/**
 * Query the visible dimensions of a character in this font.
 */
void Fonts_CharDimensions(font_t* font, int* width, int* height, unsigned char ch);
int Fonts_CharHeight(font_t* font, unsigned char ch);
int Fonts_CharWidth(font_t* font, unsigned char ch);

/**
 * \deprecated Only exists because font_t* is not a public reference.
 * @return  Unique identifier associated with the found font else @c 0
 */
fontid_t Fonts_IndexForUri(const Uri* uri);

#endif /* LIBDENG_REFRESH_FONTS_H */
