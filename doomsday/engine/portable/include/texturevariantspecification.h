/**\file image.h
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

#ifndef LIBDENG_GL_TEXTUREVARIANTSPECIFICATION_H
#define LIBDENG_GL_TEXTUREVARIANTSPECIFICATION_H

typedef struct material_load_params_s {
    int tmap, tclass;
    struct {
        byte flags; /// @see textureFlags
        byte border;
    } tex;
} material_load_params_t;

typedef enum {
    TC_UNKNOWN = -1,
    TEXTUREUSAGECONTEXT_FIRST = 0,
    TC_UI = TEXTUREUSAGECONTEXT_FIRST,
    TC_MAPSURFACE_DIFFUSE,
    TC_MAPSURFACE_REFLECTION,
    TC_MAPSURFACE_REFLECTIONMASK,
    TC_MAPSURFACE_LIGHTMAP,
    TC_MAPSURFACE_DETAIL,
    TC_SPRITE_DIFFUSE,
    TC_MODELSKIN_DIFFUSE,
    TC_MODELSKIN_REFLECTION,
    TC_HALO_LUMINANCE,
    TC_PSPRITE_DIFFUSE,
    TC_SKYSPHERE_DIFFUSE,
    TEXTUREUSAGECONTEXT_LAST = TC_SKYSPHERE_DIFFUSE
} textureusagecontext_t;

#define TEXTUREUSAGECONTEXT_COUNT (\
    TEXTUREUSAGECONTEXT_LAST + 1 - TEXTUREUSAGECONTEXT_FIRST )

#define VALID_TEXTUREUSAGECONTEXT(tc) (\
    (tc) >= TEXTUREUSAGECONTEXT_FIRST && (tc) <= TEXTUREUSAGECONTEXT_LAST)

/**
 * @defGroup textureFlags  Texture Flags
 */
/*@{*/
#define TF_ZEROMASK                 0x1 // Zero the alpha of loaded textures.
#define TF_NO_COMPRESSION           0x2 // Do not compress the loaded textures.
#define TF_UPSCALE_AND_SHARPEN      0x4
#define TF_MONOCHROME               0x8
/*@}*/

typedef enum {
    TEXTURESPECIFICATIONTYPE_FIRST = 0,
    TS_DEFAULT = TEXTURESPECIFICATIONTYPE_FIRST,
    TS_TRANSLATED,
    TS_DETAIL,
    TEXTURESPECIFICATIONTYPE_LAST = TS_DETAIL
} texturespecificationtype_t;

#define TEXTURESPECIFICATIONTYPE_COUNT (\
    TEXTURESPECIFICATIONTYPE_LAST + 1 - TEXTURESPECIFICATIONTYPE_FIRST )

#define VALID_TEXTURESPECIFICATIONTYPE(t) (\
    (t) >= TEXTURESPECIFICATIONTYPE_FIRST && (t) <= TEXTURESPECIFICATIONTYPE_LAST)

typedef struct texturevariantspecification_s {
    textureusagecontext_t context;
    texturespecificationtype_t type;
    byte flags; /// @see textureFlags
    byte border; /// In pixels, added to all four edges of the texture.
    union {
        struct {
            int tclass, tmap; // Color translation.
        } translated;
        struct {
            float contrast;
        } detail;
    } data; // type-specific data.
} texturevariantspecification_t;

#endif /* LIBDENG_GL_TEXTUREVARIANTSPECIFICATION_H */
