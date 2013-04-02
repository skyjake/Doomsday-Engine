/** @file gl_texmanager.h GL-Texture Management.
 *
 * @em Runtime textures are not loaded until precached or actually needed.
 * They may be cleared, in which case they will be reloaded when needed.
 *
 * @em System textures are loaded at startup and remain in memory all the
 * time. After clearing they must be manually reloaded.
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

#ifndef LIBDENG_GL_TEXMANAGER_H
#define LIBDENG_GL_TEXMANAGER_H

#ifndef __CLIENT__
#  error "GL Texture Manager only exists in the Client"
#endif

#ifndef __cplusplus
#  error "gl/gl_texmanager.h requires C++"
#endif

#include "sys_opengl.h"

#include "gl/texturecontent.h"
#include "resource/image.h"
#include "resource/r_data.h" // For flaretexid_t, lightingtexid_t, etc...
#include "resource/rawtexture.h"
#include "Texture"
#include "TextureVariantSpec"

#define TEXQ_BEST               8
#define MINTEXWIDTH             8
#define MINTEXHEIGHT            8

DENG_EXTERN_C int ratioLimit;
DENG_EXTERN_C int mipmapping, filterUI, texQuality, filterSprites;
DENG_EXTERN_C int texMagMode, texAniso;
DENG_EXTERN_C int useSmartFilter;
DENG_EXTERN_C int texMagMode;
DENG_EXTERN_C int glmode[6];
DENG_EXTERN_C boolean fillOutlines;
DENG_EXTERN_C boolean noHighResTex;
DENG_EXTERN_C boolean noHighResPatches;
DENG_EXTERN_C boolean highResWithPWAD;
DENG_EXTERN_C byte loadExtAlways;

void GL_TexRegister();

void GL_InitTextureManager();

void GL_ShutdownTextureManager();

/**
 * Call this if a full cleanup of the textures is required (engine update).
 */
void GL_ResetTextureManager();

void GL_TexReset();

/**
 * Determine the optimal size for a texture. Usually the dimensions are scaled
 * upwards to the next power of two.
 *
 * @param noStretch  If @c true, the stretching can be skipped.
 * @param isMipMapped  If @c true, we will require mipmaps (this has an effect
 *     on the optimal size).
 */
boolean GL_OptimalTextureSize(int width, int height, boolean noStretch, boolean isMipMapped,
    int *optWidth, int *optHeight);

/**
 * Change the GL minification filter for all prepared textures.
 */
void GL_SetAllTexturesMinFilter(int minFilter);

/**
 * Change the GL minification filter for all prepared "raw" textures.
 */
void GL_SetRawTexturesMinFilter(int minFilter);

/// Release all textures in all schemes.
void GL_ReleaseTextures();

/// Release all textures flagged 'runtime'.
void GL_ReleaseRuntimeTextures();

/// Release all textures flagged 'system'.
void GL_ReleaseSystemTextures();

/**
 * Release all textures in the identified scheme.
 *
 * @param schemeName  Symbolic name of the texture scheme to process.
 */
void GL_ReleaseTexturesByScheme(char const *schemeName);

/**
 * Release all textures associated with the specified @a texture.
 * @param texture  Logical Texture. Can be @c NULL, in which case this is a null-op.
 */
void GL_ReleaseGLTexturesByTexture(de::Texture &texture);

/**
 * Release all textures associated with the specified variant @a texture.
 */
void GL_ReleaseVariantTexture(de::TextureVariant &texture);

/**
 * Release all variants of @a tex which match @a spec.
 *
 * @param texture  Logical Texture to process. Can be @c NULL, in which case this is a null-op.
 * @param spec  Specification to match. Comparision mode is exact and not fuzzy.
 */
void GL_ReleaseVariantTexturesBySpec(de::Texture &texture, texturevariantspecification_t &spec);

/// Release all textures associated with the identified colorpalette @a paletteId.
void GL_ReleaseTexturesByColorPalette(colorpaletteid_t paletteId);

/// Release all textures used with 'Raw Images'.
void GL_ReleaseTexturesForRawImages();

void GL_PruneTextureVariantSpecifications();

/**
 * Prepares all the system textures (dlight, ptcgens).
 */
void GL_LoadSystemTextures();

/**
 * @param glFormat  Identifier of the desired GL texture format.
 * @param loadFormat  Identifier of the GL texture format used during upload.
 * @param pixels  Texture pixel data to be uploaded.
 * @param width  Width of the texture in pixels.
 * @param height  Height of the texture in pixels.
 * @param genMipmaps  If negative sets a specific mipmap level, e.g.:
 *      @c -1, means mipmap level 1.
 *
 * @return  @c true iff successful.
 */
boolean GL_UploadTexture(int glFormat, int loadFormat, uint8_t const *pixels,
    int width, int height, int genMipmaps);

/**
 * @param glFormat  Identifier of the desired GL texture format.
 * @param loadFormat  Identifier of the GL texture format used during upload.
 * @param pixels  Texture pixel data to be uploaded.
 * @param width  Width of the texture in pixels.
 * @param height  Height of the texture in pixels.
 * @param grayFactor  Strength of the blend where @c 0:none @c 1:full.
 *
 * @return  @c true iff successful.
 */
boolean GL_UploadTextureGrayMipmap(int glFormat, int loadFormat, uint8_t const *pixels,
    int width, int height, float grayFactor);

/**
 * Methods of uploading GL data.
 *
 * @ingroup GL
 */
enum GLUploadMethod
{
    /// Upload the data immediately.
    Immediate,

    /// Defer the data upload until convenient.
    Deferred
};

/**
 * Returns the chosen method for uploading the given texture @a content.
 */
GLUploadMethod GL_ChooseUploadMethod(texturecontent_t const &content);

/**
 * Prepare the texture content @a c, using the given image in accordance with
 * the supplied specification. The image data will be transformed in-place.
 *
 * @param c             Texture content to be completed.
 * @param glTexName     GL name for the texture we intend to upload.
 * @param image         Source image containing the pixel data to be prepared.
 * @param spec          Specification describing any transformations which
 *                      should be applied to the image.
 *
 * @param textureManifest  Manifest for the logical texture being prepared.
 *                      (for informational purposes, i.e., logging)
 */
void GL_PrepareTextureContent(texturecontent_t &c, DGLuint glTexName,
    image_t &image, texturevariantspecification_t const &spec,
    de::TextureManifest const &textureManifest);

/**
 * @param method  GL upload method. By default the upload is deferred.
 *
 * @note Can be rather time-consuming due to forced scaling operations and
 * the generation of mipmaps.
 */
void GL_UploadTextureContent(texturecontent_t const &content,
                             GLUploadMethod method = Deferred);



GLint GL_MinFilterForVariantSpec(variantspecification_t const &spec);
GLint GL_MagFilterForVariantSpec(variantspecification_t const &spec);
int GL_LogicalAnisoLevelForVariantSpec(variantspecification_t const &spec);

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
texturevariantspecification_t &GL_DetailTextureSpec(
    float contrast);

/*
 * Here follows miscellaneous routines currently awaiting refactoring into the
 * revised texture management APIs.
 */

DGLuint GL_PrepareSysFlaremap(flaretexid_t which);
DGLuint GL_PrepareLSTexture(lightingtexid_t which);

DGLuint GL_PrepareRawTexture(rawtex_t &rawTex);

DGLuint GL_NewTextureWithParams(dgltexformat_t format, int width, int height, uint8_t const *pixels, int flags);
DGLuint GL_NewTextureWithParams(dgltexformat_t format, int width, int height, uint8_t const *pixels, int flags,
                                int grayMipmap, int minFilter, int magFilter, int anisoFilter, int wrapS, int wrapT);

/// @todo Move into image_t
uint8_t *GL_LoadImage(image_t &image, de::String nativePath);

/// @todo Move into image_t
TexSource GL_LoadExtImage(image_t &image, char const *searchPath, gfxmode_t mode);

/// @todo Move into image_t
TexSource GL_LoadSourceImage(image_t &image, de::Texture const &tex,
    texturevariantspecification_t const &spec);

#endif /* LIBDENG_GL_TEXMANAGER_H */
