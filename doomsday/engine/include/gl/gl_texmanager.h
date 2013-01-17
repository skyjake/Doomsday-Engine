/**\file gl_texmanager.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * GL-Texture Management.
 *
 * 'Runtime' textures are not loaded until precached or actually needed.
 * They may be cleared, in which case they will be reloaded when needed.
 *
 * 'System' textures are loaded at startup and remain in memory all the
 * time. After clearing they must be manually reloaded.
 */

#ifndef LIBDENG_GL_TEXMANAGER_H
#define LIBDENG_GL_TEXMANAGER_H

#include "sys_opengl.h"

#include "filehandle.h"
#include "resource/r_data.h" // For flaretexid_t, lightingtexid_t, etc...
#include "resource/rawtexture.h"
#include "resource/texture.h"
#include "resource/texturevariantspec.h"

struct image_s;
struct texturecontent_s;
struct texturevariant_s;

#define TEXQ_BEST               8
#define MINTEXWIDTH             8
#define MINTEXHEIGHT            8

DENG_EXTERN_C int ratioLimit;
DENG_EXTERN_C int mipmapping, filterUI, texQuality, filterSprites;
DENG_EXTERN_C int texMagMode, texAniso;
DENG_EXTERN_C int useSmartFilter;
DENG_EXTERN_C int texMagMode;
DENG_EXTERN_C int monochrome, upscaleAndSharpenPatches;
DENG_EXTERN_C int glmode[6];
DENG_EXTERN_C boolean fillOutlines;
DENG_EXTERN_C boolean noHighResTex;
DENG_EXTERN_C boolean noHighResPatches;
DENG_EXTERN_C boolean highResWithPWAD;
DENG_EXTERN_C byte loadExtAlways;

#ifdef __cplusplus
extern "C" {
#endif

void GL_TexRegister(void);

/**
 * Called before real texture management is up and running, during engine
 * early init.
 */
void GL_EarlyInitTextureManager(void);

void GL_InitTextureManager(void);

void GL_ShutdownTextureManager(void);

/**
 * Call this if a full cleanup of the textures is required (engine update).
 */
void GL_ResetTextureManager(void);

void GL_TexReset(void);

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

void GL_UpdateTexParams(int mipMode);

/**
 * Updates the textures, flats and sprites (gameTex) or the user
 * interface textures (patches and raw screens).
 */
void GL_SetTextureParams(int minMode, int gameTex, int uiTex);

/// Release all textures in all schemes.
void GL_ReleaseTextures(void);

/// Release all textures flagged 'runtime'.
void GL_ReleaseRuntimeTextures(void);

/// Release all textures flagged 'system'.
void GL_ReleaseSystemTextures(void);

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
void GL_ReleaseGLTexturesByTexture(struct texture_s *texture);

/**
 * Release all textures associated with the specified variant @a texture.
 */
void GL_ReleaseVariantTexture(struct texturevariant_s *texture);

/**
 * Release all variants of @a tex which match @a spec.
 *
 * @param texture  Logical Texture to process. Can be @c NULL, in which case this is a null-op.
 * @param spec  Specification to match. Comparision mode is exact and not fuzzy.
 */
void GL_ReleaseVariantTexturesBySpec(struct texture_s *tex, texturevariantspecification_t *spec);

/// Release all textures associated with the identified colorpalette @a paletteId.
void GL_ReleaseTexturesByColorPalette(colorpaletteid_t paletteId);

/// Release all textures used with 'Raw Images'.
void GL_ReleaseTexturesForRawImages(void);

void GL_PruneTextureVariantSpecifications(void);

/**
 * Prepares all the system textures (dlight, ptcgens).
 */
void GL_LoadSystemTextures(void);

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
 * @note Can be rather time-consuming due to forced scaling operations and
 * the generation of mipmaps.
 */
void GL_UploadTextureContent(struct texturecontent_s const *content);

uint8_t *GL_LoadImage(struct image_s *img, char const *filePath);
uint8_t *GL_LoadImageStr(struct image_s *img, ddstring_t const *filePath);

TexSource GL_LoadExtTexture(struct image_s *image, char const *name, gfxmode_t mode);

GLint GL_MinFilterForVariantSpec(variantspecification_t const *spec);
GLint GL_MagFilterForVariantSpec(variantspecification_t const *spec);
int GL_LogicalAnisoLevelForVariantSpec(variantspecification_t const *spec);

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
 * @return  A rationalized and valid TextureVariantSpecification or @c NULL if out of memory.
 */
texturevariantspecification_t *GL_TextureVariantSpecificationForContext(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass,
    int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha);

/**
 * Prepare a TextureVariantSpecification according to usage context. If incomplete
 * context information is supplied, suitable defaults are chosen in their place.
 *
 * @return  A rationalized and valid TextureVariantSpecification or @c NULL if out of memory.
 */
texturevariantspecification_t *GL_DetailTextureVariantSpecificationForContext(
    float contrast);

/**
 * Output a human readable representation of TextureVariantSpecification to console.
 *
 * @param spec  Specification to echo.
 */
void GL_PrintTextureVariantSpecification(texturevariantspecification_t const *spec);

/**
 * Dump the pixel data of @a img to an ARGB32 at @a filePath.
 *
 * @param img           The image to be dumped. A temporary copy will be made if
 *                      the pixel data is not already in either ARGB32 or ABGR32
 *                      formats.
 * @param filePath      Location to write the new file. If an extension is not
 *                      specified the file will be in PNG format.
 *
 * @return @c true= Dump was successful.
 */
boolean GL_DumpImage(struct image_s const *img, char const *filePath);

/*
 * Here follows miscellaneous routines currently awaiting refactoring into the
 * revised texture management APIs.
 */

/**
 * Set mode to 2 to include an alpha channel. Set to 3 to make the actual pixel
 * colors all white.
 */
DGLuint GL_PrepareExtTexture(char const *name, gfxmode_t mode, int useMipmap,
    int minFilter, int magFilter, int anisoFilter, int wrapS, int wrapT, int flags);

DGLuint GL_PrepareSysFlaremap(flaretexid_t flare);
DGLuint GL_PrepareLightmap(Uri const *path);
DGLuint GL_PrepareLSTexture(lightingtexid_t which);
DGLuint GL_PrepareRawTexture(rawtex_t *rawTex);

struct texturevariant_s *GL_PreparePatchTexture2(struct texture_s *tex, int wrapS, int wrapT);
struct texturevariant_s *GL_PreparePatchTexture(struct texture_s *tex);

/**
 * Attempt to locate and prepare a flare texture.
 * Somewhat more complicated than it needs to be due to the fact there
 * are two different selection methods.
 *
 * @param name          Name of a flare texture or "0" to "4".
 * @param oldIdx        Old method of flare texture selection, by id.
 *
 * @return  @c 0= Use the automatic selection logic.
 */
DGLuint GL_PrepareFlareTexture(Uri const *path, int oldIdx);

DGLuint GL_NewTextureWithParams(dgltexformat_t format, int width, int height, uint8_t const *pixels, int flags);
DGLuint GL_NewTextureWithParams2(dgltexformat_t format, int width, int height, uint8_t const *pixels, int flags,
                                 int grayMipmap, int minFilter, int magFilter, int anisoFilter, int wrapS, int wrapT);

#ifdef __cplusplus
} // extern "C"

/// Result of a request to prepare a Texture::Variant
typedef enum {
    PTR_NOTFOUND = 0,       /// Failed. No suitable variant could be found/prepared.
    PTR_FOUND,              /// Success. Reusing a cached resource.
    PTR_UPLOADED_ORIGINAL,  /// Success. Prepared and cached using an original-game resource.
    PTR_UPLOADED_EXTERNAL   /// Success. Prepared and cached using an external-replacement resource.
} preparetextureresult_t;

/**
 * Prepare variant @a texture for render.
 *
 * @param texture  Texture variant to be prepared.
 * @return  Logical result.
 */
preparetextureresult_t GL_PrepareTexture(de::Texture::Variant &texture);

/**
 * Choose/create a variant of @a texture which fulfills @a spec and then
 * immediately prepare it for render. If a suitable variant cannot be found a
 * new one will be constructed and prepared.
 *
 * @note If a cache miss occurs texture content data may need to be uploaded
 * to GL to satisfy the variant specification. However the actual upload will
 * be deferred if possible. This has the side effect that although the variant
 * is considered "prepared", attempting to render using the associated texture
 * will result in "uninitialized" white texels being used instead.
 *
 * @param texture   Base texture from which to derive a variant.
 * @param spec      Specification for the derivation of @a texture.
 * @param returnOutcome  If not @c NULL the logical result is written back here.
 *
 * @return  The prepared texture variant if successful; otherwise @c 0.
 */
de::Texture::Variant *GL_PrepareTexture(de::Texture &texture, texturevariantspecification_t &spec,
                                        preparetextureresult_t *returnOutcome = 0);
#endif

#endif /* LIBDENG_GL_TEXMANAGER_H */
