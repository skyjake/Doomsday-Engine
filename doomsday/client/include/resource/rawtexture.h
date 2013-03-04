/** @file rawtexture.h Raw Texture.
 *
 * @author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_RESOURCE_RAWTEXTURE
#define LIBDENG_RESOURCE_RAWTEXTURE

#include "dd_share.h" // For lumpnum_t
#include <de/str.h>

/**
 * A rawtex is a lump raw graphic that has been prepared for render.
 */
struct rawtex_t
{
    ddstring_t name; ///< Percent-encoded.
    lumpnum_t lumpNum;
    DGLuint tex; /// Name of the associated DGL texture.
    short width, height;
    byte masked;
    rawtex_t *next;
};

void R_InitRawTexs();
void R_UpdateRawTexs();

/**
 * Returns a rawtex_t for the given lump if one already exists; otherwise @c 0.
 */
rawtex_t *R_FindRawTex(lumpnum_t lumpNum);

/**
 * Get a rawtex_t data structure for a raw texture specified with a WAD lump
 * number. Allocates a new rawtex_t if it hasn't been loaded yet.
 */
rawtex_t *R_GetRawTex(lumpnum_t lumpNum);

/**
 * Returns a NULL-terminated array of pointers to all the rawtexs.
 * The array must be freed with Z_Free.
 */
rawtex_t **R_CollectRawTexs(int *count = 0);

#endif // LIBDENG_RESOURCE_RAWTEXTURE
