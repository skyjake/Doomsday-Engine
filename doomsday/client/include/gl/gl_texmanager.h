/** @file gl_texmanager.h  GL-Texture Management.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_GL_TEXMANAGER_H
#define DENG_CLIENT_GL_TEXMANAGER_H

#ifndef __CLIENT__
#  error "GL Texture Manager only exists in the Client"
#endif

#ifndef __cplusplus
#  error "gl/gl_texmanager.h requires C++"
#endif

#include "api_gl.h"
#include "resource/image.h"
#include "resource/rawtexture.h"
#include "TextureVariantSpec"
#include "uri.hh"

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

void GL_InitTextureManager();

void GL_ShutdownTextureManager();

/**
 * Call this if a full cleanup of the textures is required (engine update).
 */
void GL_ResetTextureManager();

void GL_TexReset();

void GL_PruneTextureVariantSpecifications();

/**
 * Prepare a TextureVariantSpecification according to usage context. If incomplete
 * context information is supplied, suitable defaults are chosen in their place.
 *
 * @param tc  Usage context.
 * @param flags  @ref textureVariantSpecificationFlags
 * @param border  Border size in pixels (all edges).
 * @param tClass  Color palette translation class.
 * @param tMap  Color palette translation map.
 *
 * @return  A rationalized and valid TextureVariantSpecification.
 */
texturevariantspecification_t &GL_TextureVariantSpec(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass,
    int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha);

/**
 * Prepare a TextureVariantSpecification according to usage context. If incomplete
 * context information is supplied, suitable defaults are chosen in their place.
 *
 * @return  A rationalized and valid TextureVariantSpecification.
 */
texturevariantspecification_t &GL_DetailTextureSpec(float contrast);

/*
 * Here follows miscellaneous routines currently awaiting refactoring into the
 * revised texture management APIs.
 */

void GL_LoadLightingSystemTextures();
void GL_ReleaseAllLightingSystemTextures();

DGLuint GL_PrepareLSTexture(lightingtexid_t which);

void GL_LoadFlareTextures();
void GL_ReleaseAllFlareTextures();

DGLuint GL_PrepareFlaremap(de::Uri const &resourceUri);
DGLuint GL_PrepareSysFlaremap(flaretexid_t which);


DGLuint GL_PrepareRawTexture(rawtex_t &rawTex);

/// Release all textures used with 'Raw Images'.
void GL_ReleaseTexturesForRawImages();

/**
 * Change the GL minification filter for all prepared "raw" textures.
 */
void GL_SetRawTexturesMinFilter(int minFilter);


DGLuint GL_NewTextureWithParams(dgltexformat_t format, int width, int height, uint8_t const *pixels, int flags);
DGLuint GL_NewTextureWithParams(dgltexformat_t format, int width, int height, uint8_t const *pixels, int flags,
                                int grayMipmap, int minFilter, int magFilter, int anisoFilter, int wrapS, int wrapT);

#endif // DENG_CLIENT_GL_TEXMANAGER_H
