/** @file fonts.h Font resource collection.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_RESOURCE_FONTS_H
#define DENG_RESOURCE_FONTS_H

#include "dd_share.h" // fontschemeid_t
#include "def_data.h"
#include "uri.hh"

/// Special value used to signify an invalid font id.
#define NOFONTID                    0

struct font_s;

namespace de {

/**
 * Font resource collection.
 *
 * @em Runtime fonts are not loaded until precached or actually needed. They may
 * be cleared, in which case they will be reloaded when needed.
 *
 * @em System fonts are loaded at startup and remain in memory all the time. After
 * clearing they must be manually reloaded.
 *
 * "Clearing" a font means any names bound to it are deleted (on client side, any
 * GL textures acquired for it are 'released' at this time). The Font instance
 * record used  to represent it is also deleted.
 *
 * "Releasing" a font on client side will release any GL textures acquired for it.
 *
 * Thus there are two general states for a font:
 *
 *   1) Declared but not defined.
 *   2) Declared and defined.
 *
 * @ingroup resource
 */
class Fonts
{
public:
    Fonts();

    /// Register the console commands, variables, etc..., of this module.
    static void consoleRegister();

    /// To be called during a definition database reset to clear all links to defs.
    void clearDefinitionLinks();

    /**
     * Try to interpret a font scheme identifier from @a str.
     * If found to match a known scheme name, return the associated identifier.
     * If the reference @a str is not valid (i.e., NULL or a zero-length string)
     * then the special identifier @c FS_ANY is returned.
     * Otherwise @c FS_INVALID.
     */
    fontschemeid_t parseScheme(char const *str);

    /// @return  Name associated with the identified @a schemeId else a zero-length string.
    ddstring_t const *schemeName(fontschemeid_t schemeId);

    /// @return  Total number of unique Fonts in the collection.
    uint size();

    /// @return  Number of unique Fonts in the identified @a schemeId.
    uint count(fontschemeid_t schemeId);

    /// Clear all fonts in all schemes (and release any acquired GL-textures).
    void clear();

    /// Clear all fonts flagged 'runtime' (and release any acquired GL-textures).
    void clearRuntime();

    /// Clear all fonts flagged 'system' (and release any acquired GL-textures).
    void clearSystem();

    /**
     * Clear all fonts in the identified scheme(s) (and release any acquired GL-textures).
     *
     * @param schemeId  Unique identifier of the scheme to process
     *     or @c FS_ANY to clear all fonts in any scheme.
     */
    void clearScheme(fontschemeid_t schemeId);

    /// @return  Unique identifier of the primary name for @a font else @c NOFONTID.
    fontid_t id(struct font_s *font);

    /// @return  Font associated with unique identifier @a fontId else @c NULL.
    struct font_s *toFont(fontid_t fontId);

    /// @return  Font associated with the scheme-unique identifier @a index else @c NOFONTID.
    fontid_t fontForUniqueId(fontschemeid_t schemeId, int uniqueId);

    /// @return  Scheme-unique identfier associated with the identified @a fontId.
    int uniqueId(fontid_t fontId);

    /// @return  Unique identifier of the scheme this name is in.
    fontschemeid_t scheme(fontid_t fontId);

    /// @return  Symbolic name/path-to this font as a string.
    AutoStr *composePath(fontid_t fontId);

    /// @return  URI to this font. Must be destroyed with Uri_Delete().
    Uri *composeUri(fontid_t fontid);

    /// @return  Unique URN to this font. Must be destroyed with Uri_Delete().
    Uri *composeUrn(fontid_t fontId);

    /**
     * Search the Fonts collection for a font associated with @a uri.
     *
     * @param uri    Either a path or URN to the font.
     * @param quiet  @c true: suppress the console message that is printed if the Uri cannot be found.
     *
     * @return  Unique identifier of the found texture else @c NOFONTID.
     */
    fontid_t resolveUri(Uri const &uri, boolean quiet);

    /**
     * Declare a font in the collection. If a font with the specified @a uri
     * already exists, its unique identifier is returned..
     *
     * @param uri       Uri representing a path to the font in the virtual hierarchy.
     * @param uniqueId  Scheme-unique identifier to associate with the font.
     *
     * @return  Unique identifier for this font unless @a uri is invalid, in
     * which case @c NOFONTID is returned.
     */
    fontid_t declare(Uri const &uri, int uniqueId);

    struct font_s *createFontFromFile(Uri const &uri, char const *resourcePath);

    struct font_s *createFontFromDef(ded_compositefont_t *def);

    /**
     * Iterate over defined Fonts in the collection making a callback for each
     * visited. Iteration ends when all fonts have been visited or a callback
     * returns non-zero.
     *
     * @param schemeId  If a valid scheme identifier, only consider fonts in this
     *                  scheme, otherwise visit all fonts.
     * @param callback  Callback function ptr.
     * @param context   Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int iterate(fontschemeid_t schemeId, int (*callback)(struct font_s *font, void *context),
                void *context = 0);

    /**
     * Iterate over declared fonts in the collection making a callback for each
     * visited. Iteration ends when all fonts have been visited or a callback
     * returns non-zero.
     *
     * @param schemeId  If a valid scheme identifier, only consider fonts in this
     *                  scheme, otherwise visit all fonts.
     * @param callback  Callback function ptr.
     * @param context   Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int iterateDeclared(fontschemeid_t schemeId, int (*callback)(fontid_t textureId, void *context),
                        void *context = 0);

    inline void releaseAllTextures() { releaseTexturesByScheme(FS_ANY); }

    void releaseTexturesByScheme(fontschemeid_t schemeId);

    /**
     * To be called during engine/gl-subsystem reset to release all resources
     * acquired from the GL subsystem (v-buffers, d-lists, textures, etc...)
     * for fonts.
     *
     * @note Called automatically prior to module shutdown.
     * @todo Define new texture schemes for font textures and refactor away.
     */
    void releaseRuntimeTextures();
    void releaseSystemTextures();

    /// @return  List of collected font names.
    ddstring_t **collectNames(int *count);

public: /// @todo should be private:
    /// Load an external font from a local file.
    struct font_s *createFromFile(fontid_t id, char const *filePath);

    /// Create a bitmap composite font from @a def.
    struct font_s *createFromDef(fontid_t id, ded_compositefont_t *def);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

/**
 * Update the Font according to the supplied definition.
 * To be called after an engine update/reset.
 *
 * @param font  Font to be updated.
 * @param def  font definition to update using.
 */
void Font_RebuildFromDef(struct font_s *font, ded_compositefont_t *def);

void Font_RebuildFromFile(struct font_s *font, char const *resourcePath);

/// Same as Fonts_ResolveUri except @a uri is a C-string.
fontid_t Fonts_ResolveUriCString2(char const *uri, boolean quiet);
fontid_t Fonts_ResolveUriCString(char const *uri); /*quiet=!(verbose >= 1)*/

struct font_s *R_CreateFontFromFile(Uri *uri, char const *resourcePath);
struct font_s *R_CreateFontFromDef(ded_compositefont_t *def);

/*
 * Here follows miscellaneous routines currently awaiting refactoring into the
 * revised resource and texture management APIs.
 */

int Fonts_Ascent(struct font_s *font);
int Fonts_Descent(struct font_s *font);
int Fonts_Leading(struct font_s *font);

/**
 * Query the visible dimensions of a character in this font.
 */
void Fonts_CharSize(struct font_s *font, Size2Raw *size, unsigned char ch);
int Fonts_CharHeight(struct font_s *font, unsigned char ch);
int Fonts_CharWidth(struct font_s *font, unsigned char ch);

#endif // DENG_RESOURCE_FONTS_H
