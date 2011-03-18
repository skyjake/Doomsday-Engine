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

#include "r_data.h"
#include "r_model.h"
#include "gl_model.h"
#include "gl_defer.h"

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
extern boolean allowMaskedTexEnlarge;
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
void GL_DestroyTextures(void);

void GL_TexReset(void);

/**
 * Prepares all the system textures (dlight, ptcgens).
 */
void GL_LoadSystemTextures(void);

void GL_ClearTextureMemory(void);

/**
 * Runtime textures are not loaded until precached or actually needed.
 * They may be cleared, in which case they will be reloaded when needed.
 */
void GL_ClearRuntimeTextures(void);

/**
 * System textures are loaded at startup and remain in memory all the time.
 * After clearing they must be manually reloaded.
 */
void GL_ClearSystemTextures(void);

/**
 * To save texture memory, delete all raw image textures. Raw images are
 * used as interlude backgrounds, title screens, etc. Called from
 * DD_SetupLevel.
 */
void GL_DeleteRawImages(void);

/**
 * Called when changing the value of any cvar affecting texture quality which
 * in turn calls GL_TexReset. Added to remove the need for reseting manually.
 */
void GL_DoTexReset(const cvar_t* /*cvar*/);

/**
 * Called when changing the value of the texture gamma cvar.
 */
void GL_DoUpdateTexGamma(const cvar_t* /*cvar*/);

/**
 * Called when changing the value of any cvar affecting texture quality which
 * can be actioned by simply changing texture paramaters i.e. does not require
 * flushing GL textures).
 */
void GL_DoUpdateTexParams(const cvar_t* /*cvar*/);

void GL_UpdateTexParams(int mipmode);

/**
 * Updates the textures, flats and sprites (gameTex) or the user
 * interface textures (patches and raw screens).
 */
void GL_SetTextureParams(int minMode, int gameTex, int uiTex);

DGLuint GL_UploadTexture(const uint8_t* data, int width, int height,
    boolean flagAlphaChannel, boolean flagGenerateMipmaps, boolean flagRgbData,
    boolean flagNoStretch, boolean flagNoSmartFilter, int minFilter, int magFilter,
    int anisoFilter, int wrapS, int wrapT, int otherFlags);

/**
 * Can be rather time-consuming due to scaling operations and mipmap
 * generation. The texture parameters will NOT be set here.
 *
 * @return  The name of the texture.
 */
DGLuint GL_UploadTexture2(const struct texturecontent_s* content);

/**
 * @return  @c true iff this operation was deferred.
 */
boolean GL_NewTexture(const struct texturecontent_s* content);

DGLuint GL_NewTextureWithParams(dgltexformat_t format, int width, int height,
    const uint8_t* pixels, int flags);
DGLuint GL_NewTextureWithParams2(dgltexformat_t format, int width, int height,
    const uint8_t* pixels, int flags, int grayMipmap, int minFilter, int magFilter,
    int anisoFilter, int wrapS, int wrapT);
DGLuint GL_NewTextureWithParams3(dgltexformat_t format, int width, int height,
    const uint8_t* pixels, int flags, int grayMipmap, int minFilter, int magFilter,
    int anisoFilter, int wrapS, int wrapT, boolean* didDefer);

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

byte GL_LoadDetailTextureLump(struct image_s* image, const struct texture_s* tex);

byte GL_LoadFlatLump(struct image_s* image, const struct texture_s* tex);

byte GL_LoadSpriteLump(struct image_s* image, const struct texture_s* tex,
    boolean prepareForPSprite, int tclass, int tmap, int border);

byte GL_LoadDoomPatchLump(struct image_s* image, const struct texture_s* tex,
    boolean scaleSharp);

byte GL_LoadDoomTexture(struct image_s* image, const struct texture_s* tex,
    boolean prepareForSkySphere, boolean zeroMask);

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

DGLuint GL_GetLightMapTexture(const dduri_t* path);

/**
 * Attempt to locate and prepare a flare texture.
 * Somewhat more complicated than it needs to be due to the fact there
 * are two different selection methods.
 *
 * @param name  Name of a flare texture or "0" to "4".
 * @param oldIdx  Old method of flare texture selection, by id.
 */
DGLuint GL_GetFlareTexture(const dduri_t* path, int oldIdx);

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
 * Output a human readable representation of TextureVariantSpecification
 * to console output.
 *
 * @param spec  Specification to echo.
 */
void GL_PrintTextureVariantSpecification(const struct texturevariantspecification_s* spec);

/**
 * Prepare a TextureVariantSpecification according to usage context.
 * If incomplete context information is supplied, suitable defaults are
 * chosen in their place.
 *
 * @return  Ptr to a rationalized and valid TextureVariantSpecification
 *      or @c NULL if out of memory.
 */
struct texturevariantspecification_s* GL_TextureVariantSpecificationForContext(
    const struct texture_s* tex, void* context);

struct texturevariant_s* GL_FindSuitableTextureVariant(struct texture_s* tex,
    const struct texturevariantspecification_s* spec);

void GL_ReleaseGLTexturesForTexture(struct texture_s* tex);

struct texture_s* GL_ToTexture(textureid_t id);

/**
 * Attempt to prepare (upload to GL) an instance of Texture which fulfills
 * the variant specification defined by the usage context.
 *
 * @param context  Usage-specific context data (if any).
 * @param result  Result of this process:
 *      @c 0== Failed: No suitable variant could be found/prepared.
 *      @c 1== Success: Suitable variant prepared from an original resource.
 *      @c 2== Success: Suitable variant prepared from a replacement resource.
 * @return  Prepared variant if successful else @c NULL.
 */
const struct texturevariant_s* GL_PrepareTexture(textureid_t id, void* context,
    byte* result);

const struct texture_s* GL_CreateTexture(const char* name, uint index, texturenamespaceid_t texNamespace);

const struct texture_s* GL_TextureByUri2(const dduri_t* uri, boolean silent);
const struct texture_s* GL_TextureByUri(const dduri_t* uri);
const struct texture_s* GL_TextureByIndex(int index, texturenamespaceid_t texNamespace);

uint GL_TextureIndexForUri2(const dduri_t* uri, boolean silent);
uint GL_TextureIndexForUri(const dduri_t* uri);

/**
 * Change the GL minification filter for all prepared TextureVariants.
 */
void GL_SetAllTexturesMinFilter(int minFilter);

/**
 * Releases all GL texture objects for all prepared TextureVariants.
 */
void GL_ReleaseGLTexturesByNamespace(texturenamespaceid_t texNamespace);

#endif /* LIBDENG_TEXTURE_MANAGER_H */
