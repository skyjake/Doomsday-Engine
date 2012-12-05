/**
 * @file rawtexture.h Raw Texture.
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

#ifndef LIBDENG_RESOURCE_RAWTEXTURE_H
#define LIBDENG_RESOURCE_RAWTEXTURE_H

#include "dd_share.h" // For lumpnum_t

#include <de/str.h>

/**
 * A rawtex is a lump raw graphic that has been prepared for render.
 */
typedef struct rawtex_s {
    ddstring_t name; ///< Percent-encoded.
    lumpnum_t lumpNum;
    DGLuint tex; /// Name of the associated DGL texture.
    short width, height;
    byte masked;
    struct rawtex_s *next;
} rawtex_t;

#ifdef __cplusplus
extern "C" {
#endif

void R_InitRawTexs(void);
void R_UpdateRawTexs(void);

/**
 * Returns a rawtex_t* for the given lump if one already exists else @c NULL.
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
rawtex_t **R_CollectRawTexs(int* count);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RESOURCE_RAWTEXTURE_H */
