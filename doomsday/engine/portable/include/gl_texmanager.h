/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * gltexture
 *
 * Presents an abstract interface to all supported texture types so that
 * they may be managed transparently.
 */

#define GLTEXTURE_TYPE_STRING(t)     ((t) == GLT_FLAT? "flat" : \
    (t) == GLT_DOOMTEXTURE? "doomtexture" : \
    (t) == GLT_DOOMPATCH? "doompatch" : \
    (t) == GLT_SPRITE? "sprite" : \
    (t) == GLT_DETAIL? "detailtex" : \
    (t) == GLT_SHINY? "shinytex" : \
    (t) == GLT_MASK? "masktex" : \
    (t) == GLT_MODELSKIN? "modelskin" : \
    (t) == GLT_MODELSHINYSKIN? "modelshinyskin" : \
    (t) == GLT_LIGHTMAP? "lightmap" : \
    (t) == GLT_FLARE? "flaretex" : "systemtex")

typedef struct gltexture_s {
    gltextureid_t   id;
    char            name[9];
    gltexture_type_t type;
    int             ofTypeID;
    void*           instances;

    uint            hashNext; // 1-based index
} gltexture_t;

/**
 * @defGroup GLTextureFlags GLTexture Flags
 */
/*@{*/
#define GLTF_ZEROMASK               0x1 // Zero the alpha of loaded textures.
#define GLTF_NO_COMPRESSION         0x2 // Do not compress the loaded textures.
#define GLTF_UPSCALE_AND_SHARPEN    0x4
#define GLTF_MONOCHROME             0x8
/*@}*/

typedef struct gltexture_inst_s {
    DGLuint         id; // Name of the associated DGL texture.
    byte            flags; // GLTF_* flags.
    byte            border; // In texels, added to all four edges of the texture.
    boolean         isMasked;
    const gltexture_t* tex;
    union {
        struct {
            float           color[3]; // Average color (for lighting).
            float           colorAmplified[3]; // Average color amplified (for lighting).
            float           topColor[3]; // Averaged top line color, used for sky fadeouts.
        } texture; // also used with GLT_FLAT.
        struct {
            boolean         pSprite; // @c true, iff this is for use as a psprite.
            float           flareX, flareY, lumSize;
            rgbcol_t        autoLightColor;
            float           texCoord[2]; // Prepared texture coordinates.
            int             tmap, tclass; // Color translation.
        } sprite;
        struct {
            float           contrast;
        } detail;
    } data; // type-specific data.
} gltexture_inst_t;

/**
 * This structure is used with GL_LoadImage. When it is no longer needed
 * it must be discarded with GL_DestroyImage.
 */
typedef struct image_s {
    filename_t      fileName;
    int             width;
    int             height;
    int             pixelSize;
    boolean         isMasked;
    int             originalBits;  // Bits per pixel in the image file.
    byte*           pixels;
} image_t;

extern int ratioLimit;
extern int mipmapping, filterUI, texQuality, filterSprites;
extern int texMagMode, texAniso;
extern int useSmartFilter;
extern byte loadExtAlways;
extern int texMagMode;
extern int monochrome, upscaleAndSharpenPatches;
extern int glmode[6];

void            GL_TexRegister(void);

void            GL_EarlyInitTextureManager(void);
void            GL_InitTextureManager(void);
void            GL_ResetTextureManager(void);
void            GL_ShutdownTextureManager(void);

void            GL_TexReset(void);
void            GL_LoadSystemTextures(void);
void            GL_ClearTextureMemory(void);
void            GL_ClearRuntimeTextures(void);
void            GL_ClearSystemTextures(void);
void            GL_DeleteRawImages(void);
int             GL_InitPalettedTexture(void);

void            GL_DoTexReset(cvar_t* unused);
void            GL_DoUpdateTexGamma(cvar_t* unused);
void            GL_DoUpdateTexParams(cvar_t* unused);
void            GL_UpdateTexParams(int mipmode);

void            GL_BindTexture(DGLuint texname, int magMode);
void            GL_SetNoTexture(void);
void            GL_SetTextureParams(int minMode, int gameTex, int uiTex);
boolean         GL_IsColorKeyed(const char* path);

byte*           GL_LoadImage(image_t* img, const char* imagefn);
void            GL_DestroyImage(image_t* img);

DGLuint         GL_UploadTexture(byte* data, int width, int height,
                                 boolean flagAlphaChannel,
                                 boolean flagGenerateMipmaps,
                                 boolean flagRgbData,
                                 boolean flagNoStretch,
                                 boolean flagNoSmartFilter,
                                 int minFilter, int magFilter,
                                 int anisoFilter, int wrapS, int wrapT,
                                 int otherFlags);
DGLuint         GL_UploadTexture2(texturecontent_t *content);

byte            GL_LoadFlat(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadDoomTexture(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadSprite(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadDDTexture(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadShinyTexture(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadMaskTexture(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadDetailTexture(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadModelSkin(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadModelShinySkin(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadLightMap(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadFlareTexture(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadDoomPatch(image_t* image, const gltexture_inst_t* inst, void* context);

byte            GL_LoadRawTex(image_t* image, const rawtex_t* r);
byte            GL_LoadExtTexture(image_t* image, ddresourceclass_t resClass,
                                  const char* name, gfxmode_t mode);

DGLuint         GL_PrepareExtTexture(ddresourceclass_t resClass,
                                     const char* name, gfxmode_t mode,
                                     int useMipmap, int minFilter,
                                     int magFilter, int anisoFilter,
                                     int wrapS, int wrapT, int otherFlags);
DGLuint         GL_PrepareLSTexture(lightingtexid_t which);
DGLuint         GL_PrepareSysFlareTexture(flaretexid_t flare);

DGLuint         GL_PreparePatch(patchtex_t* patch);
DGLuint         GL_PrepareRawTex(rawtex_t* rawTex);

DGLuint         GL_GetLightMapTexture(const char* name);
DGLuint         GL_GetFlareTexture(const char* name, int oldIdx);

void            GL_SetMaterial(material_t* mat);
void            GL_SetPSprite(material_t* mat);
void            GL_SetTranslatedSprite(material_t* mat, int tclass, int tmap);

void            GL_SetRawImage(lumpnum_t lump, int wrapS, int wrapT);


// Management of and access to gltextures (via the texmanager):
const gltexture_t* GL_CreateGLTexture(const char* name, int ofTypeId, gltexture_type_t type);
void            GL_ReleaseGLTexture(gltextureid_t id);
const gltexture_inst_t* GL_PrepareGLTexture(gltextureid_t id, void* context, byte* result);
const gltexture_t* GL_GetGLTexture(gltextureid_t id);
const gltexture_t* GL_GetGLTextureByName(const char* name, gltexture_type_t type);
const gltexture_t* GL_GetGLTextureByTypeId(int ofTypeId, gltexture_type_t type);
uint            GL_CheckTextureNumForName(const char* name, gltexture_type_t type);
uint            GL_TextureNumForName(const char* name, gltexture_type_t type);
void            GL_SetAllGLTexturesMinMode(int minMode);
void            GL_DeleteAllTexturesForGLTextures(gltexture_type_t);

/// \todo should not be visible outside the texmanager?
const char*     GLTexture_Name(const gltexture_t* tex);
float           GLTexture_GetWidth(const gltexture_t* tex);
float           GLTexture_GetHeight(const gltexture_t* tex);
boolean         GLTexture_IsFromIWAD(const gltexture_t* tex);

#endif
