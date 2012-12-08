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

/**
 * @defgroup patchFlags Patch flags
 * @ingroup flags
 * @{
 */
#define PF_MONOCHROME         0x1
#define PF_UPSCALE_AND_SHARPEN 0x2
///@}

typedef struct patchtex_s
{
    /// @ref patchFlags
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

// Posts are runs of non masked source pixels.
typedef struct post_s {
    byte topOffset; // @c 0xff is the last post in a column.
    byte length;
    // Length palette indices follow.
} post_t;
#pragma pack()

// column_t is a list of 0 or more post_t, (uint8_t)-1 terminated
typedef post_t column_t;

#ifdef __cplusplus

#include <QSize>
#include <QPoint>
#include <de/IReadable>
#include <de/Reader>

struct PatchHeader : public de::IReadable
{
    /// Dimensions of the patch in texels.
    QSize dimensions;

    /// Origin offset for the patch in texels.
    QPoint origin;

    /// Implements IReadable.
    void operator << (de::Reader &from)
    {
        dint16 width, height;
        from >> width >> height;
        dimensions.setWidth(width);
        dimensions.setHeight(height);

        dint16 xOrigin, yOrigin;
        from >> xOrigin >> yOrigin;
        origin.setX(xOrigin);
        origin.setY(yOrigin);
    }
};

#endif

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

/*
 * TODO: Merge/generalize these very similar routines.
 */

struct texture_s *R_CreateSkinTex(Uri const *resourceUri, boolean isShinySkin);
struct texture_s *R_CreateLightmap(Uri const *resourceUri);
struct texture_s *R_CreateFlaremap(Uri const *resourceUri);
struct texture_s *R_CreateReflectionTexture(Uri const *resourceUri);
struct texture_s *R_CreateMaskTexture(Uri const *resourceUri, Size2Raw const *dimensions);
struct texture_s *R_CreateDetailTexture(Uri const *resourceUri);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_REFRESH_DATA_H
