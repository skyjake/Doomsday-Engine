/**
 * @file fonts.h
 *
 * Font Resources. @ingroup resource
 *
 * 'Runtime' fonts are not loaded until precached or actually needed. They may
 * be cleared, in which case they will be reloaded when needed.
 *
 * 'System' fonts are loaded at startup and remain in memory all the time. After
 * clearing they must be manually reloaded.
 *
 * 'Clear'ing a Font is to "undefine" it - any names bound to it are deleted,
 * any GL textures acquired for it are 'released'. The Font instance record used
 * to represent it is also deleted.
 *
 * 'Release'ing a Font will leave it defined (any names bound to it will persist)
 * and any GL textures acquired for it are so too released. Note that the Font
 * instance record used to represent it will NOT be deleted.
 *
 * Thus there are two general states for fonts in the collection:
 *
 *   1) Declared but not defined.
 *   2) Declared and defined.
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

#ifndef LIBDENG_RESOURCE_FONTS_H
#define LIBDENG_RESOURCE_FONTS_H

#include "uri.h"
#include "def_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Special value used to signify an invalid font id.
#define NOFONTID                    0

enum fontschemeid_e; // Defined in dd_share.h
struct font_s;

/// Register the console commands, variables, etc..., of this module.
void Fonts_Register(void);

/// Determines whether the fonts collection has been initialized.
boolean Fonts_IsInitialized(void);

/// Initialize this module.
void Fonts_Init(void);

/// Shutdown this module.
void Fonts_Shutdown(void);

/// To be called during a definition database reset to clear all links to defs.
void Fonts_ClearDefinitionLinks(void);

/**
 * Try to interpret a font scheme identifier from @a str.
 * If found to match a known scheme name, return the associated identifier.
 * If the reference @a str is not valid (i.e., NULL or a zero-length string)
 * then the special identifier @c FS_ANY is returned.
 * Otherwise @c FS_INVALID.
 */
fontschemeid_t Fonts_ParseScheme(const char* str);

/// @return  Name associated with the identified @a schemeId else a zero-length string.
const ddstring_t* Fonts_SchemeName(fontschemeid_t schemeId);

/// @return  Total number of unique Fonts in the collection.
uint Fonts_Size(void);

/// @return  Number of unique Fonts in the identified @a schemeId.
uint Fonts_Count(fontschemeid_t schemeId);

/// Clear all fonts in all schemes (and release any acquired GL-textures).
void Fonts_Clear(void);

/// Clear all fonts flagged 'runtime' (and release any acquired GL-textures).
void Fonts_ClearRuntime(void);

/// Clear all fonts flagged 'system' (and release any acquired GL-textures).
void Fonts_ClearSystem(void);

/**
 * Clear all fonts in the identified scheme(s) (and release any acquired GL-textures).
 *
 * @param schemeId  Unique identifier of the scheme to process
 *     or @c FS_ANY to clear all fonts in any scheme.
 */
void Fonts_ClearScheme(fontschemeid_t schemeId);

/// @return  Unique identifier of the primary name for @a font else @c NOFONTID.
fontid_t Fonts_Id(struct font_s* font);

/// @return  Font associated with unique identifier @a fontId else @c NULL.
struct font_s* Fonts_ToFont(fontid_t fontId);

/// @return  Font associated with the scheme-unique identifier @a index else @c NOFONTID.
fontid_t Fonts_FontForUniqueId(fontschemeid_t schemeId, int uniqueId);

/// @return  Scheme-unique identfier associated with the identified @a fontId.
int Fonts_UniqueId(fontid_t fontId);

/// @return  Unique identifier of the scheme this name is in.
fontschemeid_t Fonts_Scheme(fontid_t fontId);

/// @return  Symbolic name/path-to this font as a string.
AutoStr* Fonts_ComposePath(fontid_t fontId);

/// @return  URI to this font. Must be destroyed with Uri_Delete().
Uri* Fonts_ComposeUri(fontid_t fontid);

/// @return  Unique URN to this font. Must be destroyed with Uri_Delete().
Uri* Fonts_ComposeUrn(fontid_t fontId);

/**
 * Update the Font according to the supplied definition.
 * To be called after an engine update/reset.
 *
 * @param font  Font to be updated.
 * @param def  font definition to update using.
 */
void Fonts_RebuildFromDef(struct font_s* font, ded_compositefont_t* def);

void Fonts_RebuildFromFile(struct font_s* font, const char* resourcePath);

/**
 * Search the Fonts collection for a font associated with @a uri.
 *
 * @param uri    Either a path or a URN.
 *
 * @return  Unique identifier of the found texture else @c NOFONTID.
 */
fontid_t Fonts_ResolveUri(Uri const * uri); /*quiet=!(verbose >= 1)*/

/**
 * @copydoc Fonts_ResolveUri()
 *
 * @param uri       Either a path or URN to the font.
 * @param quiet     @c true = suppress the console message that is printed if the Uri cannot be found.
 */
fontid_t Fonts_ResolveUri2(Uri const* uri, boolean quiet);

/// Same as Fonts::ResolveUri except @a uri is a C-string.
fontid_t Fonts_ResolveUriCString2(const char* uri, boolean quiet);
fontid_t Fonts_ResolveUriCString(const char* uri); /*quiet=!(verbose >= 1)*/

/**
 * Declare a font in the collection. If a font with the specified @a uri
 * already exists, its unique identifier is returned..
 *
 * @param uri  Uri representing a path to the font in the virtual hierarchy.
 * @param uniqueId  Scheme-unique identifier to associate with the font.
 * @return  Unique identifier for this font unless @a uri is invalid, in
 *     which case @c NOFONTID is returned.
 */
fontid_t Fonts_Declare(Uri* uri, int uniqueId);

/// Load an external font from a local file.
struct font_s* Fonts_CreateFromFile(fontid_t id, const char* filePath);

/// Create a bitmap composite font from @a def.
struct font_s* Fonts_CreateFromDef(fontid_t id, ded_compositefont_t* def);

/**
 * Iterate over defined Fonts in the collection making a callback for
 * each visited. Iteration ends when all fonts have been visited or a
 * callback returns non-zero.
 *
 * @param schemeId  If a valid scheme identifier, only consider
 *     fonts in this scheme, otherwise visit all fonts.
 * @param callback  Callback function ptr.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Fonts_Iterate2(fontschemeid_t schemeId, int (*callback)(struct font_s* font, void* paramaters), void* paramaters);
int Fonts_Iterate(fontschemeid_t schemeId, int (*callback)(struct font_s* font, void* paramaters)); /*paramaters=NULL*/

/**
 * Iterate over declared fonts in the collection making a callback for
 * each visited. Iteration ends when all fonts have been visited or a
 * callback returns non-zero.
 *
 * @param schemeId  If a valid scheme identifier, only consider
 *     fonts in this scheme, otherwise visit all fonts.
 * @param callback  Callback function ptr.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Fonts_IterateDeclared2(fontschemeid_t schemeId, int (*callback)(fontid_t textureId, void* paramaters), void* paramaters);
int Fonts_IterateDeclared(fontschemeid_t schemeId, int (*callback)(fontid_t textureId, void* paramaters)); /*paramaters=NULL*/

struct font_s *R_CreateFontFromFile(Uri *uri, char const *resourcePath);
struct font_s *R_CreateFontFromDef(ded_compositefont_t *def);

/*
 * Here follows miscellaneous routines currently awaiting refactoring into the
 * revised resource and texture management APIs.
 */

/**
 * To be called during engine/gl-subsystem reset to release all resources
 * acquired from the GL subsystem (v-buffers, d-lists, textures, etc...)
 * for fonts.
 *
 * @note Called automatically prior to module shutdown.
 * @todo Define new texture schemes for font textures and refactor away.
 */
void Fonts_ReleaseRuntimeTextures(void);
void Fonts_ReleaseSystemTextures(void);

/// @return  List of collected font names.
ddstring_t** Fonts_CollectNames(int* count);

int Fonts_Ascent(struct font_s* font);
int Fonts_Descent(struct font_s* font);
int Fonts_Leading(struct font_s* font);

/**
 * Query the visible dimensions of a character in this font.
 */
void Fonts_CharSize(struct font_s* font, Size2Raw* size, unsigned char ch);
int Fonts_CharHeight(struct font_s* font, unsigned char ch);
int Fonts_CharWidth(struct font_s* font, unsigned char ch);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RESOURCE_FONTS_H */
