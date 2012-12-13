/** @file r_data.h Data structures for refresh.
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

#ifndef LIBDENG_REFRESH_DATA_H
#define LIBDENG_REFRESH_DATA_H

#include "dd_types.h"
#include "gl/gl_main.h"
#include "dd_def.h"
#include "thinker.h"
#include "def_data.h"
#include "textures.h"

struct texture_s;
struct font_s;

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

typedef struct {
    DGLuint tex;
} ddtexture_t;

#ifdef __cplusplus
extern "C" {
#endif

void R_InitSystemTextures(void);
void R_InitCompositeTextures(void);
void R_InitFlatTextures(void);
void R_InitSpriteTextures(void);

patchid_t R_DeclarePatch(char const *name);

/**
 * Retrieve extended info for the patch associated with @a id.
 * @param id  Unique identifier of the patch to lookup.
 * @param info  Extend info will be written here if found.
 * @return  @c true= Extended info for this patch was found.
 */
boolean R_GetPatchInfo(patchid_t id, patchinfo_t *info);

/// @return  Uri for the patch associated with @a id. Should be released with Uri_Delete()
Uri *R_ComposePatchUri(patchid_t id);

/// @return  Path for the patch associated with @a id. A zero-length string is
///          returned if the id is invalid/unknown.
AutoStr *R_ComposePatchPath(patchid_t id);
#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
struct texture_s *R_DefineTexture(de::String schemeName, de::Uri const &resourceUri, QSize const &dimensions);
struct texture_s *R_DefineTexture(de::String schemeName, de::Uri const &resourceUri);
#endif

#endif /// LIBDENG_REFRESH_DATA_H
