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
struct gltexture_s;
struct gltexturevariant_s;

typedef enum {
    GLT_ANY = -1,
    GLT_FIRST = 0,
    GLT_SYSTEM = GLT_FIRST, // system texture e.g., the "missing" texture.
    GLT_FLAT,
    GLT_PATCHCOMPOSITE,
    GLT_PATCH,
    GLT_SPRITE,
    GLT_DETAIL,
    GLT_SHINY,
    GLT_MASK,
    GLT_MODELSKIN,
    GLT_MODELSHINYSKIN,
    GLT_LIGHTMAP,
    GLT_FLARE,
    NUM_GLTEXTURE_TYPES
} gltexture_type_t;

#define VALID_GLTEXTURE_TYPE(t)     ((t) >= GLT_FIRST && (t) < NUM_GLTEXTURE_TYPES)

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
void GL_ClearTextures(void);
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

byte GL_LoadDetailTextureLump(struct image_s* image, const struct gltexture_s* tex, void* context);
byte GL_LoadFlatLump(struct image_s* image, const struct gltexture_s* tex, void* context);
byte GL_LoadSpriteLump(struct image_s* image, const struct gltexture_s* tex, void* context);
byte GL_LoadDoomPatchLump(struct image_s* image, const struct gltexture_s* tex, void* context);

byte GL_LoadDoomTexture(struct image_s* image, const struct gltexture_s* tex, void* context);

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

void GL_ReleaseGLTexture(gltextureid_t id);

const struct gltexture_s* GL_GetGLTexture(gltextureid_t id);
const struct gltexturevariant_s* GL_PrepareGLTexture(gltextureid_t id, void* context, byte* result);

uint GL_TextureIndexForName(const char* name, texturenamespaceid_t texNamespace);

const struct gltexture_s* GL_CreateGLTexture(const char* name, uint index, gltexture_type_t type);

const struct gltexture_s* GL_GetGLTextureByName(const char* name, texturenamespaceid_t texNamespace);

const struct gltexture_s* GL_GetGLTextureByIndex(int index, texturenamespaceid_t texNamespace);

/**
 * Updates the minification mode of ALL gltextures.
 *
 * @param minMode The DGL minification mode to set.
 */
void GL_SetAllGLTexturesMinMode(int minMode);

/**
 * Deletes all OpenGL texture instances for ALL gltextures.
 */
void GL_DeleteAllTexturesForGLTextures(gltexture_type_t);

#endif /* LIBDENG_TEXTURE_MANAGER_H */
