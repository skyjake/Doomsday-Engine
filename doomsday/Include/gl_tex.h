/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * gl_tex.h: Texture Management
 */

#ifndef __DOOMSDAY_TEXTURES_H__
#define __DOOMSDAY_TEXTURES_H__

#include "r_data.h"
#include "r_model.h"
#include "con_decl.h"
#include "gl_model.h"

/*
 * This structure is used with GL_LoadImage. When it is no longer needed
 * it must be discarded with GL_DestroyImage.
 */
typedef struct image_s {
	char fileName[256];
	int width;
	int height;
	int pixelSize;
	boolean isMasked;
	int originalBits;	// Bits per pixel in the image file.
	byte *pixels;
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
	LST_DYNAMIC,	// Round dynamic light
	LST_GRADIENT,	// Top-down gradient
	LST_RADIO_CO, 	// FakeRadio closed/open corner shadow
	LST_RADIO_CC,	// FakeRadio closed/closed corner shadow
	NUM_LIGHTING_TEXTURES
} lightingtex_t;


extern int		mipmapping, linearRaw, texQuality, filterSprites;
extern int		texMagMode;
extern int		useSmartFilter;
extern boolean	loadExtAlways;
extern float	texw, texh;
extern int		texmask;	
extern detailinfo_t *texdetail;
extern unsigned int	curtex;
extern int		pallump;
extern DGLuint	lightingTexNames[NUM_LIGHTING_TEXTURES];

int				CeilPow2(int num);

void			GL_InitTextureManager(void);
void			GL_ShutdownTextureManager(void);
void			GL_LoadSystemTextures(boolean loadLightMaps);
void			GL_ClearTextureMemory(void);
void			GL_ClearRuntimeTextures(void);
void			GL_ClearSystemTextures(void);
int				GL_InitPalettedTexture(void);
void			GL_DestroySkinNames(void);
void			GL_ResetLumpTexData(void);
void			GL_UpdateGamma(void);
void			GL_DownMipmap32(byte *in, int width, int height, int comps);
unsigned int	GL_BindTexFlat(struct flat_s *fl);
void			GL_SetFlat(int idx);
void			GL_BindTexture(DGLuint texname);
void			GL_TextureFilterMode(int target, int parm);
boolean			GL_IsColorKeyed(const char *path);
boolean			GL_ColorKey(byte *color);
void			GL_DoColorKeying(byte *rgbaBuf, int width);
void			GL_LowRes();
void			PalIdxToRGB(byte *pal, int idx, byte *rgb);
void			TranslatePatch(struct patch_s *patch, byte *transTable);
void			GL_ConvertToLuminance(image_t *image);
void			GL_ConvertToAlpha(image_t *image, boolean makeWhite);
void			GL_ScaleBuffer32(byte *in, int inWidth, int inHeight, byte *out, int outWidth, int outHeight, int comps);
byte *			GL_LoadImage(image_t *img, const char *imagefn, boolean useModelPath);
byte *			GL_LoadImageCK(image_t *img, const char *imagefn, boolean useModelPath);
void			GL_DestroyImage(image_t *img);
byte *			GL_LoadTexture(image_t *img, char *name);
DGLuint			GL_LoadGraphics(const char *name, gfxmode_t mode);
DGLuint			GL_GetTextureInfo(int index);
DGLuint			GL_GetTextureInfo2(int index, boolean translate);
DGLuint			GL_PrepareTexture(int idx); 
DGLuint			GL_PrepareTexture2(int idx, boolean translate);
DGLuint			GL_PrepareFlat(int idx);
DGLuint			GL_PrepareFlat2(int idx, boolean translate);
DGLuint			GL_PrepareLSTexture(lightingtex_t which);
DGLuint			GL_PrepareFlareTexture(int flare);
DGLuint			GL_PrepareSky(int idx, boolean zeroMask);
DGLuint			GL_PrepareSky2(int idx, boolean zeroMask, boolean translate);
unsigned int	GL_PrepareSprite(int pnum, int spriteMode);
void			GL_SetTexture(int idx);
void			GL_GetSkyTopColor(int texidx, byte *rgb);
void			GL_SetSprite(int pnum, int spriteType);
void			GL_SetTranslatedSprite(int pnum, int tmap, int tclass);
void			GL_GetSpriteColor(int pnum, unsigned char *rgb);
void			GL_GetFlatColor(int fnum, unsigned char *rgb);
void			GL_NewSplitTex(int lump, DGLuint part2name);
DGLuint			GL_GetOtherPart(int lump);
void			GL_SetPatch(int lump);	// No mipmaps are generated.
void			GL_SetNoTexture(void);
int				GL_GetLumpTexWidth(int lump);
int				GL_GetLumpTexHeight(int lump);
int				GL_ValidTexHeight2(int width, int height);
void			GL_UpdateTexParams(int mipmode);
void			GL_UpdateRawScreenParams(int smoothing);
void			GL_DeleteRawImages(void);
void			GL_DeleteSprite(int spritelump);
int				GL_GetSkinTexIndex(const char *skin);

// Part is either 1 or 2. Part 0 means only the left side is loaded.
// No splittex is created in that case. Once a raw image is loaded
// as part 0 it must be deleted before the other part is loaded at the
// next loading.
unsigned int 	GL_SetRawImage(int lump, int part);

// Returns the real DGL texture, if such exists
unsigned int	GL_GetTextureName(int texidx); 

// Only for textures (not for flats, sprites, etc.)
void			GL_DeleteTexture(int texidx); 

// Load the skin texture and prepare it for rendering.
unsigned int	GL_PrepareSkin(model_t *mdl, int skin);
unsigned int	GL_PrepareShinySkin(modeldef_t *md, int sub);

// Console commands.
D_CMD( LowRes );
D_CMD( ResetTextures );
D_CMD( MipMap );
D_CMD( SmoothRaw );

#endif
