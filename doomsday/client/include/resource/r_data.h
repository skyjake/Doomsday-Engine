/** @file r_data.h Data structures for refresh.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_REFRESH_DATA_H
#define LIBDENG_REFRESH_DATA_H

#include "Textures"
#include <de/Vector>

//struct font_s;

/**
 * Textures used in the lighting system.
 */
typedef enum lightingtexid_e {
    LST_DYNAMIC, ///< Round dynamic light
    LST_GRADIENT, ///< Top-down gradient
    LST_RADIO_CO, ///< FakeRadio closed/open corner shadow
    LST_RADIO_CC, ///< FakeRadio closed/closed corner shadow
    LST_RADIO_OO, ///< FakeRadio open/open shadow
    LST_RADIO_OE, ///< FakeRadio open/edge shadow
    LST_CAMERA_VIGNETTE,
    NUM_LIGHTING_TEXTURES
} lightingtexid_t;

typedef enum flaretexid_e {
    FXT_ROUND,
    FXT_FLARE,
    FXT_BRFLARE,
    FXT_BIGFLARE,
    NUM_SYSFLARE_TEXTURES
} flaretexid_t;

typedef enum uitexid_e {
    UITEX_MOUSE,
    UITEX_CORNER,
    UITEX_FILL,
    UITEX_SHADE,
    UITEX_HINT,
    UITEX_LOGO,
    UITEX_BACKGROUND,
    NUM_UITEXTURES
} uitexid_t;

void R_InitSystemTextures();
void R_InitCompositeTextures();
void R_InitFlatTextures();
void R_InitSpriteTextures();

/**
 * Search the application's Textures collection for a texture with the specified
 * @a schemeName and @a resourceUri.
 *
 * @param schemeName  Unique name of the scheme in which to search.
 * @param resourceUri  Path to the (image) resource to find the texture for.
 * @return  The found texture; otherwise @c 0.
 */
de::Texture *R_FindTextureByResourceUri(de::String schemeName, de::Uri const *resourceUri);

de::Texture *R_DefineTexture(de::String schemeName, de::Uri const &resourceUri, de::Vector2i const &dimensions);
de::Texture *R_DefineTexture(de::String schemeName, de::Uri const &resourceUri);

#endif // LIBDENG_REFRESH_DATA_H
