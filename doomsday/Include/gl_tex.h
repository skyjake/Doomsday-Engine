//===========================================================================
// GL_TEX.H
//===========================================================================
#ifndef __DOOMSDAY_TEXTURES_H__
#define __DOOMSDAY_TEXTURES_H__

#include "r_data.h"
#include "con_decl.h"

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
	byte *pixels;
} image_t;

extern int		mipmapping, linearRaw, texQuality, filterSprites;
extern float	texw, texh;
extern int		texmask;	
extern detailinfo_t *texdetail;
extern unsigned int	curtex;
extern int		pallump;
extern DGLuint	dltexname, glowtexname;	

int				CeilPow2(int num);

void			GL_InitTextureManager(void);
void			GL_ShutdownTextureManager(void);
void			GL_LoadSystemTextures(void);
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
byte *			GL_LoadImage(image_t *img, const char *imagefn, boolean useModelPath);
byte *			GL_LoadImageCK(image_t *img, const char *imagefn, boolean useModelPath);
byte *			GL_LoadTexture(image_t *img, char *name);
void			GL_DestroyImage(image_t *img);
DGLuint			GL_GetTextureInfo(int index);
DGLuint			GL_PrepareTexture(int idx); 
DGLuint			GL_PrepareFlat(int idx);
DGLuint			GL_PrepareLightTexture(void);	// The dynamic light map.
DGLuint			GL_PrepareGlowTexture(void);	// Glow map.
DGLuint			GL_PrepareFlareTexture(int flare);
unsigned int	GL_PrepareSky(int idx, boolean zeroMask);
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
unsigned int	GL_PrepareSkin(struct model_s *mdl, int skin);
unsigned int	GL_PrepareShinySkin(struct modeldef_s *md, int sub);

// Console commands.
D_CMD( LowRes );
D_CMD( ResetTextures );
D_CMD( MipMap );
D_CMD( SmoothRaw );

#endif