/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
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

#define TEXQ_BEST 8

/*
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

/*
 * Processing modes for GL_LoadGraphics.
 */
typedef enum gfxmode_e {
    LGM_NORMAL = 0,
    LGM_GRAYSCALE = 1,
    LGM_GRAYSCALE_ALPHA = 2,
    LGM_WHITE_ALPHA = 3
} gfxmode_t;

/*
 * Textures used in the lighting system.
 */
typedef enum lightingtex_e {
    LST_DYNAMIC,                   // Round dynamic light
    LST_GRADIENT,                  // Top-down gradient
    LST_RADIO_CO,                  // FakeRadio closed/open corner shadow
    LST_RADIO_CC,                  // FakeRadio closed/closed corner shadow
    LST_RADIO_OO,                  // FakeRadio open/open shadow
    LST_RADIO_OE,                  // FakeRadio open/edge shadow
    NUM_LIGHTING_TEXTURES
} lightingtex_t;

typedef enum flaretex_e {
    FXT_FLARE,                     // (flare)
    FXT_BRFLARE,                   // (brFlare)
    FXT_BIGFLARE,                  // (bigFlare)
    NUM_FLARE_TEXTURES
} flaretex_t;

/*
 * Textures used in world rendering.
 * eg a surface with a missing tex/flat is drawn using the "missing" graphic
 */
typedef enum ddtexture_e {
    DDT_UNKNOWN,          // Drawn if a texture/flat is unknown
    DDT_MISSING,          // Drawn in place of HOMs in dev mode.
    DDT_BBOX,             // Drawn when rendering bounding boxes
    DDT_GRAY,             // For lighting debug.
    NUM_DD_TEXTURES
} ddtexture_t;

extern int      glMaxTexSize;
extern int      ratioLimit;
extern int      mipmapping, linearRaw, texQuality, filterSprites;
extern int      texMagMode;
extern int      useSmartFilter;
extern byte     loadExtAlways;
extern int      texMagMode;
extern float    texw, texh;
extern int      texmask;
extern detailinfo_t *texdetail;
extern unsigned int curtex;

extern DGLuint  lightingTexNames[NUM_LIGHTING_TEXTURES];
extern DGLuint  flareTexNames[NUM_FLARE_TEXTURES];

void            GL_TexRegister(void);
void            GL_InitTextureManager(void);
void            GL_ShutdownTextureManager(void);
void            GL_LoadSystemTextures(boolean loadLightMaps, boolean loadFlareMaps);
void            GL_ClearTextureMemory(void);
void            GL_ClearRuntimeTextures(void);
void            GL_ClearSystemTextures(void);
void            GL_DoTexReset(cvar_t *unused);
int             GL_InitPalettedTexture(void);
void            GL_DestroySkinNames(void);
void            GL_ResetLumpTexData(void);

unsigned int    GL_BindTexFlat(struct flat_s *fl);
void            GL_SetFlat(int idx);
void            GL_BindTexture(DGLuint texname);
void            GL_TextureFilterMode(int target, int parm);

extern int      pallump;
void            GL_UpdateGamma(void);

void            GL_LowRes(void);
void            TranslatePatch(struct patch_s *patch, byte *transTable);
byte           *GL_LoadImage(image_t * img, const char *imagefn,
                             boolean useModelPath);
byte           *GL_LoadImageCK(image_t * img, const char *imagefn,
                               boolean useModelPath);
void            GL_DestroyImage(image_t * img);
byte           *GL_LoadTexture(image_t * img, char *name);
DGLuint         GL_LoadGraphics(const char *name, gfxmode_t mode);
DGLuint         GL_LoadGraphics2(resourceclass_t resClass, const char *name,
                                 gfxmode_t mode, int useMipmap, boolean clamped);
DGLuint         GL_LoadGraphics3(const char *name, gfxmode_t mode,
                                 int minFilter, int magFilter, 
                                 int wrapS, int wrapT, int otherFlags);
DGLuint         GL_LoadGraphics4(resourceclass_t resClass, const char *name,
                                 gfxmode_t mode, int useMipmap, 
                                 int minFilter, int magFilter, int wrapS, int wrapT, 
                                 int otherFlags);
DGLuint         GL_UploadTexture(byte *data, int width, int height,
                                 boolean flagAlphaChannel,
                                 boolean flagGenerateMipmaps,
                                 boolean flagRgbData,
                                 boolean flagNoStretch,
                                 boolean flagNoSmartFilter,
                                 int minFilter, int magFilter,
                                 int wrapS, int wrapT, int otherFlags);
DGLuint         GL_UploadTexture2(texturecontent_t *content);
DGLuint         GL_GetTextureInfo(int index);
DGLuint         GL_GetTextureInfo2(int index, boolean translate);
DGLuint         GL_GetFlatInfo(int idx, boolean translate);
DGLuint         GL_PrepareTexture(int idx);
DGLuint         GL_PrepareTexture2(int idx, boolean translate);
DGLuint         GL_PrepareFlat(int idx);
DGLuint         GL_PrepareFlat2(int idx, boolean translate);
DGLuint         GL_PrepareLSTexture(lightingtex_t which);
DGLuint         GL_PrepareFlareTexture(flaretex_t flare);
DGLuint         GL_PrepareSky(int idx, boolean zeroMask);
DGLuint         GL_PrepareSky2(int idx, boolean zeroMask, boolean translate);
DGLuint         GL_PrepareDDTexture(ddtexture_t idx);
void            GL_BufferSkyTexture(int idx, byte **outbuffer, int *width,
                                    int *height, boolean zeroMask);
unsigned int    GL_PrepareSprite(int pnum, int spriteMode);
byte           *GL_GetPalette(void);
byte           *GL_GetPal18to8(void);
void            GL_SetTexture(int idx);
void            GL_SetSprite(int pnum, int spriteType);
void            GL_SetTranslatedSprite(int pnum, int tmap, int tclass);
void            GL_NewSplitTex(int lump, DGLuint part2name);
DGLuint         GL_GetOtherPart(int lump);
void            GL_SetPatch(int lump);  // No mipmaps are generated.
void            GL_SetNoTexture(void);
int             GL_GetLumpTexWidth(int lump);
int             GL_GetLumpTexHeight(int lump);
void            GL_UpdateTexParams(int mipmode);
void            GL_UpdateRawScreenParams(int smoothing);
void            GL_DeleteRawImages(void);
void            GL_DeleteSprite(int spritelump);
int             GL_GetSkinTexIndex(const char *skin);

boolean         GL_IsColorKeyed(const char *path);
void            GL_GetSkyTopColor(int texidx, byte *rgb);
void            GL_GetSpriteColorf(int pnum, float *rgb);
void            GL_GetFlatColor(int fnum, unsigned char *rgb);
void            GL_GetTextureColor(int texid, unsigned char *rgb);

// Part is either 1 or 2. Part 0 means only the left side is loaded.
// No splittex is created in that case. Once a raw image is loaded
// as part 0 it must be deleted before the other part is loaded at the
// next loading.
unsigned int    GL_SetRawImage(int lump, int part);

// Returns the real DGL texture, if such exists
unsigned int    GL_GetTextureName(int texidx);

// Only for textures (not for flats, sprites, etc.)
void            GL_DeleteTexture(int texidx);

// Load the skin texture and prepare it for rendering.
unsigned int    GL_PrepareSkin(model_t * mdl, int skin);
unsigned int    GL_PrepareShinySkin(modeldef_t * md, int sub);

// Loads the shiny texture and the mask texture, if they aren't yet loaded.
boolean         GL_LoadReflectionMap(ded_reflection_t *ref);
#endif
