/**\file gl_texmanager.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Texture Management
 */

#ifndef LIBDENG_TEXTURE_MANAGER_H
#define LIBDENG_TEXTURE_MANAGER_H

#include "sys_file.h"
#include "r_data.h"
#include "r_model.h"
#include "gl_model.h"
#include "gl_defer.h"
#include "texturevariantspecification.h"

#define TEXQ_BEST               8
#define MINTEXWIDTH             8
#define MINTEXHEIGHT            8

struct image_s;
struct texturecontent_s;
struct texture_s;
struct texturevariant_s;
struct texturevariantspecification_s;

extern int ratioLimit;
extern int mipmapping, filterUI, texQuality, filterSprites;
extern int texMagMode, texAniso;
extern int useSmartFilter;
extern int texMagMode;
extern int monochrome, upscaleAndSharpenPatches;
extern int glmode[6];
extern boolean fillOutlines;
extern boolean noHighResTex;
extern boolean noHighResPatches;
extern boolean highResWithPWAD;
extern byte loadExtAlways;

void GL_TexRegister(void);

/**
 * Called before real texture management is up and running, during engine
 * early init.
 */
void GL_EarlyInitTextureManager(void);

void GL_InitTextureManager(void);

/**
 * Call this if a full cleanup of the textures is required (engine update).
 */
void GL_ResetTextureManager(void);

void GL_ShutdownTextureManager(void);

void GL_TexReset(void);

/**
 * Prepares all the system textures (dlight, ptcgens).
 */
void GL_LoadSystemTextures(void);

void GL_ClearTextureMemory(void);

void GL_PruneTextureVariantSpecifications(void);

/**
 * Runtime textures are not loaded until precached or actually needed.
 * They may be cleared, in which case they will be reloaded when needed.
 */
void GL_ReleaseRuntimeTextures(void);

/**
 * System textures are loaded at startup and remain in memory all the time.
 * After clearing they must be manually reloaded.
 */
void GL_ReleaseSystemTextures(void);

/**
 * To save texture memory, delete all raw image textures. Raw images are
 * used as interlude backgrounds, title screens, etc. Called from
 * DD_SetupLevel.
 */
void GL_ReleaseTexturesForRawImages(void);

void GL_DestroyTextures(void);

void GL_DestroyRuntimeTextures(void);

void GL_DestroySystemTextures(void);

/**
 * Called when changing the value of any cvar affecting texture quality which
 * in turn calls GL_TexReset. Added to remove the need for reseting manually.
 */
void GL_DoTexReset(void);

/**
 * Called when changing the value of the texture gamma cvar.
 */
void GL_DoUpdateTexGamma(void);

/**
 * Called when changing the value of any cvar affecting texture quality which
 * can be actioned by simply changing texture paramaters i.e. does not require
 * flushing GL textures).
 */
void GL_DoUpdateTexParams(void);

void GL_UpdateTexParams(int mipmode);

/**
 * Updates the textures, flats and sprites (gameTex) or the user
 * interface textures (patches and raw screens).
 */
void GL_SetTextureParams(int minMode, int gameTex, int uiTex);

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
boolean GL_TexImage(int glFormat, int loadFormat, const uint8_t* pixels,
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
boolean GL_TexImageGrayMipmap(int glFormat, int loadFormat, const uint8_t* pixels,
    int width, int height, float grayFactor);

/**
 * \note Can be rather time-consuming due to forced scaling operations and
 * the generation of mipmaps.
 *
 * @return  Name of the resultant GL texture object.
 */
DGLuint GL_UploadTextureContent(const struct texturecontent_s* content);

DGLuint GL_UploadTextureWithParams(const uint8_t* pixels, int width, int height,
    dgltexformat_t texFormat, boolean flagGenerateMipmaps,
    boolean flagNoStretch, boolean flagNoSmartFilter, int minFilter,
    int magFilter, int anisoFilter, int wrapS, int wrapT, int otherFlags);

DGLuint GL_NewTextureWithParams(dgltexformat_t format, int width, int height,
    const uint8_t* pixels, int flags);
DGLuint GL_NewTextureWithParams2(dgltexformat_t format, int width, int height,
    const uint8_t* pixels, int flags, int grayMipmap, int minFilter, int magFilter,
    int anisoFilter, int wrapS, int wrapT);

/**
 * @return  The outcome:
 *     0 = not prepared
 *     1 = found and prepared a lump resource.
 *     2 = found and prepared an external resource.
 */
byte GL_LoadRawTex(struct image_s* image, const rawtex_t* r);

/**
 * @return  The outcome:
 *     0 = not prepared
 *     2 = found and prepared an external resource.
 */
byte GL_LoadExtTexture(struct image_s* image, const char* name, gfxmode_t mode);
byte GL_LoadExtTextureEX(struct image_s* image, const char* searchPath,
    const char* optionalSuffix, boolean silent);

byte GL_LoadFlatLump(struct image_s* image, DFILE* file, const char* lumpName);

byte GL_LoadPatchLumpAsPatch(struct image_s* image, DFILE* file, lumpnum_t lumpNum, int tclass,
    int tmap, int border, patchtex_t* patchTex);
byte GL_LoadPatchLumpAsSprite(struct image_s* image, DFILE* file, lumpnum_t lumpNum, int tclass,
    int tmap, int border, spritetex_t* spriteTex);

byte GL_LoadDetailTextureLump(struct image_s* image, DFILE* file, const char* lumpName);

byte GL_LoadPatchComposite(struct image_s* image, const struct texture_s* tex);
byte GL_LoadPatchCompositeAsSky(struct image_s* image, const struct texture_s* tex,
    boolean zeroMask);

/**
 * Set mode to 2 to include an alpha channel. Set to 3 to make the
 * actual pixel colors all white.
 */
DGLuint GL_PrepareExtTexture(const char* name, gfxmode_t mode, int useMipmap,
    int minFilter, int magFilter, int anisoFilter, int wrapS, int wrapT,
    int flags);

/**
 * Prepare a texture used in the lighting system. 'which' must be one
 * of the LST_* constants.
 */
DGLuint GL_PrepareLSTexture(lightingtexid_t which);

DGLuint GL_PrepareSysFlareTexture(flaretexid_t flare);

/**
 * Returns the OpenGL name of the texture.
 */
DGLuint GL_PreparePatch(patchtex_t* patch);

/**
 * Returns the OpenGL name of the texture.
 */
DGLuint GL_PrepareRawTex(rawtex_t* rawTex);

DGLuint GL_GetLightMapTexture(const Uri* path);

/**
 * Attempt to locate and prepare a flare texture.
 * Somewhat more complicated than it needs to be due to the fact there
 * are two different selection methods.
 *
 * @param name  Name of a flare texture or "0" to "4".
 * @param oldIdx  Old method of flare texture selection, by id.
 */
DGLuint GL_GetFlareTexture(const Uri* path, int oldIdx);

/**
 * Determine the optimal size for a texture. Usually the dimensions are
 * scaled upwards to the next power of two.
 *
 * @param noStretch  If @c true, the stretching can be skipped.
 * @param isMipMapped  If @c true, we will require mipmaps (this has an
 *      effect on the optimal size).
 */
boolean GL_OptimalTextureSize(int width, int height, boolean noStretch,
    boolean isMipMapped, int* optWidth, int* optHeight);

/**
 * Compare the given TextureVariantSpecifications and determine whether
 * they can be considered equal (dependent on current engine state and
 * the available features of the GL implementation).
 */
int GL_CompareTextureVariantSpecifications(const texturevariantspecification_t* a,
    const texturevariantspecification_t* b);

/**
 * Prepare a TextureVariantSpecification according to usage context.
 * If incomplete context information is supplied, suitable defaults are
 * chosen in their place.
 *
 * @param tc  Usage context.
 * @param flags  @see textureVariantSpecificationFlags
 * @param border  Border size in pixels (all edges).
 * @param tClass  Color palette translation class.
 * @param tMap  Color palette translation map.
 *
 * @return  Ptr to a rationalized and valid TextureVariantSpecification
 *      or @c NULL if out of memory.
 */
texturevariantspecification_t* GL_TextureVariantSpecificationForContext(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass,
    int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha);

/**
 * Prepare a TextureVariantSpecification according to usage context.
 * If incomplete context information is supplied, suitable defaults are
 * chosen in their place.
 *
 * @return  Ptr to a rationalized and valid TextureVariantSpecification
 *      or @c NULL if out of memory.
 */
texturevariantspecification_t* GL_DetailTextureVariantSpecificationForContext(
    float contrast);

/**
 * Output a human readable representation of TextureVariantSpecification
 * to console output.
 *
 * @param spec  Specification to echo.
 */
void GL_PrintTextureVariantSpecification(const texturevariantspecification_t* spec);

void GL_ReleaseGLTexturesForTexture(struct texture_s* tex);

/**
 * Given a texture identifier retrieve the associated texture.
 * @return  Found Texture object else @c NULL
 */
struct texture_s* GL_ToTexture(textureid_t id);

/// Result of a request to prepare a TextureVariant
typedef enum {
    PTR_NOTFOUND = 0,       /// Failed. No suitable variant could be found/prepared.
    PTR_FOUND,              /// Success. Reusing a cached resource.
    PTR_UPLOADED_ORIGINAL,  /// Success. Prepared and cached using an original-game resource.
    PTR_UPLOADED_EXTERNAL,  /// Success. Prepared and cached using an external-replacement resource.
} preparetextureresult_t;

/**
 * Attempt to prepare a variant of Texture which fulfills the specification
 * defined by the usage context. If a suitable variant cannot be found a new
 * one will be constructed and prepared.
 *
 * \note If a cache miss occurs texture content data may need to be uploaded
 * to GL to satisfy the variant specification. However the actual upload will
 * be deferred if possible. This has the side effect that although the variant
 * is considered "prepared", attempting to render using the associated texture
 * will result in "uninitialized" white texels being used instead.
 *
 * @param tex  Texture from which a prepared variant is desired.
 * @param spec  Variant specification for the proposed usage context.
 * @param returnOutcome  If not @c NULL detailed result information for this
 *      process is written back here.
 *
 * @return  GL-name of the prepared variant if successful else @c 0
 */
DGLuint GL_PrepareTexture2(struct texture_s* tex,
    texturevariantspecification_t* spec, preparetextureresult_t* returnOutcome);
DGLuint GL_PrepareTexture(struct texture_s* tex,
    texturevariantspecification_t* spec);

/**
 * Same as GL_PrepareTexture(2) except for visibility of TextureVariant.
 * \todo Should not need to be public.
 */
const struct texturevariant_s* GL_PrepareTextureVariant2(struct texture_s* tex,
    texturevariantspecification_t* spec, preparetextureresult_t* returnOutcome);
const struct texturevariant_s* GL_PrepareTextureVariant(struct texture_s* tex,
    texturevariantspecification_t* spec);

const struct texture_s* GL_CreateTexture2(const char* name, uint index,
    texturenamespaceid_t texNamespace, int width, int height);
const struct texture_s* GL_CreateTexture(const char* name, uint index,
    texturenamespaceid_t texNamespace);

const struct texture_s* GL_TextureByUri2(const Uri* uri, boolean silent);
const struct texture_s* GL_TextureByUri(const Uri* uri);
const struct texture_s* GL_TextureByIndex(int index, texturenamespaceid_t texNamespace);

uint GL_TextureIndexForUri2(const Uri* uri, boolean silent);
uint GL_TextureIndexForUri(const Uri* uri);

/**
 * Given a Texture construct a new Uri reference to it.
 * \todo Do not construct. Store into a resource namespace and return a reference.
 * @return  Associated Uri.
 */
Uri* GL_NewUriForTexture(struct texture_s* tex);

/**
 * Change the GL minification filter for all prepared TextureVariants.
 */
void GL_SetAllTexturesMinFilter(int minFilter);

void GL_ReleaseGLTexturesByNamespace(texturenamespaceid_t texNamespace);
void GL_ReleaseGLTexturesByColorPalette(colorpaletteid_t paletteId);

#endif /* LIBDENG_TEXTURE_MANAGER_H */
