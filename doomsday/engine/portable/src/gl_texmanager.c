/**\file gl_texmanager.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 1999-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2002 Graham Jackson <no contact email published>
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
 * Texture management routines.
 *
 * Much of this stuff actually belongs in Refresh.
 * This file needs to be split into smaller portions.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <ctype.h>

#ifdef UNIX
#   include "de_platform.h"
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_misc.h"
#include "de_play.h"

#include "image.h"
#include "def_main.h"
#include "ui_main.h"
#include "p_particle.h"

// MACROS ------------------------------------------------------------------

#define GLTEXTURE_NAME_HASH_SIZE (512)

// TYPES -------------------------------------------------------------------

typedef struct {
    const char* name; // Format/handler name.
    const char* ext; // Expected file extension.
    boolean (*loadFunc)(image_t* img, DFILE* file);
    const char* (*getLastErrorFunc) (void);
} imagehandler_t;

typedef struct gltexture_inst_node_s {
    short            flags; // Texture instance (MLF_*) flags.
    gltexture_inst_t inst;
    struct gltexture_inst_node_s* next; // Next in list of instances.
} gltexture_inst_node_t;

typedef struct gltexture_typedata_s {
    byte           (*loadData) (image_t*, const gltexture_inst_t*, void*);
    uint            hashTable[GLTEXTURE_NAME_HASH_SIZE];
} gltexture_typedata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(LowRes);
D_CMD(ResetTextures);
D_CMD(MipMap);

void GL_DoResetDetailTextures(const cvar_t* cvar);

/// gltexture_t abstract interface:
gltexture_inst_t* GLTexture_Prepare(gltexture_t* tex,
                                    void* context, byte* result);
void            GLTexture_ReleaseTextures(gltexture_t* tex);

void            GLTexture_SetMinMode(gltexture_t* tex, int minMode);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void calcGammaTable(void);

static boolean tryLoadPCX(image_t* img, DFILE* file);
static boolean tryLoadPNG(image_t* img, DFILE* file);
static boolean tryLoadTGA(image_t* img, DFILE* file);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int ratioLimit;
extern boolean palettedTextureExtAvailable;
extern boolean s3tcAvailable;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int ratioLimit = 0; // Zero if none.
boolean fillOutlines = true;
byte loadExtAlways = false; // Always check for extres (cvar)
byte paletted = false; // Use GL_EXT_paletted_texture (cvar)
boolean load8bit = false; // Load textures as 8 bit? (w/paltex)
int monochrome = 0; // desaturate a patch (average colours)
int upscaleAndSharpenPatches = 0;
int useSmartFilter = 0; // Smart filter mode (cvar: 1=hq2x)
int mipmapping = 5, filterUI = 1, texQuality = TEXQ_BEST;
int filterSprites = true;
int texMagMode = 1; // Linear.
int texAniso = -1; // Use best.

float texGamma = 0;
byte gammaTable[256];

int glmode[6] = // Indexed by 'mipmapping'.
{
    GL_NEAREST,
    GL_LINEAR,
    GL_NEAREST_MIPMAP_NEAREST,
    GL_LINEAR_MIPMAP_NEAREST,
    GL_NEAREST_MIPMAP_LINEAR,
    GL_LINEAR_MIPMAP_LINEAR
};

// Names of the dynamic light textures.
ddtexture_t lightingTextures[NUM_LIGHTING_TEXTURES];

// Names of the flare textures (halos).
ddtexture_t sysFlareTextures[NUM_SYSFLARE_TEXTURES];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean texInited = false; // Init done.
static boolean allowMaskedTexEnlarge = false;
static boolean noHighResTex = false;
static boolean noHighResPatches = false;
static boolean highResWithPWAD = false;

// Image file handlers.
static const imagehandler_t handlers[] = {
    { "PNG", "png", tryLoadPNG, PNG_LastError },
    { "TGA", "tga", tryLoadTGA, TGA_LastError },
    { "PCX", "pcx", tryLoadPCX, PCX_LastError },
    { 0 } // Terminate.
};

// gltextures.
static uint numGLTextures;
static gltexture_t** glTextures;
static gltexture_typedata_t glTextureTypeData[NUM_GLTEXTURE_TYPES] = {
    { GL_LoadDDTexture }, // GLT_SYSTEM
    { GL_LoadFlat }, // GLT_FLAT
    { GL_LoadDoomTexture }, // GLT_DOOMTEXTURE
    { GL_LoadDoomPatch }, // GLT_DOOMPATCH
    { GL_LoadSprite }, // GLT_SPRITE
    { GL_LoadDetailTexture }, // GLT_DETAIL
    { GL_LoadShinyTexture }, // GLT_SHINY
    { GL_LoadMaskTexture }, // GLT_MASK
    { GL_LoadModelSkin }, // GLT_MODELSKIN
    { GL_LoadModelShinySkin }, // GLT_MODELSHINYSKIN
    { GL_LoadLightMap }, // GLT_LIGHTMAP
    { GL_LoadFlareTexture }
};

// CODE --------------------------------------------------------------------

void GL_TexRegister(void)
{
    // Cvars
    C_VAR_INT("rend-tex", &renderTextures, CVF_NO_ARCHIVE, 0, 2);
    C_VAR_FLOAT2("rend-tex-gamma", &texGamma, 0, 0, 1,
                 GL_DoUpdateTexGamma);
    C_VAR_INT2("rend-tex-mipmap", &mipmapping, CVF_PROTECTED, 0, 5,
               GL_DoTexReset);
    C_VAR_BYTE2("rend-tex-paletted", &paletted, CVF_PROTECTED, 0, 1,
                GL_DoTexReset);
    C_VAR_BYTE2("rend-tex-external-always", &loadExtAlways, 0, 0, 1,
                GL_DoTexReset);
    C_VAR_INT2("rend-tex-quality", &texQuality, 0, 0, 8,
               GL_DoTexReset);
    C_VAR_INT("rend-tex-filter-mag", &texMagMode, 0, 0, 1);
    C_VAR_INT("rend-tex-filter-sprite", &filterSprites, 0, 0, 1);
    C_VAR_INT("rend-tex-filter-ui", &filterUI, 0, 0, 1);
    C_VAR_INT2("rend-tex-filter-smart", &useSmartFilter, 0, 0, 1,
               GL_DoTexReset);
    C_VAR_INT("rend-tex-filter-anisotropic", &texAniso, 0, -1, 4);
    C_VAR_INT("rend-tex-detail", &r_detail, 0, 0, 1);
    C_VAR_FLOAT("rend-tex-detail-scale", &detailScale,
                CVF_NO_MIN | CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT2("rend-tex-detail-strength", &detailFactor, 0, 0, 10,
                 GL_DoResetDetailTextures);
    C_VAR_INT("rend-tex-detail-multitex", &useMultiTexDetails, 0, 0, 1);

    // Ccmds
    C_CMD_FLAGS("lowres", "", LowRes, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("mipmap", "i", MipMap, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("texreset", "", ResetTextures, CMDF_NO_DEDICATED);
}

void GL_EarlyInitTextureManager(void)
{
    GL_InitSmartFilterHQ2x();
    calcGammaTable();

    numGLTextures = 0;
    glTextures = NULL;

    { int i;
    for(i = 0; i < NUM_GLTEXTURE_TYPES; ++i)
        memset(glTextureTypeData[i].hashTable, 0, sizeof(glTextureTypeData[i].hashTable));
    }
}

/**
 * This should be cleaned up once and for all.
 */
void GL_InitTextureManager(void)
{
    if(novideo)
        return;

    if(texInited)
        return; // Don't init again.

    // The -bigmtex option allows the engine to enlarge masked textures
    // that have taller patches than they are themselves.
    allowMaskedTexEnlarge = ArgExists("-bigmtex");

    // Disable the use of 'high resolution' textures and/or patches?
    noHighResTex = ArgExists("-nohightex");
    noHighResPatches = ArgExists("-nohighpat");

    // Should we allow using external resources with PWAD textures?
    highResWithPWAD = ArgExists("-pwadtex");

    // System textures loaded in GL_LoadSystemTextures.
    memset(sysFlareTextures, 0, sizeof(sysFlareTextures));
    memset(lightingTextures, 0, sizeof(lightingTextures));

    // Initialization done.
    texInited = true;
}

/**
 * Call this if a full cleanup of the textures is required (engine update).
 */
void GL_ResetTextureManager(void)
{
    if(!texInited)
        return;

    GL_ClearTextureMemory();
    texInited = false;
}

void GL_DestroyTextures(void)
{
    if(!texInited) return;

    if(numGLTextures)
    {
        GL_ClearTextures();
        { uint i;
        for(i = 0; i < numGLTextures; ++i)
        {
            gltexture_t* mTex = glTextures[i];
            gltexture_inst_node_t* node = (gltexture_inst_node_t*) mTex->instances;
            while(node)
            {
                gltexture_inst_node_t* next = node->next;
                Z_Free(node);
                node = next;
            }
            Z_Free(mTex);
        }}

        Z_Free(glTextures);
    }
    glTextures = 0;
    numGLTextures = 0;

    { uint i;
    for(i = 0; i < NUM_GLTEXTURE_TYPES; ++i)
        memset(glTextureTypeData[i].hashTable, 0, sizeof(glTextureTypeData[i].hashTable));
    }
}

void GL_ClearTextures(void)
{
    if(!texInited) return;

    // gltexture-wrapped GL textures; textures, flats, sprites, system...
    GL_DeleteAllTexturesForGLTextures(GLT_ANY);
}

/**
 * Called once during engine shutdown.
 */
void GL_ShutdownTextureManager(void)
{
    if(!texInited)
        return; // Already been here?

    GL_DestroyTextures();
    texInited = false;
}

static void calcGammaTable(void)
{
    int                 i;
    double              invGamma;

    // Clamp to a sane range.
    invGamma = 1.0f - MINMAX_OF(0, texGamma, 1);
    for(i = 0; i < 256; ++i)
        gammaTable[i] = (byte)(255.0f * pow(i / 255.0f, invGamma));
}

/**
 * Initializes the paletted texture extension.
 * Returns true if successful.
 */
int GL_InitPalettedTexture(void)
{
    if(novideo)
        return true;

    // Should the extension be used?
    if(!paletted && !ArgCheck("-paltex"))
        return true;

    // Check if the operation was a success.
    if(!GL_EnablePalTexExt(true))
    {
        Con_Message("\nPaletted textures init failed!\n");
        return false;
    }

    // Textures must be uploaded as 8-bit, now.
    load8bit = true;
    return true;
}

/**
 * Prepares all the system textures (dlight, ptcgens).
 */
void GL_LoadSystemTextures(void)
{
    if(!texInited)
        return;

    UI_LoadTextures();

    // Preload lighting system textures.
    GL_PrepareLSTexture(LST_DYNAMIC);
    GL_PrepareLSTexture(LST_GRADIENT);

    GL_PrepareSysFlareTexture(FXT_ROUND);
    GL_PrepareSysFlareTexture(FXT_FLARE);
    if(!haloRealistic)
    {
        GL_PrepareSysFlareTexture(FXT_BRFLARE);
        GL_PrepareSysFlareTexture(FXT_BIGFLARE);
    }

    Rend_ParticleLoadSystemTextures();
    R_InitSystemTextures();
}

/**
 * System textures are loaded at startup and remain in memory all the time.
 * After clearing they must be manually reloaded.
 */
void GL_ClearSystemTextures(void)
{
    if(!texInited)
        return;

    { int i;
    for(i = 0; i < NUM_LIGHTING_TEXTURES; ++i)
        glDeleteTextures(1, (const GLuint*) &lightingTextures[i].tex);
    }
    memset(lightingTextures, 0, sizeof(lightingTextures));

    { int i;
    for(i = 0; i < NUM_SYSFLARE_TEXTURES; ++i)
        glDeleteTextures(1, (const GLuint*) &sysFlareTextures[i].tex);
    }
    memset(sysFlareTextures, 0, sizeof(sysFlareTextures));

    Materials_DeleteTextures(MATERIALS_SYSTEM_RESOURCE_NAMESPACE_NAME);
    UI_ClearTextures();

    Rend_ParticleClearSystemTextures();
}

/**
 * Runtime textures are not loaded until precached or actually needed.
 * They may be cleared, in which case they will be reloaded when needed.
 */
void GL_ClearRuntimeTextures(void)
{
    if(!texInited)
        return;

    // The rendering lists contain persistent references to texture names.
    // Which, obviously, can't persist any longer...
    RL_DeleteLists();

    // gltexture-wrapped GL textures; textures, flats, sprites...
    GL_DeleteAllTexturesForGLTextures(GLT_FLAT);
    GL_DeleteAllTexturesForGLTextures(GLT_DOOMTEXTURE);
    GL_DeleteAllTexturesForGLTextures(GLT_DOOMPATCH);
    GL_DeleteAllTexturesForGLTextures(GLT_SPRITE);
    GL_DeleteAllTexturesForGLTextures(GLT_DETAIL);
    GL_DeleteAllTexturesForGLTextures(GLT_SHINY);
    GL_DeleteAllTexturesForGLTextures(GLT_MASK);
    GL_DeleteAllTexturesForGLTextures(GLT_MODELSKIN);
    GL_DeleteAllTexturesForGLTextures(GLT_MODELSHINYSKIN);
    GL_DeleteAllTexturesForGLTextures(GLT_LIGHTMAP);
    GL_DeleteAllTexturesForGLTextures(GLT_FLARE);
    GL_DeleteRawImages();

    Rend_ParticleClearExtraTextures();
}

void GL_ClearTextureMemory(void)
{
    if(!texInited)
        return;
    // Delete runtime textures (textures, flats, ...)
    GL_ClearRuntimeTextures();
}

/**
 * Binds the OGL texture.
 */
void GL_BindTexture(DGLuint texname, int magMode)
{
    if(Con_IsBusy())
        return;

    glBindTexture(GL_TEXTURE_2D, texname);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magMode);
    if(GL_state.useAnisotropic)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GL_GetTexAnisoMul(texAniso));
}

static __inline void initImage(image_t* img)
{
    assert(img);
    memset(img, 0, sizeof(*img));
}

static boolean tryLoadPCX(image_t* img, DFILE* file)
{
    assert(img && file);
    initImage(img);
    img->pixels = PCX_Load(file, &img->width, &img->height, &img->pixelSize);
    img->originalBits = 8 * img->pixelSize;
    return (0 != img->pixels);
}

static boolean tryLoadPNG(image_t* img, DFILE* file)
{
    assert(img && file);
    initImage(img);
    img->pixels = PNG_Load(file, &img->width, &img->height, &img->pixelSize);
    img->originalBits = 8 * img->pixelSize;
    return (0 != img->pixels);
}

static boolean tryLoadTGA(image_t* img, DFILE* file)
{
    assert(img && file);
    initImage(img);
    img->pixels = TGA_Load(file, &img->width, &img->height, &img->pixelSize);
    img->originalBits = 8 * img->pixelSize;
    return (0 != img->pixels);
}

const imagehandler_t* findHandlerFromFileName(const char* filePath)
{
    const imagehandler_t* hdlr = 0; // No handler for this format.
    const char* p;
    if(0 != (p = M_FindFileExtension(filePath)))
    {
        int i = 0;
        do
        {
            if(!stricmp(p, handlers[i].ext))
                hdlr = &handlers[i];
        } while(0 == hdlr && 0 != handlers[++i].ext);
    }
    return hdlr;
}

/// \todo Remove the filePath argument by obtaining the path via the
/// File Stream Abstraction Layer. This method can then be made public.
static byte* GL_LoadImageDFile(image_t* img, DFILE* file, const char* filePath)
{
    assert(img && file && filePath);
    {
    const imagehandler_t* hdlr;

    // Firstly try the expected format given the file name.
    if(0 != (hdlr = findHandlerFromFileName(filePath)))
    {
        hdlr->loadFunc(img, file);
    }

    // If not loaded. Try each recognisable format.
    /// \todo Order here should be determined by the resource locator.
    { int n = 0;
    while(0 == img->pixels && 0 != handlers[n].name)
    {
        if(&handlers[n] == hdlr) continue; // We already know its not in this format.
        handlers[n++].loadFunc(img, file);
    }}

    if(0 == img->pixels)
        return 0; // Not a recogniseable format.

    VERBOSE( Con_Message("GL_LoadImage: \"%s\" (%ix%i)\n", M_PrettyPath(filePath), img->width, img->height) );

    // How about some color-keying?
    if(GL_IsColorKeyed(filePath))
    {
        byte* out = GL_ApplyColorKeying(img->pixels, img->pixelSize, img->width, img->height);
        if(out)
        {
            // Had to allocate a larger buffer, free the old and attach the new.
            free(img->pixels);
            img->pixels = out;
        }
        // Color keying is done; now we have 4 bytes per pixel.
        img->pixelSize = 4;
        img->originalBits = 32; // Effectively...
    }

    // Any alpha pixels?
    if(ImageHasAlpha(img))
        img->flags |= IMGF_IS_MASKED;

    return img->pixels;
    }
}

byte* GL_LoadImage(image_t* img, const char* filePath)
{
    assert(img);
    {
    byte* result = 0;
    DFILE* file;
    if(NULL != (file = F_Open(filePath, "rb")))
    {
        result = GL_LoadImageDFile(img, file, filePath);
        F_Close(file);
    }
    return result;
    }
}

byte* GL_LoadImageStr(image_t* img, const ddstring_t* filePath)
{
    if(filePath)
        return GL_LoadImage(img, Str_Text(filePath));
    return 0;
}

void GL_DestroyImage(image_t* img)
{
    assert(img);
    if(img->pixels)
        free(img->pixels);
}

/*DGLuint GL_UploadTexture(byte *data, int width, int height,
                         boolean alphaChannel, boolean generateMipmaps,
                         boolean RGBData, boolean noStretch)*/
DGLuint GL_UploadTexture(byte *data, int width, int height,
                         boolean flagAlphaChannel,
                         boolean flagGenerateMipmaps,
                         boolean flagRgbData,
                         boolean flagNoStretch,
                         boolean flagNoSmartFilter,
                         int minFilter, int magFilter, int anisoFilter,
                         int wrapS, int wrapT, int otherFlags)/*boolean alphaChannel, boolean generateMipmaps,
                         boolean RGBData, boolean noStretch)*/
{
    texturecontent_t content;

    GL_InitTextureContent(&content);
    content.buffer = data;
    content.format = (flagRgbData? (flagAlphaChannel? DGL_RGBA : DGL_RGB) :
                      (flagAlphaChannel? DGL_COLOR_INDEX_8_PLUS_A8 : DGL_COLOR_INDEX_8));
    content.width = width;
    content.height = height;
    content.flags = TXCF_EASY_UPLOAD | otherFlags;
    if(flagAlphaChannel) content.flags |= TXCF_UPLOAD_ARG_ALPHACHANNEL;
    if(flagGenerateMipmaps) content.flags |= TXCF_MIPMAP;
    if(flagRgbData) content.flags |= TXCF_UPLOAD_ARG_RGBDATA;
    if(flagNoStretch) content.flags |= TXCF_UPLOAD_ARG_NOSTRETCH;
    if(flagNoSmartFilter) content.flags |= TXCF_UPLOAD_ARG_NOSMARTFILTER;
    content.minFilter = minFilter;
    content.magFilter = magFilter;
    content.anisoFilter = anisoFilter;
    content.wrap[0] = wrapS;
    content.wrap[1] = wrapT;
    return GL_NewTexture(&content, NULL);
}

/**
 * Can be rather time-consuming due to scaling operations and mipmap
 * generation. The texture parameters will NOT be set here.
 *
 * @param data          Contains indices to the color palette.
 * @param alphaChannel  If true, 'data' also contains the alpha values
 *                      (after the indices).
 * @return              The name of the texture (same as 'texName').
 */
DGLuint GL_UploadTexture2(texturecontent_t* content)
{
    uint8_t* data = (uint8_t*) content->buffer;
    int width = content->width;
    int height = content->height;
    boolean alphaChannel = ((content->flags & TXCF_UPLOAD_ARG_ALPHACHANNEL) != 0);
    boolean generateMipmaps = ((content->flags & TXCF_MIPMAP) != 0);
    boolean RGBData = ((content->flags & TXCF_UPLOAD_ARG_RGBDATA) != 0);
    boolean noStretch = ((content->flags & TXCF_UPLOAD_ARG_NOSTRETCH) != 0);
    boolean noSmartFilter = ((content->flags & TXCF_UPLOAD_ARG_NOSMARTFILTER) != 0);
    boolean applyTexGamma = ((content->flags & TXCF_APPLY_GAMMACORRECTION) != 0);
    int i, comps, levelWidth, levelHeight; // width and height at the current level
    uint8_t* buffer = NULL, *rgbaOriginal = NULL, *idxBuffer = NULL;
    boolean freeOriginal;
    boolean freeBuffer;

    // Number of color components in the destination image.
    comps = (alphaChannel ? 4 : 3);

    // Calculate the real dimensions for the texture, as required by
    // the graphics hardware.
    noStretch = GL_OptimalSize(width, height, &levelWidth, &levelHeight, noStretch, generateMipmaps);

    // Get the RGB(A) version of the original texture.
    if(RGBData)
    {
        // The source image can be used as-is.
        freeOriginal = false;
        rgbaOriginal = data;
    }
    else
    {
        // Convert a paletted source image to truecolor so it can be scaled.
        // If there isn't an alpha channel yet, add one.
        freeOriginal = true;
        comps = 4;
        if(0 == (rgbaOriginal = malloc(width * height * comps)))
            Con_Error("GL_UploadTexture2: Failed on allocation of %lu bytes for RGBA"
                      "conversion buffer.", (unsigned long) (width * height * comps));

        GL_ConvertBuffer(width, height, alphaChannel ? 2 : 1, comps, data, rgbaOriginal, 0, !load8bit);

        if(!alphaChannel)
        {
            int n;
            for(n = 0; n < width * height; ++n)
                rgbaOriginal[n * 4 + 3] = 255;
            alphaChannel = true;
        }
    }

    if(applyTexGamma)
    {
        for(i = 0; i < width * height * comps; i += comps)
        {
            rgbaOriginal[i]   = gammaTable[data[i]];
            rgbaOriginal[i+1] = gammaTable[data[i+1]];
            rgbaOriginal[i+2] = gammaTable[data[i+2]];
        }
    }

    if(useSmartFilter && !noSmartFilter)
    {
        uint8_t* filtered;
        int smartFlags = ICF_UPSCALE_SAMPLE_WRAP;

        if(comps == 3)
        {
            // Must convert to RGBA.
            uint8_t* temp;

            if(0 == (temp = malloc(4 * width * height)))
                Con_Error("GL_UploadTexture2: Failed on allocation of %lu bytes for"
                          "RGBA conversion buffer.", (unsigned long) (4 * width * height));

            GL_ConvertBuffer(width, height, 3, 4, rgbaOriginal, temp, 0, !load8bit);

            if(freeOriginal)
                free(rgbaOriginal);

            rgbaOriginal = temp;
            freeOriginal = true;
            comps = 4;
            alphaChannel = true;
        }

        filtered = GL_SmartFilter(GL_ChooseSmartFilter(width, height, 0), rgbaOriginal, width, height, smartFlags, &width, &height);
        noStretch = GL_OptimalSize(width, height, &levelWidth, &levelHeight, noStretch, generateMipmaps);

        if(filtered != rgbaOriginal)
        {
            freeOriginal = true;
            // The filtered copy will be the 'original' image data.
            if(freeOriginal)
                free(rgbaOriginal);
            rgbaOriginal = filtered;
        }
    }

    // Prepare the RGB(A) buffer for the texture: we want a buffer with
    // power-of-two dimensions. It will be the mipmap level zero.
    // The buffer will be modified in the mipmap generation (if done here).
    if(width == levelWidth && height == levelHeight)
    {
        // No resizing necessary.
        buffer = rgbaOriginal;
        freeBuffer = freeOriginal;
        freeOriginal = false;
    }
    else
    {
        if(noStretch)
        {
            // Copy the image into a buffer with power-of-two dimensions.
            if(0 == (buffer = malloc(levelWidth * levelHeight * comps)))
                Con_Error("GL_UploadTexture2: Failed on allocation of %lu bytes for"
                          "upscale buffer.", (unsigned long) (levelWidth * levelHeight * comps));

            memset(buffer, 0, levelWidth * levelHeight * comps);
            for(i = 0; i < height; ++i) // Copy line by line.
                memcpy(buffer + levelWidth * comps * i, rgbaOriginal + width * comps * i, comps * width);
            freeBuffer = true;
        }
        else
        {
            // Stretch to fit into power-of-two.
            if(width != levelWidth || height != levelHeight)
            {
                buffer = GL_ScaleBuffer32(rgbaOriginal, width, height, comps, levelWidth, levelHeight);
                if(buffer != rgbaOriginal)
                    freeBuffer = true;
            }
        }
    }

    // The RGB(A) copy of the source image is no longer needed.
    if(freeOriginal)
    {
        free(rgbaOriginal);
        rgbaOriginal = NULL;
    }

    // Bind the texture so we can upload content.
    glBindTexture(GL_TEXTURE_2D, content->name);

    if(load8bit)
    {
        // We are unable to generate mipmaps for paletted textures.
        DGLuint pal = R_GetColorPalette(content->palette);
        size_t outSize = levelWidth * levelHeight * (alphaChannel ? 2 : 1);
        int canGenMips = 0;

        // Prepare the palette indices buffer, to be handed over to DGL.
        if(0 == (idxBuffer = malloc(outSize)))
            Con_Error("GL_UploadTexture2: Failed on allocation of %lu bytes for"
                      "8bit-palettized down-mip buffer.", (unsigned long) outSize);

        // Since this is a paletted texture, we may need to manually
        // generate the mipmaps.
        for(i = 0; levelWidth || levelHeight; ++i)
        {
            if(!levelWidth)
                levelWidth = 1;
            if(!levelHeight)
                levelHeight = 1;

            // Convert to palette indices.
            GL_ConvertBuffer(levelWidth, levelHeight, comps, alphaChannel ? 2 : 1, buffer, idxBuffer, content->palette, false);

            // Upload it.
            if(!GL_TexImage(alphaChannel ? DGL_COLOR_INDEX_8_PLUS_A8 : DGL_COLOR_INDEX_8, pal, levelWidth, levelHeight, generateMipmaps && canGenMips ? true : generateMipmaps ? -i : false, idxBuffer))
            {
                Con_Error("GL_UploadTexture: TexImage failed (%i x %i) as 8-bit, alpha:%i\n", levelWidth, levelHeight, alphaChannel);
            }

            // If no mipmaps need to generated, quit now.
            if(!generateMipmaps || canGenMips)
                break;

            if(levelWidth > 1 || levelHeight > 1)
                GL_DownMipmap32(buffer, levelWidth, levelHeight, comps);

            // Move on.
            levelWidth >>= 1;
            levelHeight >>= 1;
        }

        free(idxBuffer);
    }
    else
    {
        // DGL knows how to generate mipmaps for RGB(A) textures.
        if(!GL_TexImage(alphaChannel ? DGL_RGBA : DGL_RGB, 0, levelWidth, levelHeight, generateMipmaps ? true : false, buffer))
        {
            Con_Error("GL_UploadTexture: TexImage failed (%i x %i), alpha:%i\n", levelWidth, levelHeight, alphaChannel);
        }
    }

    if(freeBuffer)
        free(buffer);

    return content->name;
}

/**
 * @return              The outcome:
 *                      0 = not loaded.
 *                      1 = loaded data from a lump resource.
 *                      2 = loaded data from an external resource.
 */
byte GL_LoadDDTexture(image_t* image, const gltexture_inst_t* inst, void* context)
{
    assert(image);
    {
    static const char* ddTexNames[NUM_DD_TEXTURES] = {
        "unknown",
        "missing",
        "bbox",
        "gray"
    };
    int num = inst->tex->ofTypeID;
    ddstring_t foundPath;
    byte result = 0;

    if(num < 0 || num > NUM_DD_TEXTURES)
        Con_Error("GL_LoadDDTexture: Internal error, invalid ddtex id %i.", num);

    Str_Init(&foundPath);
    if(F_FindResource2(RC_GRAPHIC, ddTexNames[num], &foundPath) != 0 &&
       GL_LoadImage(image, Str_Text(&foundPath)))
    {
        result = 2;
    }
    Str_Free(&foundPath);

    if(result == 0)
        Con_Error("GL_LoadDDTexture: Error, \"%s\" not found!\n", ddTexNames[num]);

    return result;
    }
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadDetailTexture(image_t* image, const gltexture_inst_t* inst, void* context)
{
    assert(image && inst);
    {
    detailtex_t* dTex = detailTextures[inst->tex->ofTypeID];
    byte result = 0;
    if(dTex->isExternal)
    {
        ddstring_t foundPath, *searchPath = Uri_ComposePath(dTex->filePath);

        Str_Init(&foundPath);
        if(F_FindResourceStr2(RC_GRAPHIC, searchPath, &foundPath) != 0 &&
           GL_LoadImage(image, Str_Text(&foundPath)))
        {
            result = 2;
        }

        if(result == 0)
            Con_Message("GL_LoadDetailTexture: Warning, failed to load \"%s\"\n", Str_Text(searchPath));

        Str_Delete(searchPath);
        Str_Free(&foundPath);
    }
    else
    {
        const char* lumpName = Str_Text(Uri_Path(dTex->filePath));
        DFILE* file;
        if(NULL != (file = F_OpenLump(lumpName, false)))
        {
            if(0 != GL_LoadImageDFile(image, file, lumpName))
            {
                result = 1;
            }
            else
            {   // It must be an old-fashioned "raw" image.
                size_t fileLength = F_Length(file);

                initImage(image);

                /**
                 * @fixme we should not error out here if the lump is not
                 * of the required format! Perform this check much earlier,
                 * when the definitions are read and mark which are valid.
                 */

                // How big is it?
                if(fileLength != 256 * 256)
                {
                    if(fileLength != 128 * 128)
                    {
                        if(fileLength != 64 * 64)
                            Con_Error("GL_LoadDetailTexture: Must be 256x256, 128x128 or 64x64.\n");
                        image->width = image->height = 64;
                    }
                    else
                    {
                        image->width = image->height = 128;
                    }
                }
                else
                    image->width = image->height = 256;

                image->pixelSize = 1;
                image->pixels = malloc(image->width * image->height);
                if(fileLength < (unsigned) (image->width * image->height))
                    memset(image->pixels, 0, image->width * image->height);
                // Load the raw image data.
                F_Read(image->pixels, fileLength, file);
                result = 1;
            }
            F_Close(file);
        }
    }
    return result;
    }
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadLightMap(image_t* image, const gltexture_inst_t* inst, void* context)
{
    assert(image && inst);
    {
    lightmap_t* lmap = lightMaps[inst->tex->ofTypeID];
    ddstring_t* searchPath, foundPath, suffix = { "-ck" };
    byte result = 0;

    searchPath = Uri_ComposePath(lmap->external);

    Str_Init(&foundPath);
    if(F_FindResourceStr3(RC_GRAPHIC, searchPath, &foundPath, &suffix) != 0 &&
       GL_LoadImage(image, Str_Text(&foundPath)))
    {
        result = 2;
    }

    if(result == 0)
        Con_Message("GL_LoadLightMap: Warning, failed to load \"%s\".\n", Str_Text(searchPath));

    Str_Delete(searchPath);
    Str_Free(&foundPath);

    return result;
    }
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadFlareTexture(image_t* image, const gltexture_inst_t* inst, void* context)
{
    assert(image && inst);
    {
    flaretex_t* fTex = flareTextures[inst->tex->ofTypeID];
    ddstring_t* searchPath, foundPath, suffix = { "-ck" };
    byte result = 0;

    searchPath = Uri_ComposePath(fTex->external);

    Str_Init(&foundPath);
    if(F_FindResourceStr3(RC_GRAPHIC, searchPath, &foundPath, &suffix) != 0 &&
       GL_LoadImage(image, Str_Text(&foundPath)))
    {
        result = 2;
    }

    if(result == 0)
        Con_Message("GL_LoadFlareTexture: Warning, failed to load \"%s\"\n", Str_Text(searchPath));

    Str_Delete(searchPath);
    Str_Free(&foundPath);

    return result;
    }
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadShinyTexture(image_t* image, const gltexture_inst_t* inst, void* context)
{
    assert(image && inst);
    {
    shinytex_t* sTex = shinyTextures[inst->tex->ofTypeID];
    ddstring_t* searchPath, foundPath;
    byte result = 0;

    searchPath = Uri_ComposePath(sTex->external);

    Str_Init(&foundPath);
    if(F_FindResourceStr2(RC_GRAPHIC, searchPath, &foundPath) != 0 &&
       GL_LoadImage(image, Str_Text(&foundPath)))
    {
        result = 2;
    }

    if(result == 0)
        Con_Message("GL_LoadShinyTexture: Warning, failed to load \"%s\"\n", Str_Text(searchPath));

    Str_Delete(searchPath);
    Str_Free(&foundPath);

    return result;
    }
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadMaskTexture(image_t* image, const gltexture_inst_t* inst, void* context)
{
    assert(image && inst);
    {
    masktex_t* mTex = maskTextures[inst->tex->ofTypeID];
    ddstring_t* searchPath, foundPath;
    byte result = 0;

    searchPath = Uri_ComposePath(mTex->external);

    Str_Init(&foundPath);
    if(F_FindResourceStr2(RC_GRAPHIC, searchPath, &foundPath) != 0 &&
       GL_LoadImage(image, Str_Text(&foundPath)))
    {
        result = 2;
    }

    if(result == 0)
        Con_Message("GL_LoadMaskTexture: Warning, failed to load \"%s\"\n", Str_Text(searchPath));

    Str_Delete(searchPath);
    Str_Free(&foundPath);

    return result;
    }
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadModelSkin(image_t* image, const gltexture_inst_t* inst, void* context)
{
    assert(image && inst);
    {
    skinname_t* sn = &skinNames[inst->tex->ofTypeID];
    ddstring_t foundPath;
    byte result = 0;

    // Skin name paths are always absolute, so simply locate using it.
    Str_Init(&foundPath);
    if(F_FindResource2(RC_GRAPHIC, sn->path, &foundPath) != 0 &&
       GL_LoadImage(image, Str_Text(&foundPath)))
    {
        result = 2;
    }
    Str_Free(&foundPath);

    if(result == 0)
        Con_Message("GL_LoadModelSkin: Warning, failed to load \"%s\"\n", sn->path);

    return result;
    }
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadModelShinySkin(image_t* image, const gltexture_inst_t* inst, void* context)
{
    assert(image && inst);
    {
    skinname_t* sn = &skinNames[inst->tex->ofTypeID];
    ddstring_t foundPath;
    byte result = 0;

    Str_Init(&foundPath);
    if(F_FindResource3(RC_GRAPHIC, sn->path, &foundPath, "-ck") != 0 &&
       GL_LoadImage(image, Str_Text(&foundPath)))
    {
        result = 2;
    }
    Str_Free(&foundPath);

    if(result == 0)
        Con_Message("GL_LoadModelShinySkin: Warning, failed to load \"%s\"\n", sn->path);

    return result;
    }
}

/**
 * Prepare a texture used in the lighting system. 'which' must be one
 * of the LST_* constants.
 */
DGLuint GL_PrepareLSTexture(lightingtexid_t which)
{
    struct lstex_s {
        const char*     name;
        int             wrapS, wrapT;
    } lstexes[NUM_LIGHTING_TEXTURES] = {
        { "dLight",     GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE },
        { "wallglow",   GL_REPEAT,          GL_CLAMP_TO_EDGE },
        { "radioCO",    GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE },
        { "radioCC",    GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE },
        { "radioOO",    GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE },
        { "radioOE",    GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE }
    };

    if(which < 0 || which >= NUM_LIGHTING_TEXTURES)
        return 0;

    if(!lightingTextures[which].tex)
    {
        lightingTextures[which].tex = GL_PrepareExtTexture(lstexes[which].name, LGM_WHITE_ALPHA,
            false, GL_LINEAR, GL_LINEAR, -1 /*best anisotropy*/, lstexes[which].wrapS,
            lstexes[which].wrapT, TXCF_NO_COMPRESSION);
    }
    return lightingTextures[which].tex;
}

DGLuint GL_PrepareSysFlareTexture(flaretexid_t flare)
{
    if(flare < 0 || flare >= NUM_SYSFLARE_TEXTURES)
        return 0;

    if(!sysFlareTextures[flare].tex)
    {
        // We don't want to compress the flares (banding would be noticeable).
        sysFlareTextures[flare].tex = GL_PrepareExtTexture(flare == 0 ? "dlight" : flare == 1 ? "flare" : flare == 2 ? "brflare" : "bigflare", LGM_WHITE_ALPHA,
            false, GL_NEAREST, GL_LINEAR, 0 /*no anisotropy*/, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
            TXCF_NO_COMPRESSION);
    }
    if(sysFlareTextures[flare].tex == 0)
        Con_Error("GL_PrepareSysFlareTexture: Error, flare texture %i not found!\n", flare);
    return sysFlareTextures[flare].tex;
}

/**
 * @return              The outcome:
 *                      0 = not prepared
 *                      1 = found and prepared a lump resource.
 *                      2 = found and prepared an external resource.
 */
byte GL_LoadExtTexture(image_t* image, const char* name, gfxmode_t mode)
{
    ddstring_t foundPath;
    byte result = 0;

    Str_Init(&foundPath);
    if(F_FindResource2(RC_GRAPHIC, name, &foundPath) != 0 &&
       GL_LoadImage(image, Str_Text(&foundPath)))
    {
        // Too big for us? \todo Should not be done here.
        if(image->width  > GL_state.maxTexSize || image->height > GL_state.maxTexSize)
        {
            int newWidth = MIN_OF(image->width, GL_state.maxTexSize);
            int newHeight = MIN_OF(image->height, GL_state.maxTexSize);
            uint8_t* scaledPixels = GL_ScaleBuffer32(image->pixels, image->width, image->height, image->pixelSize, newWidth, newHeight);
            if(scaledPixels != image->pixels)
            {
                free(image->pixels);
                image->pixels = scaledPixels;
                image->width = newWidth;
                image->height = newHeight;
            }
        }

        // Force it to grayscale?
        if(mode == LGM_GRAYSCALE_ALPHA || mode == LGM_WHITE_ALPHA)
        {
            GL_ConvertToAlpha(image, mode == LGM_WHITE_ALPHA);
        }
        else if(mode == LGM_GRAYSCALE)
        {
            GL_ConvertToLuminance(image);
        }
        result = 2; // External.
    }
    Str_Free(&foundPath);
    return result;
}

/**
 * @return              The outcome:
 *                      0 = not prepared
 *                      1 = found and prepared a lump resource.
 *                      2 = found and prepared an external resource.
 */
byte GL_LoadFlat(image_t* image, const gltexture_inst_t* inst, void* context)
{
    assert(image && inst);
    {
    flat_t* flat = R_GetFlatForIdx(inst->tex->ofTypeID);
    byte result = 0;

    // Try to load a high resolution replacement for this flat?
    if(!noHighResTex && (loadExtAlways || highResWithPWAD || GLTexture_IsFromIWAD(inst->tex)))
    {
        ddstring_t searchPath, foundPath, suffix = { "-ck" };

        // First try the flats namespace then the old-fashioned "flat-name" in the textures namespace.
        Str_Init(&searchPath); Str_Appendf(&searchPath, FLATS_RESOURCE_NAMESPACE_NAME":%s;" TEXTURES_RESOURCE_NAMESPACE_NAME":flat-%s;", flat->name, flat->name);
        Str_Init(&foundPath);

        if(F_FindResourceStr3(RC_GRAPHIC, &searchPath, &foundPath, &suffix) != 0 &&
           GL_LoadImage(image, Str_Text(&foundPath)))
        {
            result = 2;
        }

        Str_Free(&searchPath);
        Str_Free(&foundPath);

        if(result != 0)
            return result;
    }

    // None found. Load the original version.
    { DFILE* file;
    if(NULL != (file = F_OpenLump(flat->name, false)))
    {
        if(0 != GL_LoadImageDFile(image, file, flat->name))
        {
            result = 1;
        }
        else
        {   // It must be an old-fashioned DOOM flat.
#define FLAT_WIDTH          64
#define FLAT_HEIGHT         64

            size_t fileLength = F_Length(file);
            size_t bufSize = MAX_OF(fileLength, 4096);

            initImage(image);
            image->pixels = malloc(bufSize);
            if(fileLength < bufSize)
                memset(image->pixels, 0, bufSize);

            // Load the raw image data.
            F_Read(image->pixels, fileLength, file);
            /// \fixme not all flats are 64x64!
            image->width = FLAT_WIDTH;
            image->height = FLAT_HEIGHT;
            image->pixelSize = 1;
            result = 1;

#undef FLAT_HEIGHT
#undef FLAT_WIDTH
        }
        F_Close(file);
    }}

    return result;
    }
}

/**
 * Set mode to 2 to include an alpha channel. Set to 3 to make the
 * actual pixel colors all white.
 */
DGLuint GL_PrepareExtTexture(const char* name,
    gfxmode_t mode, int useMipmap, int minFilter, int magFilter, int anisoFilter,
    int wrapS, int wrapT, int otherFlags)
{
    image_t image;
    DGLuint texture = 0;

    if(GL_LoadExtTexture(&image, name, mode))
    {   // Loaded successfully and converted accordingly.
        // Upload the image to GL.
        texture = GL_NewTextureWithParams2(
            ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
              image.pixelSize == 3 ? DGL_RGB :
              image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
            image.width, image.height, image.pixels,
            ( otherFlags | (useMipmap? TXCF_MIPMAP : 0) |
              (useMipmap == DDMAXINT? TXCF_GRAY_MIPMAP : 0) |
              (image.width < 128 && image.height < 128? TXCF_NO_COMPRESSION : 0) ),
              (useMipmap ? glmode[mipmapping] : GL_LINEAR),
              magFilter, texAniso, wrapS, wrapT);

        GL_DestroyImage(&image);
    }

    return texture;
}

// Posts are runs of non masked source pixels.
typedef struct {
    byte topdelta; // @c -1 is the last post in a column.
    byte length;
    // Length data bytes follows.
} post_t;

// column_t is a list of 0 or more post_t, (uint8_t)-1 terminated
typedef post_t column_t;

/**
 * \important: The buffer must have room for the new alpha data!
 *
 * @param dstBuf  The destination buffer the patch will be drawn to.
 * @param texwidth  Width of the dst buffer in pixels.
 * @param texheight  Height of the dst buffer in pixels.
 * @param patch  Ptr to the patch structure to draw to the dst buffer.
 * @param origx  X coordinate in the dst buffer to draw the patch too.
 * @param origy  Y coordinate in the dst buffer to draw the patch too.
 * @param trans  If @c >= 0 id number of the translation table to use.
 * @param maskZero  Used with sky textures.
 * @param checkForAlpha If @c true, the composited image will be checked
 *     for alpha pixels and will return accordingly if present.
 *
 * @return  If @a checkForAlpha @c = false will return @c false. Else,
 *          @c true iff the buffer really has alpha information.
 */
static int loadDoomPatch(uint8_t* buffer, int texwidth, int texheight,
    const doompatch_header_t* patch, int origx, int origy, int trans,
    boolean maskZero, boolean checkForAlpha)
{
    assert(buffer && texwidth > 0 && texheight > 0 && patch);
    {
    int count, col = 0;
    int x = origx, y, top; // Keep track of pos (clipping).
    int w = SHORT(patch->width);
    size_t bufsize = texwidth * texheight;
    const column_t* column;
    const uint8_t* source;
    uint8_t* dest1, *dest2, *destTop, *destAlphaTop;
    // Column offsets begin immediately following the header.
    const int32_t* columnOfs = (const int32_t*)((const uint8_t*) patch + sizeof(doompatch_header_t));
    // \todo Validate column offset is within the Patch!

    destTop = buffer + origx;
    destAlphaTop = buffer + origx + bufsize;

    for(; col < w; ++col, destTop++, destAlphaTop++, ++x)
    {
        column = (const column_t*) ((const uint8_t*) patch + LONG(columnOfs[col]));
        top = -1;

        // Step through the posts in a column
        while(column->topdelta != 0xff)
        {
            source = (uint8_t*) column + 3;

            if(x < 0 || x >= texwidth)
                break; // Out of bounds.

            if(column->topdelta <= top)
                top += column->topdelta;
            else
                top = column->topdelta;

            if((count = column->length) > 0)
            {
                y = origy + top;
                dest1 = destTop + y * texwidth;
                dest2 = destAlphaTop + y * texwidth;

                while(count--)
                {
                    uint8_t palidx = *source++;

                    if(trans >= 0)
                    {
                        /// \fixme Check bounds!
                        palidx = *(translationTables + trans + palidx);
                    }

                    // Is the destination within bounds?
                    if(y >= 0 && y < texheight)
                    {
                        if(!maskZero || palidx)
                            *dest1 = palidx;

                        if(maskZero)
                            *dest2 = (palidx ? 0xff : 0);
                        else
                            *dest2 = 0xff;
                    }

                    // One row down.
                    dest1 += texwidth;
                    dest2 += texwidth;
                    ++y;
                }
            }

            column = (const column_t*) ((const uint8_t*) column + column->length + 4);
        }
    }

    if(checkForAlpha)
    {
        boolean allowSingleAlpha = (texwidth < 128 || texheight < 128);
        int i;

        // Scan through the RGBA buffer and check for sub-0xff alpha.
        source = buffer + texwidth * texheight;
        for(i = 0, count = 0; i < texwidth * texheight; ++i)
        {
            if(source[i] < 0xff)
            {
                /// \kludge 'Small' textures tolerate no alpha.
                if(allowSingleAlpha)
                    return true;

                // Big ones can have a single alpha pixel (ZZZFACE3!).
                if(count++ > 1)
                    return true; // Has alpha data.
            }
        }
    }

    return false; // Doesn't have alpha data.
    }
}

/**
 * Renders the given texture into the buffer.
 */
static boolean bufferTexture(const doomtexturedef_t* texDef, uint8_t* buffer,
 int width, int height, int* hasBigPatch)
{
    boolean hasAlpha = false;

    // Clear the buffer.
    memset(buffer, 0, 2 * width * height); // Pal8 plus 8-bit alpha.

    // By default zero is put in the big patch height.
    if(hasBigPatch)
        *hasBigPatch = 0;

    // Draw all the patches. Check for alpha pixels after last patch has
    // been drawn.
    { int i;
    for(i = 0; i < texDef->patchCount; ++i)
    {
        const texpatch_t* patchDef = &texDef->patches[i];
        const doompatch_header_t* patch =
            (doompatch_header_t*) W_CacheLumpNum(patchDef->lump, PU_CACHE);

        // Check for big patches?
        if(hasBigPatch && SHORT(patch->height) > texDef->height &&
           *hasBigPatch < SHORT(patch->height))
        {
            *hasBigPatch = SHORT(patch->height);
        }

        // Draw the patch in the buffer.
        hasAlpha = loadDoomPatch(buffer, /*palette,*/ width, height, patch,
            patchDef->offX, patchDef->offY, -1, false, i == texDef->patchCount - 1);
    }}

    return hasAlpha;
}

/**
 * Draws the given sky texture in a buffer. The returned buffer must be
 * freed by the caller. Idx must be a valid texture number.
 */
static void bufferSkyTexture(const doomtexturedef_t* texDef, byte** outbuffer,
                             int width, int height, boolean zeroMask)
{
    int                 i, numpels;
    byte*               imgdata;

    if(texDef->patchCount > 1)
    {
        numpels = width * height;
        imgdata = M_Calloc(2 * numpels);

        for(i = 0; i < texDef->patchCount; ++i)
        {
            const texpatch_t*   patchDef = &texDef->patches[i];

            loadDoomPatch(imgdata, /*palette,*/ width, height,
                          W_CacheLumpNum(patchDef->lump, PU_CACHE),
                          patchDef->offX, patchDef->offY, -1,
                          zeroMask, false);
        }
    }
    else
    {
        doompatch_header_t *patch =
            (doompatch_header_t*) W_CacheLumpNum(texDef->patches[0].lump, PU_CACHE);
        int     bufHeight =
            SHORT(patch->height) > height ? SHORT(patch->height) : height;

        if(bufHeight > height)
        {
            // Heretic sky textures are reported to be 128 tall, even if the
            // data is 200. We'll adjust the real height of the texture up to
            // 200 pixels (remember Caldera?).
            height = bufHeight;
            if(height > 200)
                height = 200;
        }

        // Allocate a large enough buffer.
        numpels = width * bufHeight;
        imgdata = M_Calloc(2 * numpels);
        loadDoomPatch(imgdata, /*palette,*/ width, bufHeight, patch, 0, 0, -1,
                      zeroMask, false);
    }

    *outbuffer = imgdata;
}

/**
 * @return              The outcome:
 *                      0 = not loaded.
 *                      1 = loaded data from a lump resource.
 *                      2 = loaded data from an external resource.
 */
byte GL_LoadDoomTexture(image_t* image, const gltexture_inst_t* inst, void* context)
{
    int i, flags = 0;
    doomtexturedef_t* texDef;
    boolean loadAsSky = false, zeroMask = false;
    material_load_params_t* params = (material_load_params_t*) context;

    if(!image)
        return 0; // Wha?

    if(params)
    {
        loadAsSky = (params->flags & MLF_LOAD_AS_SKY)? true : false;
        zeroMask = (params->tex.flags & GLTF_ZEROMASK)? true : false;
    }

    texDef = R_GetDoomTextureDef(inst->tex->ofTypeID);

    // Try to load a high resolution version of this texture?
    if(!noHighResTex && (loadExtAlways || highResWithPWAD || GLTexture_IsFromIWAD(inst->tex)))
    {
        ddstring_t searchPath, foundPath, suffix = { "-ck" };
        byte result = 0;

        Str_Init(&searchPath); Str_Appendf(&searchPath, TEXTURES_RESOURCE_NAMESPACE_NAME":%s;", texDef->name);
        Str_Init(&foundPath);

        if(F_FindResourceStr3(RC_GRAPHIC, &searchPath, &foundPath, &suffix) != 0 &&
           GL_LoadImage(image, Str_Text(&foundPath)))
        {
            result = 2; // High resolution texture loaded.
        }

        Str_Free(&searchPath);
        Str_Free(&foundPath);

        if(result != 0)
            return result;
    }

    // None found. Load the original version.
    initImage(image);
    image->width = texDef->width;
    image->height = texDef->height;
    image->pixelSize = 1;

    if(loadAsSky)
    {
        bufferSkyTexture(texDef, &image->pixels, image->width, image->height, zeroMask);
        if(zeroMask)
            image->flags |= IMGF_IS_MASKED;
    }
    else
    {
        boolean isMasked;

        /**
         * \todo if we are resizing masked textures re match patches then
         * we are needlessly duplicating work.
         */
        image->pixels = M_Malloc(2 * image->width * image->height);
        isMasked = bufferTexture(texDef, image->pixels, image->width, image->height, &i);

        // The -bigmtex option allows the engine to resize masked
        // textures whose patches are too big to fit the texture.
        if(allowMaskedTexEnlarge && isMasked && i)
        {
            // Adjust the defined height to fit the largest patch.
            texDef->height = image->height = i;
            // Get a new buffer.
            M_Free(image->pixels);
            image->pixels = M_Malloc(2 * image->width * image->height);
            isMasked = bufferTexture(texDef, image->pixels, image->width, image->height, 0);
        }

        if(isMasked)
            image->flags |= IMGF_IS_MASKED;
    }

    return 1;
}

/**
 * @return              The outcome:
 *                      0 = not prepared
 *                      1 = found and prepared a lump resource.
 *                      2 = found and prepared an external resource.
 */
byte GL_LoadDoomPatch(image_t* image, const gltexture_inst_t* inst,
    void* context)
{
    material_load_params_t* params = (material_load_params_t*) context;
    boolean scaleSharp = false;
    patchtex_t* p;

    if(!image)
        return 0; // Wha?

    p = R_FindPatchTex(inst->tex->ofTypeID);

    if(params)
    {
        scaleSharp = (params->tex.flags & GLTF_UPSCALE_AND_SHARPEN) != 0;
    }

    // Try to load an external replacement for this version of the patch?
    if(!noHighResTex && (loadExtAlways || highResWithPWAD || GLTexture_IsFromIWAD(inst->tex)))
    {
        ddstring_t searchPath, foundPath, suffix = { "-ck" };
        byte result = 0;

        Str_Init(&searchPath); Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s;", W_LumpName(p->lump));
        Str_Init(&foundPath);

        if(F_FindResourceStr3(RC_GRAPHIC, &searchPath, &foundPath, &suffix) != 0 &&
           GL_LoadImage(image, Str_Text(&foundPath)))
        {
            result = 2;
        }

        Str_Free(&searchPath);
        Str_Free(&foundPath);

        if(result != 0)
            return result;
    }

    // Use data from the normal lump.
    if(p->width * p->height)
    {
        const doompatch_header_t* patch = W_CacheLumpNum(p->lump, PU_APPSTATIC);
        initImage(image);
        image->pixelSize = 1;
        image->width = SHORT(p->width);
        image->height = SHORT(p->height);
        if(scaleSharp)
        {
            image->width += 2;
            image->height += 2;
        }
        image->pixels = M_Calloc(2 * image->width * image->height);
        if(loadDoomPatch(image->pixels, image->width, image->height, patch, scaleSharp? 1 : 0, scaleSharp? 1 : 0, -1, false, true))
            image->flags |= IMGF_IS_MASKED;
        W_ChangeCacheTag(p->lump, PU_CACHE);
        return 1;
    }

    return 0;
}

/**
 * @return  0 = not loaded.
 *          1 = loaded data from an original resource.
 *          2 = loaded data from a replacement resource.
 */
byte GL_LoadSprite(image_t* image, const gltexture_inst_t* inst, void* context)
{
    material_load_params_t* params = (material_load_params_t*) context;
    const spritetex_t*  sprTex;
    int tmap = 0, tclass = 0;
    boolean pSprite = false;
    byte border = 0;

    if(!image)
        return 0; // Wha?

    sprTex = R_SpriteTextureForIndex(inst->tex->ofTypeID);
    assert(NULL != sprTex);

    if(params)
    {
        tmap = params->tmap;
        tclass = params->tclass;
        pSprite = params->pSprite;
        border = params->tex.border;
    }

    // Attempt to load a high resolution replacement for this sprite?
    if(!noHighResPatches)
    {
        ddstring_t searchPath, foundPath, suffix = { "-ck" };
        byte result = 0;

        // Compose resource names, prefer translated or psprite versions.
        Str_Init(&searchPath);
        Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s;", sprTex->name);
        if(pSprite || tclass || tmap)
        {
            if(pSprite)
                Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s-hud;", sprTex->name);
            else // Translated.
                Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s-table%i%i;", sprTex->name, tclass, tmap);
        }
        Str_Init(&foundPath);

        if(F_FindResourceStr3(RC_GRAPHIC, &searchPath, &foundPath, &suffix) != 0 &&
           GL_LoadImage(image, Str_Text(&foundPath)))
        {
            result = 2;
        }

        Str_Free(&searchPath);
        Str_Free(&foundPath);

        if(result != 0)
            return result;
    }

    {
    lumpnum_t lumpNum = W_GetNumForName(sprTex->name);
    boolean freePatch = false;
    const doompatch_header_t*  patch;
    int trans = -1;

    initImage(image);
    image->width = sprTex->width+border*2;
    image->height = sprTex->height+border*2;
    image->pixelSize = 1;
    image->pixels = M_Calloc(2 * image->width * image->height);

    if(tclass || tmap)
    {   // We need to translate the patch.
        trans = MAX_OF(0, -256 + tclass * ((8-1) * 256) + tmap * 256);
    }

    patch = W_CacheLumpNum(lumpNum, PU_APPSTATIC);
    if(loadDoomPatch(image->pixels, image->width, image->height, patch, border, border, trans, false, true))
        image->flags |= IMGF_IS_MASKED;
    W_ChangeCacheTag(lumpNum, PU_CACHE);

    return 1;
    }
}

void GL_SetPSprite(material_t* mat)
{
    material_load_params_t params;
    material_snapshot_t ms;

    memset(&params, 0, sizeof(params));
    params.pSprite = true;
    params.tex.border = 1;

    Materials_Prepare(&ms, mat, true, &params);

    GL_BindTexture(ms.units[MTU_PRIMARY].texInst->id, ms.units[MTU_PRIMARY].magMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void GL_SetTranslatedSprite(material_t* mat, int tclass, int tmap)
{
    material_snapshot_t ms;
    material_load_params_t params;

    memset(&params, 0, sizeof(params));
    params.tmap = tmap;
    params.tclass = tclass;
    params.tex.border = 1;

    Materials_Prepare(&ms, mat, true, &params);
    GL_BindTexture(ms.units[MTU_PRIMARY].texInst->id, ms.units[MTU_PRIMARY].magMode);
}

/**
 * @return              The outcome:
 *                      0 = not prepared
 *                      1 = found and prepared a lump resource.
 *                      2 = found and prepared an external resource.
 */
byte GL_LoadRawTex(image_t* image, const rawtex_t* r)
{
    assert(image);
    {
    const char* lumpName = W_LumpName(r->lump);
    ddstring_t searchPath, foundPath;
    byte result = 0;

    // First try to find an external resource.
    Str_Init(&searchPath); Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s;", lumpName);
    Str_Init(&foundPath);

    if(F_FindResourceStr2(RC_GRAPHIC, &searchPath, &foundPath) != 0 &&
       GL_LoadImage(image, Str_Text(&foundPath)))
    {   // "External" image loaded.
        result = 2;
    }
    else
    {
        DFILE* file;
        if(NULL != (file = F_OpenLump(lumpName, false)))
        {
            if(0 != GL_LoadImageDFile(image, file, lumpName))
            {
                result = 1;
            }
            else
            {   // It must be an old-fashioned "raw" image.
#define RAW_WIDTH           320
#define RAW_HEIGHT          200

                size_t fileLength = F_Length(file);
                size_t bufSize = 3 * RAW_WIDTH * RAW_HEIGHT;

                initImage(image);
                image->pixels = malloc(bufSize);
                if(fileLength < bufSize)
                    memset(image->pixels, 0, bufSize);

                // Load the raw image data.
                F_Read(image->pixels, fileLength, file);
                image->width = RAW_WIDTH;
                image->height = (int) (fileLength / image->width);
                image->pixelSize = 1;
                result = 1;

#undef RAW_HEIGHT
#undef RAW_WIDTH
            }
            F_Close(file);
        }
    }

    Str_Free(&searchPath);
    Str_Free(&foundPath);

    return result;
    }
}

/**
 * Raw images are always 320x200.
 *
 * 2003-05-30 (skyjake): External resources can be larger than 320x200,
 * but they're never split into two parts.
 */
DGLuint GL_PrepareRawTex2(rawtex_t* raw)
{
    if(!raw)
        return 0; // Wha?

    if(raw->lump < 0 || raw->lump >= W_NumLumps())
    {
        GL_BindTexture(0, 0);
        return 0;
    }

    if(!raw->tex)
    {
        image_t image;
        byte result;

        // Clear any old values.
        memset(&image, 0, sizeof(image));

        if((result = GL_LoadRawTex(&image, raw)) == 2)
        {   // Loaded an external raw texture.
            // We have the image in the buffer. We'll upload it as one big texture.
            raw->tex =
                GL_UploadTexture(image.pixels, image.width, image.height,
                                 image.pixelSize == 4, false, true, false, false,
                                 GL_NEAREST, (filterUI ? GL_LINEAR : GL_NEAREST),
                                 0 /*no anisotropy*/,
                                 GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                 (image.pixelSize == 4? TXCF_APPLY_GAMMACORRECTION : 0));

            raw->width = 320;
            raw->height = 200;

            GL_DestroyImage(&image);
        }
        else
        {
            boolean rgbdata = (image.pixelSize > 1? true:false);
            int assumedWidth = GL_state.textureNonPow2? image.width : 256;

            // Generate a texture.
            raw->tex = GL_UploadTexture(image.pixels, GL_state.textureNonPow2? image.width : 256, image.height,
                false, false, rgbdata, false, false, GL_NEAREST,
                (filterUI? GL_LINEAR:GL_NEAREST), 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                (rgbdata? TXCF_APPLY_GAMMACORRECTION : 0));

            raw->width = GL_state.textureNonPow2? image.width : 256;
            raw->height = image.height;

            GL_DestroyImage(&image);
        }
    }

    return raw->tex;
}

/**
 * Returns the OpenGL name of the texture.
 */
DGLuint GL_PrepareRawTex(rawtex_t* rawTex)
{
    if(rawTex)
    {
        if(!rawTex->tex)
        {   // The rawtex isn't yet bound with OpenGL.
            rawTex->tex = GL_PrepareRawTex2(rawTex);
        }

        return rawTex->tex;
    }

    return 0;
}

void GL_SetRawImage(lumpnum_t lump, int wrapS, int wrapT)
{
    rawtex_t* rawTex;

    if((rawTex = R_GetRawTex(lump)))
    {
        DGLuint tex = GL_PrepareRawTex(rawTex);

        GL_BindTexture(tex, (filterUI ? GL_LINEAR : GL_NEAREST));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    }
}

DGLuint GL_GetLightMapTexture(const dduri_t* uri)
{
    lightmap_t* lmap;

    if(uri)
    {
        if(!Str_CompareIgnoreCase(Uri_Path(uri), "-"))
            return 0;

        if((lmap = R_GetLightMap(uri)) != 0)
        {
            const gltexture_inst_t* texInst;
            if((texInst = GL_PrepareGLTexture(lmap->id, 0, 0)))
                return texInst->id;
        }
    }

    // Return the default texture name.
    return GL_PrepareLSTexture(LST_DYNAMIC);
}

/**
 * Attempt to locate and prepare a flare texture.
 * Somewhat more complicated than it needs to be due to the fact there
 * are two different selection methods.
 *
 * @param name          Name of a flare texture or "0" to "4".
 * @param oldIdx        Old method of flare texture selection, by id.
 */
DGLuint GL_GetFlareTexture(const dduri_t* uri, int oldIdx)
{
    if(uri)
    {
        const ddstring_t* path = Uri_Path(uri);
        flaretex_t* fTex;

        if(Str_At(path, 0) == '-' || (Str_At(path, 0) == '0' && !Str_At(path, 1)))
            return 0; // Use the automatic selection logic.

        if(Str_At(path, 0) >= '1' && Str_At(path, 0) <= '4' && !Str_At(path, 1))
            return GL_PrepareSysFlareTexture(Str_At(path, 0) - '1');

        if((fTex = R_GetFlareTexture(uri)))
        {
            const gltexture_inst_t* texInst;
            if((texInst = GL_PrepareGLTexture(fTex->id, 0, 0)))
                return texInst->id;
        }
    }
    else if(oldIdx > 0 && oldIdx < NUM_SYSFLARE_TEXTURES)
    {
        return GL_PrepareSysFlareTexture(oldIdx-1);
    }
    return 0; // Use the automatic selection logic.
}

/**
 * @param pixels  RGBA data. Input read here, and output written here.
 * @param width   Width of the buffer.
 * @param height  Height of the buffer.
 */
static void BlackOutlines(byte* pixels, int width, int height)
{
    short*  dark = M_Calloc(width * height * 2);
    int     x, y;
    int     a, b;
    byte*   pix;

    for(y = 1; y < height - 1; ++y)
    {
        for(x = 1; x < width - 1; ++x)
        {
            pix = pixels + (x + y * width) * 4;
            if(pix[3] > 128) // Not transparent.
            {
                // Apply darkness to surrounding transparent pixels.
                for(a = -1; a <= 1; ++a)
                {
                    for(b = -1; b <= 1; ++b)
                    {
                        byte* other = pix + (a + b * width) * 4;
                        if(other[3] < 128) // Transparent.
                        {
                            dark[(x + a) + (y + b) * width] += 40;
                        }
                    }
                }
            }
        }
    }

    // Apply the darkness.
    for(a = 0, pix = pixels; a < width * height; ++a, pix += 4)
    {
        uint c = pix[3] + dark[a];
        pix[3] = MIN_OF(255, c);
    }

    // Release temporary storage.
    M_Free(dark);
}

static void Equalize(byte* pixels, int width, int height,
    float* rBaMul, float* rHiMul, float* rLoMul)
{
    byte min = 255, max = 0;
    int i, avg = 0;
    float hiMul, loMul, baMul;
    byte* pix;

    for(i = 0, pix = pixels; i < width * height; ++i, pix += 1)
    {
        if(*pix < min) min = *pix;
        if(*pix > max) max = *pix;
        avg += *pix;
    }

    if(max <= min || max == 0 || min == 255)
    {
        if(rBaMul) *rBaMul = -1;
        if(rHiMul) *rHiMul = -1;
        if(rLoMul) *rLoMul = -1;
        return; // Nothing we can do.
    }

    avg /= width * height;

    // Allow a small margin of variance with the balance multiplier.
    baMul = (!INRANGE_OF(avg, 127, 4)? (float)127/avg : 1);
    if(baMul != 1)
    {
        if(max < 255)
            max = (byte) MINMAX_OF(1, (float)max - (255-max) * baMul, 255);
        if(min > 0)
            min = (byte) MINMAX_OF(0, (float)min + min * baMul, 255);
    }

    hiMul = (max < 255?    (float)255/max  : 1);
    loMul = (min > 0  ? 1-((float)min/255) : 1);

    if(!(baMul == 1 && hiMul == 1 && loMul == 1))
        for(i = 0, pix = pixels; i < width * height; ++i, pix += 1)
        {
            // First balance.
            *pix = (byte) MINMAX_OF(0, ((float)*pix) * baMul, 255);

            // Now amplify.
            if(*pix > 127)
                *pix = (byte) MINMAX_OF(0, ((float)*pix) * hiMul, 255);
            else
                *pix = (byte) MINMAX_OF(0, ((float)*pix) * loMul, 255);
        }

    if(rBaMul) *rBaMul = baMul;
    if(rHiMul) *rHiMul = hiMul;
    if(rLoMul) *rLoMul = loMul;
}

static void Desaturate(byte* pixels, int width, int height, int comps)
{
    byte* pix;
    int i;

    for(i = 0, pix = pixels; i < width * height; ++i, pix += comps)
    {
        int min = MIN_OF(pix[CR], MIN_OF(pix[CG], pix[CB]));
        int max = MAX_OF(pix[CR], MAX_OF(pix[CG], pix[CB]));
        pix[CR] = pix[CG] = pix[CB] = (min + max) / 2;
    }
}

static void Amplify(byte* pixels, int width, int height, boolean hasAlpha)
{
    size_t i, numPels = width * height;
    byte max = 0;
    byte* pix;

    if(hasAlpha)
    {
        byte* apix;
        for(i = 0, pix = pixels, apix = pixels + numPels; i < numPels; ++i, pix++, apix++)
        {
            // Only non-masked pixels count.
            if(!(*apix > 0))
                continue;

            if(*pix > max)
                max = *pix;
        }
    }
    else
    {
        for(i = 0, pix = pixels; i < numPels; ++i, pix++)
        {
            if(*pix > max)
                max = *pix;
        }
    }

    if(!max || max == 255)
        return;

    for(i = 0, pix = pixels; i < numPels; ++i, pix++)
    {
        *pix = (byte) MINMAX_OF(0, (float)*pix / max * 255, 255);
    }
}

static void EnhanceContrast(byte *pixels, int width, int height)
{
    int i, c;
    byte* pix;

    for(i = 0, pix = pixels; i < width * height; ++i, pix += 4)
    {
        for(c = 0; c < 3; ++c)
        {
            float f = pix[c];
            if(f < 60) // Darken dark parts.
                f = (f - 70) * 1.0125f + 70;
            else if(f > 185) // Lighten light parts.
                f = (f - 185) * 1.0125f + 185;

            pix[c] = MINMAX_OF(0, f, 255);
        }
    }
}

static void SharpenPixels(byte* pixels, int width, int height)
{
    int x, y, c;
    byte* result = M_Calloc(4 * width * height);
    const float strength = .05f;
    float A, B, C;

    A = strength;
    B = .70710678 * strength; // 1/sqrt(2)
    C = 1 + 4*A + 4*B;

    for(y = 1; y < height - 1; ++y)
    {
        for(x = 1; x < width -1; ++x)
        {
            byte* pix = pixels + (x + y*width) * 4;
            byte* out = result + (x + y*width) * 4;
            for(c = 0; c < 3; ++c)
            {
                int r = (C*pix[c] - A*pix[c - width] - A*pix[c + 4] - A*pix[c - 4] -
                         A*pix[c + width] - B*pix[c + 4 - width] - B*pix[c + 4 + width] -
                         B*pix[c - 4 - width] - B*pix[c - 4 + width]);
                out[c] = MINMAX_OF(0, r, 255);
            }
            out[3] = pix[3];
        }
    }
    memcpy(pixels, result, 4 * width * height);
    M_Free(result);
}

/**
 * Returns the OpenGL name of the texture.
 */
DGLuint GL_PreparePatch(patchtex_t* patchTex)
{
    if(patchTex)
    {
        material_load_params_t params;
        const gltexture_inst_t* texInst;
        memset(&params, 0, sizeof(params));
        if(patchTex->flags & PF_MONOCHROME)
            params.tex.flags |= GLTF_MONOCHROME;
        if(patchTex->flags & PF_UPSCALE_AND_SHARPEN)
        {
            params.tex.flags |= GLTF_UPSCALE_AND_SHARPEN;
            params.tex.border = 1;
        }
        if((texInst = GL_PrepareGLTexture(patchTex->texId, &params, NULL)))
            return texInst->id;
    }
    return 0;
}

/**
 * You should use glDisable(GL_TEXTURE_2D) instead of this.
 */
void GL_SetNoTexture(void)
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void setTextureMinMode(DGLuint tex, int minMode)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minMode);
}

void GL_SetRawTextureParams(int minMode)
{
    rawtex_t** rawTexs, **ptr;

    rawTexs = R_CollectRawTexs(NULL);
    for(ptr = rawTexs; *ptr; ptr++)
    {
        rawtex_t* r = (*ptr);
        if(r->tex) // Is the texture loaded?
            setTextureMinMode(r->tex, minMode);
    }
    Z_Free(rawTexs);
}

/**
 * Updates the textures, flats and sprites (gameTex) or the user
 * interface textures (patches and raw screens).
 */
void GL_SetTextureParams(int minMode, int gameTex, int uiTex)
{
    if(gameTex)
    {
        GL_SetAllGLTexturesMinMode(minMode);
    }

    if(uiTex)
    {
        GL_SetRawTextureParams(minMode);
    }
}

void GL_UpdateTexParams(int mipmode)
{
    mipmapping = mipmode;
    GL_SetTextureParams(glmode[mipmode], true, false);
}

/**
 * Called when changing the value of any cvar affecting texture quality which
 * can be actioned by simply changing texture paramaters i.e. does not require
 * flushing GL textures).
 */
void GL_DoUpdateTexParams(const cvar_t* cvar)
{
    GL_SetTextureParams(glmode[mipmapping], true, true);
}

static int doTexReset(void* parm)
{
    boolean usingBusyMode = *((boolean*) parm);

    /// \todo re-upload ALL textures currently in use.
    GL_LoadSystemTextures();
    Rend_ParticleLoadExtraTextures();
    R_SkyUpdate();

    if(usingBusyMode)
    {
        Con_SetProgress(200);
        Con_BusyWorkerEnd();
    }
    return 0;
}

void GL_TexReset(void)
{
    boolean useBusyMode = !Con_IsBusy();

    GL_ClearTextureMemory();
    FR_Update();
    Con_Printf("All DGL textures deleted.\n");

    if(useBusyMode)
    {
        Con_InitProgress(200);
        Con_Busy(BUSYF_ACTIVITY | (verbose? BUSYF_CONSOLE_OUTPUT : 0), "Reseting textures...", doTexReset, &useBusyMode);
    }
    else
    {
        doTexReset(&useBusyMode);
    }
}

/**
 * Called when changing the value of the texture gamma cvar.
 */
void GL_DoUpdateTexGamma(const cvar_t* cvar)
{
    if(texInited)
    {
        calcGammaTable();
        GL_InitPalettedTexture();
        GL_TexReset();
    }

    Con_Printf("Gamma correction set to %f.\n", texGamma);
}

/**
 * Called when changing the value of any cvar affecting texture quality which
 * in turn calls GL_TexReset. Added to remove the need for reseting  manually.
 *
 * @param   unused      Unused. Must be signature compatible.
 */
void GL_DoTexReset(const cvar_t* cvar)
{
    GL_TexReset();
}

void GL_DoResetDetailTextures(const cvar_t* cvar)
{
    GL_DeleteAllTexturesForGLTextures(GLT_DETAIL);
}

/**
 * To save texture memory, delete all raw image textures. Raw images are
 * used as interlude backgrounds, title screens, etc. Called from
 * DD_SetupLevel.
 */
void GL_DeleteRawImages(void)
{
    rawtex_t** rawTexs, **ptr;

    rawTexs = R_CollectRawTexs(NULL);
    for(ptr = rawTexs; *ptr; ptr++)
    {
        rawtex_t* r = (*ptr);
        if(r->tex)
        {
            glDeleteTextures(1, (const GLuint*) &r->tex);
            r->tex = 0;
        }
    }
    Z_Free(rawTexs);
}

/**
 * Returns true if the given path name refers to an image, which should
 * be color keyed. Color keying is done for both (0,255,255) and
 * (255,0,255).
 */
boolean GL_IsColorKeyed(const char* path)
{
    filename_t          buf;

    strncpy(buf, path, FILENAME_T_MAXLEN);
    strlwr(buf);
    return strstr(buf, "-ck.") != NULL;
}

void GL_SetMaterial(material_t* mat)
{
    material_snapshot_t ms;

    if(!mat)
        return; // \fixme we need a "NULL material".

    Materials_Prepare(&ms, mat, true, NULL);
    GL_BindTexture(ms.units[MTU_PRIMARY].texInst->id, ms.units[MTU_PRIMARY].magMode);
}

/**
 * This is a hash function. Given a gltexture name it generates a
 * somewhat-random number between 0 and GLTEXTURE_NAME_HASH_SIZE.
 *
 * @return              The generated hash index.
 */
static uint hashForGLTextureName(const char* name)
{
    ushort              key = 0;
    int                 i;

    // Stop when the name ends.
    for(i = 0; *name; ++i, name++)
    {
        if(i == 0)
            key ^= (int) (*name);
        else if(i == 1)
            key *= (int) (*name);
        else if(i == 2)
        {
            key -= (int) (*name);
            i = -1;
        }
    }

    return key % GLTEXTURE_NAME_HASH_SIZE;
}

/**
 * Given a name and gltexture type, search the gltextures db for a match.
 * \assume Caller knows what it's doing; params arn't validity checked.
 *
 * @param name          Name of the gltexture to search for. Must have been
 *                      transformed to all lower case.
 * @param type          Specific GLT_* gltexture type.
 * @return              Ptr to the found gltexture_t else, @c NULL.
 */
static gltexture_t* getGLTextureByName(const char* name, uint hash,
                                       gltexture_type_t type)
{
    // Go through the candidates.
    if(glTextureTypeData[type].hashTable[hash])
    {
        gltexture_t*      tex =
            glTextures[glTextureTypeData[type].hashTable[hash] - 1];

        for(;;)
        {
            if(tex->type == type && !strncmp(tex->name, name, 8))
                return tex;

            if(!tex->hashNext)
                break;

            tex = glTextures[tex->hashNext - 1];
        }
    }

    return 0; // Not found.
}

static __inline gltexture_t* getGLTexture(gltextureid_t id)
{
    if(id != 0 && id <= numGLTextures)
        return glTextures[id - 1];

    return NULL;
}

/**
 * Updates the minification mode of ALL gltextures.
 *
 * @param minMode       The DGL minification mode to set.
 */
void GL_SetAllGLTexturesMinMode(int minMode)
{
    uint                i;

    for(i = 0; i < numGLTextures; ++i)
    {
        GLTexture_SetMinMode(glTextures[i], minMode);
    }
}

const gltexture_t* GL_CreateGLTexture(const char* rawName, int ofTypeID,
                                      gltexture_type_t type)
{
    uint                i;
    gltexture_t*        tex;

    if(!rawName || !rawName[0])
        return NULL;

#if _DEBUG
if(type < GLT_FIRST || !(type < NUM_GLTEXTURE_TYPES))
    Con_Error("GL_CreateGLTexture: Invalid type %i.", type);
#endif

    // Check if we've already created a gltexture for this.
    for(i = 0; i < numGLTextures; ++i)
    {
        tex = glTextures[i];

        if(tex->type == type && tex->ofTypeID == ofTypeID)
        {
            GLTexture_ReleaseTextures(tex);
            return tex; // Yep, return it.
        }
    }

    /**
     * A new gltexture.
     */

    tex = Z_Malloc(sizeof(*tex), PU_APPSTATIC, 0);
    tex->type = type;
    tex->ofTypeID = ofTypeID;
    tex->instances = NULL;
    tex->id = numGLTextures + 1; // 1-based index.
    // Prepare 'name'.
    for(i = 0; *rawName && i < 8; ++i, rawName++)
        tex->name[i] = tolower(*rawName);
    tex->name[i] = '\0';

    // We also hash the name for faster searching.
    {
    uint                hash = hashForGLTextureName(tex->name);
    tex->hashNext = glTextureTypeData[type].hashTable[hash];
    glTextureTypeData[type].hashTable[hash] = numGLTextures + 1; // 1-based index.
    }

    /**
     * Link the new gltexture into the global list of gltextures.
     */

    // Resize the existing list.
    glTextures =
        Z_Realloc(glTextures, sizeof(gltexture_t*) * ++numGLTextures,
                  PU_APPSTATIC);
    // Add the new gltexture;
    glTextures[numGLTextures - 1] = tex;

    return tex;
}

uint GL_TextureNumForName(const char* name, gltexture_type_t type)
{
    const gltexture_t* glTex;
    if(type < GLT_FIRST || !(type < NUM_GLTEXTURE_TYPES))
        Con_Error("GL_TextureNumForName: Invalid type %i.", type);
    if((glTex = GL_GetGLTextureByName(name, type)))
        return glTex->ofTypeID + 1;
    Con_Error("R_TextureNumForName: Unknown texture '%s' of type %s.", name, GLTEXTURE_TYPE_STRING(type));
    return 0; // Unreachable.
}

uint GL_CheckTextureNumForName(const char* name, gltexture_type_t type)
{
    const gltexture_t* glTex;
    if(type < GLT_FIRST || !(type < NUM_GLTEXTURE_TYPES))
        Con_Error("GL_CheckTextureNumForName: Invalid type %i.", type);
    if((glTex = GL_GetGLTextureByName(name, type)))
        return glTex->ofTypeID + 1;
    Con_Message("GL_CheckTextureNumForName: Warning, unknown texture '%s' of type %s.\n", name, GLTEXTURE_TYPE_STRING(type));
    return 0;
}

const gltexture_inst_t* GL_PrepareGLTexture(gltextureid_t id, void* context,
                                            byte* result)
{
    return GLTexture_Prepare(getGLTexture(id), context, result);
}

void GL_ReleaseGLTexture(gltextureid_t id)
{
    GLTexture_ReleaseTextures(getGLTexture(id));
}

/**
 * Deletes all OpenGL texture instances for ALL gltextures.
 */
void GL_DeleteAllTexturesForGLTextures(gltexture_type_t type)
{
    uint                i;

    if(type != GLT_ANY && (type < 0 || type > NUM_GLTEXTURE_TYPES))
        Con_Error("GL_DeleteAllTexturesForGLTextures: Internal error, "
                  "invalid type %i.", (int) type);

    for(i = 0; i < numGLTextures; ++i)
    {
        gltexture_t*        tex = glTextures[i];

        if(type != GLT_ANY && tex->type != type)
            continue;

        GLTexture_ReleaseTextures(tex);
    }
}

const gltexture_t* GL_GetGLTexture(gltextureid_t id)
{
    return getGLTexture(id);
}

const gltexture_t* GL_GetGLTextureByName(const char* rawName, gltexture_type_t type)
{
    char name[9];
    uint hash;
    int n;

    if(!rawName || !rawName[0])
        return NULL;

    // Prepare 'name'.
    for(n = 0; *rawName && n < 8; ++n, rawName++)
        name[n] = tolower(*rawName);
    name[n] = '\0';
    hash = hashForGLTextureName(name);

    return getGLTextureByName(name, hash, type);
}

const gltexture_t* GL_GetGLTextureByTypeId(int ofTypeId, gltexture_type_t type)
{
    if(type <= GLT_ANY || type >= NUM_GLTEXTURE_TYPES)
        Con_Error("GL_GetGLTextureByTypeId: Internal error, invalid type %i.", (int) type);

    { uint i;
    for(i = 0; i < numGLTextures; ++i)
    {
        gltexture_t* tex = glTextures[i];
        if(tex->type == type && tex->ofTypeID == ofTypeId)
            return tex;
    }}
    return 0; // Not found.
}

static gltexture_inst_t* pickGLTextureInst(gltexture_t* tex, void* context)
{
    material_load_params_t* params = (material_load_params_t*) context;
    gltexture_inst_node_t* node;
    byte texFlags = 0, border = 0;
    int flags = 0;

    if(params)
    {
        flags = params->flags;
        texFlags = params->tex.flags;
        border = params->tex.border;
    }

    node = (gltexture_inst_node_t*) tex->instances;
    while(node)
    {
        if(node->flags == flags && node->inst.flags == texFlags && node->inst.border == border)
            return &node->inst;
        node = node->next;
    }

    return NULL;
}

static gltexture_inst_t* pickDetailTextureInst(gltexture_t* tex, void* context)
{
    gltexture_inst_node_t* node;
    float contrast = *((float*) context);

    node = (gltexture_inst_node_t*) tex->instances;
    while(node)
    {
        if(node->inst.data.detail.contrast == contrast)
            return &node->inst;
        node = node->next;
    }

    return NULL;
}

static gltexture_inst_t* pickSpriteTextureInst(gltexture_t* tex, void* context)
{
    gltexture_inst_node_t* node;
    material_load_params_t* params = (material_load_params_t*) context;
    int tmap = 0, tclass = 0;
    boolean pSprite = false;
    byte texFlags = 0, border = 0;

    if(params)
    {
        tmap = params->tmap;
        tclass = params->tclass;
        pSprite = params->pSprite;
        texFlags = params->tex.flags;
        border = params->tex.border;
    }

    node = (gltexture_inst_node_t*) tex->instances;
    while(node)
    {
        if(node->inst.data.sprite.pSprite == pSprite &&
           node->inst.data.sprite.tmap == tmap &&
           node->inst.data.sprite.tclass == tclass &&
           node->inst.flags == texFlags &&
           node->inst.border == border)
            return &node->inst;
        node = node->next;
    }

    return NULL;
}

gltexture_inst_t* GLTexture_Prepare(gltexture_t* tex, void* context, byte* result)
{
    boolean monochrome    = ((tex->type != GLT_DETAIL && context)? (((material_load_params_t*) context)->tex.flags & GLTF_MONOCHROME) != 0 : false);
    boolean noCompression = ((tex->type != GLT_DETAIL && context)? (((material_load_params_t*) context)->tex.flags & GLTF_NO_COMPRESSION) != 0 : false);
    boolean scaleSharp    = ((tex->type != GLT_DETAIL && context)? (((material_load_params_t*) context)->tex.flags & GLTF_UPSCALE_AND_SHARPEN) != 0 : false);
    gltexture_inst_t* texInst = NULL;
    float contrast = 1;
    byte tmpResult;

    if(tex->type < GLT_FIRST || tex->type >= NUM_GLTEXTURE_TYPES)
    {
        Con_Error("GLTexture_Prepare: Internal error, invalid type %i.", (int) tex->type);
    }

    switch(tex->type)
    {
    case GLT_DETAIL:
        if(context)
        {
            // Round off the contrast to nearest 0.1.
            contrast = (int) ((*((float*) context) + .05f) * 10) / 10.f;
            context = (void*) &contrast;
        }

        texInst = pickDetailTextureInst(tex, context);
        break;

    case GLT_SPRITE:
        texInst = pickSpriteTextureInst(tex, context);
        break;

    default:
        texInst = pickGLTextureInst(tex, context);
        break;
    }

    if(texInst && texInst->id)
    {   // Already prepared.
        tmpResult = 0;
    }
    else
    {
        gltexture_typedata_t* glTexType = &glTextureTypeData[tex->type];
        gltexture_inst_t localInst;
        boolean noSmartFilter = false, didDefer = false;
        image_t image;

        if(!texInst)
        {   // No existing suitable instance.
            // Use a temporary local instance until we are sure preparation
            // completes successfully.
            memset(&localInst, 0, sizeof(localInst));
            localInst.tex = tex;

            texInst = &localInst;
        }

        initImage(&image);

        // Load in the raw source image.
        if(!(tmpResult = glTexType->loadData(&image, texInst, context)))
        {   // Source image not loaded.
            return NULL;
        }

        if(image.pixelSize == 1)
        {
            if(monochrome && !scaleSharp && (tex->type == GLT_DOOMPATCH || tex->type == GLT_SPRITE))
                GL_DeSaturatePalettedImage(image.pixels, R_GetColorPalette(0), image.width, image.height);

            if(tex->type == GLT_DETAIL)
            {
                float baMul, hiMul, loMul;
                Equalize(image.pixels, image.width, image.height, &baMul, &hiMul, &loMul);
                if(verbose && (baMul != 1 || hiMul != 1 || loMul != 1))
                {
                    Con_Message("GLTexture_Prepare: Equalized detail texture \"%s\" (balance: %g, high amp: %g, low amp: %g).\n",
                                texInst->tex->name, baMul, hiMul, loMul);
                }
            }

            if(scaleSharp)
            {
                int scaleMethod = GL_ChooseSmartFilter(image.width, image.height, 0);
                int numpels = image.width * image.height;
                uint8_t* rgbaPixels, *upscaledPixels;

                if(0 == (rgbaPixels = malloc(numpels * 4)))
                    Con_Error("GLTexture_Prepare: Failed on allocation of %lu bytes for "
                              "RGBA conversion buffer.", (unsigned long) (numpels * 4));

                GL_ConvertBuffer(image.width, image.height, ((image.flags & IMGF_IS_MASKED)? 2 : 1),
                    4, image.pixels, rgbaPixels, 0, false);

                if(monochrome && (tex->type == GLT_DOOMPATCH || tex->type == GLT_SPRITE))
                    Desaturate(rgbaPixels, image.width, image.height, 4);

                upscaledPixels = GL_SmartFilter(scaleMethod, rgbaPixels, image.width, image.height, 0, &image.width, &image.height);
                if(upscaledPixels != rgbaPixels)
                {
                    free(rgbaPixels);
                    rgbaPixels = upscaledPixels;
                }

                EnhanceContrast(rgbaPixels, image.width, image.height);
                //SharpenPixels(rgbaPixels, image.width, image.height);
                //BlackOutlines(rgbaPixels, image.width, image.height);

                // Back to indexed+alpha?
                if(monochrome && (tex->type == GLT_DOOMPATCH || tex->type == GLT_SPRITE))
                {   // No. We'll convert from RGB(+A) to Luminance(+A) and upload as is.
                    // Replace the old buffer.
                    free(image.pixels);
                    image.pixels = rgbaPixels;
                    image.pixelSize = 4;

                    GL_ConvertToLuminance(&image);
                    Amplify(image.pixels, image.width, image.height, image.pixelSize == 2);
                }
                else
                {   // Yes. Quantize down from RGA(+A) to Indexed(+A), replacing the old image.
                    GL_ConvertBuffer(image.width, image.height, 4, ((image.flags & IMGF_IS_MASKED)? 2 : 1),
                        rgbaPixels, image.pixels, 0, false);
                    free(rgbaPixels);
                }

                // Lets not do this again.
                noSmartFilter = true;
            }

            if(fillOutlines /*&& !scaleSharp*/ && (image.flags & IMGF_IS_MASKED) && image.pixelSize == 1)
                ColorOutlines(image.pixels, image.width, image.height);
        }
        else
        {
            if(monochrome && tex->type == GLT_DOOMPATCH && image.pixelSize > 2)
            {
                GL_ConvertToLuminance(&image);
                Amplify(image.pixels, image.width, image.height, image.pixelSize == 2);
            }
        }

        // Too big for us?
        if(image.width  > GL_state.maxTexSize || image.height > GL_state.maxTexSize)
        {
            if(image.pixelSize == 3 || image.pixelSize == 4)
            {
                int newWidth  = MIN_OF(image.width, GL_state.maxTexSize);
                int newHeight = MIN_OF(image.height, GL_state.maxTexSize);
                uint8_t* scaledPixels = GL_ScaleBuffer32(image.pixels, image.width, image.height, image.pixelSize,
                    newWidth, newHeight);
                if(scaledPixels != image.pixels)
                {
                    free(image.pixels);
                    image.pixels = scaledPixels;
                    image.width = newWidth;
                    image.height = newHeight;
                }
            }
            else
            {
                Con_Message("GLTexture_Prepare: Warning, non RGB(A) texture larger than max size (%ix%i bpp%i).\n",
                            image.width, image.height, image.pixelSize);
            }
        }

        // Lightmaps and flare textures should always be monochrome images.
        if((tex->type == GLT_LIGHTMAP || (tex->type == GLT_FLARE && image.pixelSize != 4)) &&
           !(image.flags & IMGF_IS_MASKED))
        {
            // An alpha channel is required. If one is not in the image data, we'll generate it.
            GL_ConvertToAlpha(&image, true);
        }

        {
        int magFilter, minFilter, anisoFilter, wrapS, wrapT, grayMipmap = -1, flags = 0;
        texturecontent_t content;
        dgltexformat_t texFormat;
        boolean alphaChannel;

        // Disable compression?
        if(noCompression || (image.width < 128 && image.height < 128) || tex->type == GLT_FLARE || tex->type == GLT_SHINY)
            flags |= TXCF_NO_COMPRESSION;

        if(!(tex->type == GLT_MASK || tex->type == GLT_SHINY || tex->type == GLT_LIGHTMAP) &&
           (image.pixelSize > 2 || tex->type == GLT_MODELSKIN))
            flags |= TXCF_APPLY_GAMMACORRECTION;

        if(tex->type == GLT_SPRITE /*|| tex->type == GLT_DOOMPATCH*/)
            flags |= TXCF_UPLOAD_ARG_NOSTRETCH;

        if(!monochrome && !(tex->type == GLT_DETAIL || tex->type == GLT_SYSTEM || tex->type == GLT_SHINY || tex->type == GLT_MASK))
            flags |= TXCF_EASY_UPLOAD;

        if(!monochrome)
        {
            if(tex->type == GLT_SPRITE || tex->type == GLT_MODELSKIN || tex->type == GLT_MODELSHINYSKIN)
            {
                if(image.pixelSize > 1)
                    flags |= TXCF_UPLOAD_ARG_RGBDATA;
            }
            else if(image.pixelSize > 2 && !(tex->type == GLT_SHINY || tex->type == GLT_MASK || tex->type == GLT_LIGHTMAP))
                flags |= TXCF_UPLOAD_ARG_RGBDATA;
        }

        if(tex->type == GLT_DETAIL)
        {
            /**
             * Detail textures are faded to gray depending on the contrast
             * factor. The texture is also progressively faded towards gray
             * when each mipmap level is loaded.
             */
            grayMipmap = MINMAX_OF(0, contrast * 255, 255);
            flags |= TXCF_GRAY_MIPMAP | (grayMipmap << TXCF_GRAY_MIPMAP_LEVEL_SHIFT);
        }
        else if(!(tex->type == GLT_SHINY || tex->type == GLT_DOOMPATCH || tex->type == GLT_LIGHTMAP || tex->type == GLT_FLARE || (tex->type == GLT_SPRITE && context && ((material_load_params_t*)context)->pSprite)))
            flags |= TXCF_MIPMAP;

        if(tex->type == GLT_DOOMTEXTURE || tex->type == GLT_DOOMPATCH || tex->type == GLT_SPRITE || tex->type == GLT_FLAT)
            alphaChannel = ((/*tmpResult == 2 &&*/ image.pixelSize == 4) ||
                            (/*tmpResult == 1 &&*/ image.pixelSize == 1 && (image.flags & IMGF_IS_MASKED)));
        else
            alphaChannel = image.pixelSize != 3 && !(tex->type == GLT_MASK || tex->type == GLT_SHINY);

        if(alphaChannel)
            flags |= TXCF_UPLOAD_ARG_ALPHACHANNEL;

        if(noSmartFilter)
            flags |= TXCF_UPLOAD_ARG_NOSMARTFILTER;

        if(monochrome)
        {
            texFormat = ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 : DGL_LUMINANCE );
        }
        else if(tex->type == GLT_FLAT || tex->type == GLT_DOOMTEXTURE || tex->type == GLT_DOOMPATCH || tex->type == GLT_SPRITE)
        {
            texFormat = ( image.pixelSize > 1?
                            ( alphaChannel? DGL_RGBA : DGL_RGB) :
                            ( alphaChannel? DGL_COLOR_INDEX_8_PLUS_A8 : DGL_COLOR_INDEX_8 ) );
        }
        else if(tex->type == GLT_MODELSKIN || tex->type == GLT_MODELSHINYSKIN)
        {
            texFormat = ( alphaChannel? DGL_RGBA : DGL_RGB );
        }
        else
        {
            texFormat = ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                          image.pixelSize == 3 ? DGL_RGB :
                          image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE );
        }

        if(tex->type == GLT_FLAT || tex->type == GLT_DOOMTEXTURE || tex->type == GLT_MASK)
            magFilter = glmode[texMagMode];
        else if(tex->type == GLT_SPRITE)
            magFilter = filterSprites ? GL_LINEAR : GL_NEAREST;
        else
            magFilter = GL_LINEAR;

        if(tex->type == GLT_DETAIL)
            minFilter = GL_LINEAR_MIPMAP_LINEAR;
        else if(tex->type == GLT_DOOMPATCH || (tex->type == GLT_SPRITE && context && ((material_load_params_t*)context)->pSprite))
            minFilter = GL_NEAREST;
        else if(tex->type == GLT_LIGHTMAP || tex->type == GLT_FLARE || tex->type == GLT_SHINY)
            minFilter = GL_LINEAR;
        else
            minFilter = glmode[mipmapping];

        if(tex->type == GLT_DOOMPATCH || tex->type == GLT_FLARE || (tex->type == GLT_SPRITE && context && ((material_load_params_t*)context)->pSprite))
            anisoFilter = 0 /*no anisotropic filtering*/;
        else
            anisoFilter = texAniso; /// \fixme is "best" truely a suitable default for ALL texture types?

        if(tex->type == GLT_DOOMPATCH || tex->type == GLT_SPRITE || tex->type == GLT_LIGHTMAP || tex->type == GLT_FLARE)
            wrapS = wrapT = GL_CLAMP_TO_EDGE;
        else
            wrapS = wrapT = GL_REPEAT;

        GL_InitTextureContent(&content);
        content.buffer = image.pixels;
        content.width = image.width;
        content.height = image.height;
        content.flags = flags;
        content.format = texFormat;
        content.minFilter = minFilter;
        content.magFilter = magFilter;
        content.anisoFilter = anisoFilter;
        content.wrap[0] = wrapS;
        content.wrap[1] = wrapT;
        content.grayMipmap = grayMipmap;

        texInst->id = GL_NewTexture(&content, &didDefer);
#ifdef _DEBUG
if(!didDefer)
    Con_Message("GLTexture_Prepare: Uploaded \"%s\" (%i) while not busy! "
                "Should be precached in busy mode?\n", texInst->tex->name, texInst->id);
#endif

        if(tex->type == GLT_DETAIL)
        {
            texInst->data.detail.contrast = contrast;
        }
        else
        {
            material_load_params_t* params = (material_load_params_t*) context;
            byte texFlags = 0, border = 0;

            if(params)
            {
                texFlags = params->tex.flags;
                border = params->tex.border;
            }

            texInst->flags = texFlags;
            texInst->isMasked = (image.flags & IMGF_IS_MASKED) != 0;
            texInst->border = border;
        }

        if(tex->type == GLT_SPRITE)
        {
            material_load_params_t* params = (material_load_params_t*) context;
            float* tc = texInst->data.sprite.texCoord;
            int tmap = 0, tclass = 0;
            boolean pSprite = false;

            if(params)
            {
                tmap = params->tmap;
                tclass = params->tclass;
                pSprite = params->pSprite;
            }

            texInst->data.sprite.tmap = tmap;
            texInst->data.sprite.tclass = tclass;
            texInst->data.sprite.pSprite = pSprite;

            if(!pSprite)
            {   // Calculate light source properties.
                GL_CalcLuminance(image.pixels, image.width, image.height, image.pixelSize, 0,
                                 &texInst->data.sprite.flareX, &texInst->data.sprite.flareY,
                                 &texInst->data.sprite.autoLightColor, &texInst->data.sprite.lumSize);
            }

            /**
             * Calculates texture coordinates based on the image dimensions. The
             * coordinates are calculated as width/CeilPow2(width), or 1 if larger
             * than the maximum texture size.
             */
            if(GL_state.textureNonPow2 && (pSprite || !(flags & TXCF_UPLOAD_ARG_NOSTRETCH)) &&
               !(image.width < MINTEXWIDTH || image.height < MINTEXHEIGHT))
            {
                tc[0] = tc[1] = 1;
            }
            else
            {
                int pw = M_CeilPow2(image.width), ph = M_CeilPow2(image.height);
                tc[0] = (image.width-1)  / (float) pw;
                tc[1] = (image.height-1) / (float) ph;
            }
        }

        if(tex->type == GLT_FLAT || tex->type == GLT_DOOMTEXTURE)
        {
            // Average color for glow planes and top line color.
            if(image.pixelSize > 1)
            {
                averageColorRGB(texInst->data.texture.color, image.pixels, image.width, image.height);
                lineAverageColorRGB(texInst->data.texture.topColor, image.pixels, image.width, image.height, 0);
            }
            else
            {
                averageColorIdx(texInst->data.texture.color, image.pixels, image.width, image.height, 0, false);
                lineAverageColorIdx(texInst->data.texture.topColor, image.pixels, image.width, image.height, 0, 0, false);
            }

            memcpy(texInst->data.texture.colorAmplified, texInst->data.texture.color, sizeof(texInst->data.texture.colorAmplified));
            amplify(texInst->data.texture.colorAmplified);
        }
        }

        GL_DestroyImage(&image);

        if(tmpResult && texInst == &localInst)
        {   // We have a new instance.
            gltexture_inst_node_t* node;
            int flags = 0;

            if(tex->type != GLT_DETAIL)
            {
                material_load_params_t* params = (material_load_params_t*) context;
                if(params)
                    flags = params->flags;
            }

            // Add it to the list of intances for this gltexture.
            node = Z_Malloc(sizeof(*node), PU_APPSTATIC, 0);
            memcpy(&node->inst, &localInst, sizeof(gltexture_inst_t));
            node->flags = flags;
            node->next = (gltexture_inst_node_t*) tex->instances;
            tex->instances = (void*) node;

            texInst = &node->inst;
        }
    }

    if(result)
        *result = tmpResult;

    return texInst;
}

boolean GLTexture_IsFromIWAD(const gltexture_t* tex)
{
    switch(tex->type)
    {
    case GLT_FLAT:
        return !R_GetFlatForIdx(tex->ofTypeID)->isCustom;

    case GLT_DOOMTEXTURE:
        return (R_GetDoomTextureDef(tex->ofTypeID)->flags & TXDF_IWAD) != 0;

    case GLT_SPRITE:
        return !R_SpriteTextureForIndex(tex->ofTypeID)->isCustom;

    case GLT_DOOMPATCH:
        return !R_FindPatchTex(tex->ofTypeID)->isCustom;

    case GLT_DETAIL:
    case GLT_SHINY:
    case GLT_MASK:
    case GLT_SYSTEM:
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN:
    case GLT_LIGHTMAP:
    case GLT_FLARE:
        return false; // Its definitely not.

    default:
        Con_Error("updateGLTexture: Internal Error, invalid type %i.",
                  (int) tex->type);
    }

    return false; // Unreachable.
}

const char* GLTexture_Name(const gltexture_t* tex)
{
    assert(tex);
    return tex->name;
}

float GLTexture_GetWidth(const gltexture_t* tex)
{
    if(!tex)
        return 0;

    switch(tex->type)
    {
    case GLT_FLAT:
        return 64; /// \fixme not all flats are 64x64

    case GLT_DOOMTEXTURE:
        return R_GetDoomTextureDef(tex->ofTypeID)->width;

    case GLT_SPRITE:
        return R_SpriteTextureForIndex(tex->ofTypeID)->width;

    case GLT_DOOMPATCH:
        return R_FindPatchTex(tex->ofTypeID)->width;

    case GLT_DETAIL:
        return 128;

    case GLT_SHINY:
        return 128; // Could be used for something useful.

    case GLT_MASK:
        return maskTextures[tex->ofTypeID]->width;

    case GLT_SYSTEM: /// \fixme Do not assume!
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN:
    case GLT_LIGHTMAP:
    case GLT_FLARE:
        return 64;

    default:
        Con_Error("GLTexture_GetWidth: Internal error, "
                  "invalid material type %i.", (int) tex->type);
    }

    return 0; // Unreachable.
}

float GLTexture_GetHeight(const gltexture_t* tex)
{
    if(!tex)
        return 0;

    switch(tex->type)
    {
    case GLT_FLAT:
        return 64; /// \fixme not all flats are 64x64

    case GLT_DOOMTEXTURE:
        return R_GetDoomTextureDef(tex->ofTypeID)->height;

    case GLT_SPRITE:
        return R_SpriteTextureForIndex(tex->ofTypeID)->height;

    case GLT_DOOMPATCH:
        return R_FindPatchTex(tex->ofTypeID)->height;

    case GLT_DETAIL:
        return 128;

    case GLT_SHINY:
        return 128; // Could be used for something useful.

    case GLT_MASK:
        return maskTextures[tex->ofTypeID]->height;

    case GLT_SYSTEM: /// \fixme Do not assume!
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN:
    case GLT_LIGHTMAP:
    case GLT_FLARE:
        return 64;

    default:
        Con_Error("GLTexture_GetHeight: Internal error, "
                  "invalid material type %i.", (int) tex->type);
    }

    return 0; // Unreachable.
}

/**
 * Deletes all GL texture instances for the specified gltexture.
 */
void GLTexture_ReleaseTextures(gltexture_t* tex)
{
    if(tex)
    {
        gltexture_inst_node_t*     node;

        // Delete all instances.
        node = (gltexture_inst_node_t*) tex->instances;
        while(node)
        {
            gltexture_inst_t*  inst = &node->inst;

            if(inst->id) // Is the texture loaded?
            {
                glDeleteTextures(1, (const GLuint*) &inst->id);
                inst->id = 0;
            }
            node = node->next;
        }
    }
}

/**
 * Sets the minification mode of the specified gltexture.
 *
 * @param tex           The gltexture to be updated.
 * @param minMode       The GL minification mode to set.
 */
void GLTexture_SetMinMode(gltexture_t* tex, int minMode)
{
    if(tex)
    {
        gltexture_inst_node_t*     node;

        node = (gltexture_inst_node_t*) tex->instances;
        while(node)
        {
            gltexture_inst_t*  inst = &node->inst;

            if(inst->id) // Is the texture loaded?
            {
                glBindTexture(GL_TEXTURE_2D, inst->id);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minMode);
            }
            node = node->next;
        }
    }
}

D_CMD(LowRes)
{
    GL_LowRes();
    return true;
}

D_CMD(ResetTextures)
{
    if(argc == 2 && !stricmp(argv[1], "raw"))
    {
        // Reset just raw images.
        GL_DeleteRawImages();
    }
    else
    {
        // Reset everything.
        GL_TexReset();
    }
    return true;
}

D_CMD(MipMap)
{
    GL_UpdateTexParams(strtol(argv[1], NULL, 0));
    return true;
}
