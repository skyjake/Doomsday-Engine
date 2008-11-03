/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * gl_texmanager.h: Texture Management
 */

#ifndef __DOOMSDAY_TEXTURE_MANAGER_H__
#define __DOOMSDAY_TEXTURE_MANAGER_H__

#include "r_data.h"
#include "r_model.h"
#include "r_extres.h"
#include "con_decl.h"
#include "gl_model.h"
#include "gl_defer.h"

#define TEXQ_BEST               8

/**
 * This structure is used with GL_LoadImage. When it is no longer needed
 * it must be discarded with GL_DestroyImage.
 */
typedef struct image_s {
    char            fileName[256];
    int             width;
    int             height;
    int             pixelSize;
    boolean         isMasked;
    int             originalBits;  // Bits per pixel in the image file.
    byte           *pixels;
} image_t;

/**
 * Processing modes for GL_LoadGraphics.
 */
typedef enum gfxmode_e {
    LGM_NORMAL = 0,
    LGM_GRAYSCALE = 1,
    LGM_GRAYSCALE_ALPHA = 2,
    LGM_WHITE_ALPHA = 3
} gfxmode_t;

extern int      glMaxTexSize;
extern int      ratioLimit;
extern int      mipmapping, linearRaw, texQuality, filterSprites;
extern int      texMagMode, texAniso;
extern int      useSmartFilter;
extern byte     loadExtAlways;
extern int      texMagMode;
extern int      upscaleAndSharpenPatches;
extern int      glmode[6];
extern unsigned int curTex;
extern int      palLump;

void            GL_TexRegister(void);
void            GL_EarlyInitTextureManager(void);
void            GL_InitTextureManager(void);
void            GL_ShutdownTextureManager(void);
void            GL_LoadSystemTextures(boolean loadLightMaps, boolean loadFlareMaps);
void            GL_ClearTextureMemory(void);
void            GL_ClearRuntimeTextures(void);
void            GL_ClearSystemTextures(void);
void            GL_DoTexReset(cvar_t* unused);
void            GL_DoUpdateTexGamma(cvar_t* unused);
void            GL_DoUpdateTexParams(cvar_t* unused);
int             GL_InitPalettedTexture(void);
void            GL_ResetLumpTexData(void);

void            GL_BindTexture(DGLuint texname, int magMode);
void            GL_TextureFilterMode(int target, int parm);
DGLuint         GL_BindTexPatch(struct patchtex_s* p);
DGLuint         GL_GetPatchOtherPart(lumpnum_t lump);
void            GL_SetPatch(lumpnum_t lump, int wrapS, int wrapT); // No mipmaps are generated.
DGLuint         GL_BindTexRaw(struct rawtex_s* r);
DGLuint         GL_GetRawOtherPart(lumpnum_t lump);
void            GL_SetRawTex(lumpnum_t lump, int part);

void            GL_LowRes(void);
void            TranslatePatch(lumppatch_t* patch, byte* transTable);
byte*           GL_LoadImage(image_t* img, const char* imagefn,
                             boolean useModelPath);
byte*           GL_LoadImageCK(image_t* img, const char* imagefn,
                               boolean useModelPath);
void            GL_DestroyImage(image_t* img);
byte*           GL_LoadTexture(image_t* img, const char* name);
DGLuint         GL_LoadGraphics(const char* name, gfxmode_t mode);
DGLuint         GL_LoadGraphics2(resourceclass_t resClass, const char* name,
                                 gfxmode_t mode, int useMipmap, boolean clamped,
                                 int otherFlags);
DGLuint         GL_LoadGraphics3(const char* name, gfxmode_t mode,
                                 int minFilter, int magFilter, int anisoFilter,
                                 int wrapS, int wrapT, int otherFlags);
DGLuint         GL_LoadGraphics4(resourceclass_t resClass, const char* name,
                                 gfxmode_t mode, int useMipmap,
                                 int minFilter, int magFilter, int anisoFilter,
                                 int wrapS, int wrapT, int otherFlags);
DGLuint         GL_UploadTexture(byte* data, int width, int height,
                                 boolean flagAlphaChannel,
                                 boolean flagGenerateMipmaps,
                                 boolean flagRgbData,
                                 boolean flagNoStretch,
                                 boolean flagNoSmartFilter,
                                 int minFilter, int magFilter, int anisoFilter,
                                 int wrapS, int wrapT, int otherFlags);
DGLuint         GL_UploadTexture2(texturecontent_t *content);

byte            GL_PrepareFlat(struct materialtexinst_s* inst,
                               int ofTypeID, boolean isFromIWAD);
byte            GL_PrepareDDTexture(struct materialtexinst_s* inst,
                                    int ofTypeID);
byte            GL_PrepareTexture(struct materialtexinst_s* inst,
                                  int ofTypeID, boolean isFromIWAD,
                                  boolean loadAsSky, boolean zeroMask,
                                  boolean noCompression);
byte            GL_PrepareSprite(struct materialtexinst_s* inst,
                                 int ofTypeID, boolean isFromIWAD);

DGLuint         GL_GetRawTexInfo(lumpnum_t lump, boolean part2);
DGLuint         GL_PreparePatch(lumpnum_t lump);
DGLuint         GL_PrepareRawTex(lumpnum_t lump, boolean part2);
DGLuint         GL_PrepareLSTexture(lightingtexid_t which);
DGLuint         GL_PrepareFlareTexture(flaretexid_t flare);
unsigned int    GL_PreparePSprite(materialnum_t num);
detailtexinst_t* GL_PrepareDetailTexture(detailtex_t* dTex, float contrast);
byte*           GL_GetPalette(void);
byte*           GL_GetPal18to8(void);

void            GL_SetMaterial(materialnum_t num);

unsigned int    GL_SetRawImage(lumpnum_t lump, boolean part2, int wrapS, int wrapT);
void            GL_SetPSprite(materialnum_t num);
void            GL_SetTranslatedSprite(materialnum_t num, int tmap, int tclass);
void            GL_NewSplitTex(lumpnum_t lump, DGLuint part2name);
void            GL_SetNoTexture(void);
void            GL_UpdateTexParams(int mipmode);
void            GL_DeleteRawImages(void);
void            GL_DeleteHUDSprite(int spritelump);

boolean         GL_IsColorKeyed(const char* path);

unsigned int    GL_PrepareTranslatedSprite(materialnum_t num, int tmap,
                                           int tclass);
transspr_t*     GL_GetTranslatedSprite(int pnum, unsigned char* table);

// Load the skin texture and prepare it for rendering.
unsigned int    GL_PrepareSkin(skintex_t* stp, boolean allowTexComp);
unsigned int    GL_PrepareShinySkin(skintex_t* stp);

// Loads the shiny texture and the mask texture, if they aren't yet loaded.
boolean         GL_LoadReflectionMap(ded_reflection_t* ref);
#endif
