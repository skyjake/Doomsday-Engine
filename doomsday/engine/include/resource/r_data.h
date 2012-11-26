/**\file r_data.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Data structures for refresh.
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

typedef struct {
    lumpnum_t lumpNum;
    short offX; /// block origin (always UL), which has allready
    short offY; /// Accounted for the patch's internal origin
} texpatch_t;

#define TXDF_NODRAW         0x0001 /// Not to be drawn.
#define TXDF_CUSTOM         0x0002 /// Definition does not define a texture that originates from the current game.

// Describes a rectangular texture, which is composed of one
// or more texpatch_t structures that arrange graphic patches.
typedef struct {
    ddstring_t name; ///< Percent-encoded.
    /// Size of the texture in logical pixels.
    Size2Raw size;
    short flags;
    /// Index of this resource according to the logic of the original game's indexing algorithm.
    int origIndex;
    short patchCount;
    texpatch_t* patches; // [patchcount] drawn back to front into the cached texture.
} patchcompositetex_t;

// Patch flags.
#define PF_MONOCHROME         0x1
#define PF_UPSCALE_AND_SHARPEN 0x2

typedef struct patchtex_s {
    short flags;
    /// Offset to texture origin in logical pixels.
    short offX, offY;
} patchtex_t;

#pragma pack(1)
typedef struct doompatch_header_s {
    int16_t width; /// Bounding box size.
    int16_t height;
    int16_t leftOffset; /// Pixels to the left of origin.
    int16_t topOffset; /// Pixels below the origin.
} doompatch_header_t;
#pragma pack()



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
void R_InitPatchCompositeTextures(void);
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

struct texture_s *R_CreateSkinTex(Uri const *filePath, boolean isShinySkin);

struct texture_s *R_RegisterModelSkin(char const *skin, char const *modelfn, boolean isReflection);
struct texture_s *R_FindModelSkinForResourcePath(Uri const *resourcePath);
struct texture_s *R_FindModelReflectionSkinForResourcePath(Uri const *resourcePath);

/**
 * Construct a DetailTexture according to the paramaters of the definition.
 * @note May return an existing DetailTexture if it is concluded that the
 * definition does not infer a unique DetailTexture.
 *
 * @param def  Definition describing the desired DetailTexture.
 * @return  DetailTexture inferred from the definition or @c NULL if invalid.
 */
struct texture_s *R_CreateDetailTextureFromDef(ded_detailtexture_t const *def);
struct texture_s *R_FindDetailTextureForResourcePath(Uri const *resourcePath);

struct texture_s *R_CreateLightMap(Uri const *resourcePath);
struct texture_s *R_FindLightMapForResourcePath(Uri const *resourcePath);

struct texture_s *R_CreateFlareTexture(Uri const *resourcePath);
struct texture_s *R_FindFlareTextureForResourcePath(Uri const *resourcePath);

struct texture_s *R_CreateReflectionTexture(Uri const *resourcePath);
struct texture_s *R_FindReflectionTextureForResourcePath(Uri const *resourcePath);

struct texture_s *R_CreateMaskTexture(Uri const *resourcePath, Size2Raw const *size);
struct texture_s *R_FindMaskTextureForResourcePath(Uri const *resourcePath);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_REFRESH_DATA_H
