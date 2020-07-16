/** @file rawtexture.h Raw Texture.
 *
 * @author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_RESOURCE_RAWTEXTURE
#define DE_RESOURCE_RAWTEXTURE

#include "dd_share.h" // For lumpnum_t
#include <de/string.h>

/**
 * A rawtex is a lump raw graphic that has been prepared for render.
 */
struct rawtex_t
{
    de::String name; ///< Percent-encoded.
    lumpnum_t lumpNum;
    DGLuint tex; /// Name of the associated DGL texture.
    short width, height;
    byte masked;

    rawtex_t(de::String name, lumpnum_t lumpNum)
        : name(name)
        , lumpNum(lumpNum)
        , tex(0)
        , width(0)
        , height(0)
        , masked(0)
    {}
};

#endif // DE_RESOURCE_RAWTEXTURE
