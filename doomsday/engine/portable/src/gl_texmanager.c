/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * gl_texman.c: Texture management routines.
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

#include "def_main.h"
#include "ui_main.h"
#include "p_particle.h"

// MACROS ------------------------------------------------------------------

#define GLTEXTURE_NAME_HASH_SIZE (512)

// TYPES -------------------------------------------------------------------

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
#ifdef _DEBUG
D_CMD(TranslateFont);
#endif

void GL_DoResetDetailTextures(cvar_t* unused);

/// gltexture_t abstract interface:
gltexture_inst_t* GLTexture_Prepare(gltexture_t* tex,
                                    void* context, byte* result);
void            GLTexture_ReleaseTextures(gltexture_t* tex);

void            GLTexture_SetMinMode(gltexture_t* tex, int minMode);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void calcGammaTable(void);

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
#if _DEBUG
    C_CMD_FLAGS("translatefont", "ss", TranslateFont, CMDF_NO_DEDICATED);
#endif
}

/**
 * Called before real texture management is up and running, during engine
 * early init.
 */
void GL_EarlyInitTextureManager(void)
{
    int                 i;

    // Initialize the smart texture filtering routines.
    GL_InitSmartFilter();

    calcGammaTable();

    numGLTextures = 0;
    glTextures = NULL;
    for(i = 0; i < NUM_GLTEXTURE_TYPES; ++i)
        memset(glTextureTypeData[i].hashTable, 0, sizeof(glTextureTypeData[i].hashTable));
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

    // Disable the use of 'high resolution' textures?
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

/**
 * Called once during engine shutdown.
 */
void GL_ShutdownTextureManager(void)
{
    if(!texInited)
        return; // Already been here?

    if(glTextures)
    {
        uint                i;

        for(i = 0; i < numGLTextures; ++i)
        {
            gltexture_t*         mTex = glTextures[i];
            gltexture_inst_node_t*  node, *next;

            node = (gltexture_inst_node_t*) mTex->instances;
            while(node)
            {
                next = node->next;
                Z_Free(node);
                node = next;
            }
            Z_Free(mTex);
        }

        Z_Free(glTextures);
        glTextures = NULL;
        numGLTextures = 0;
    }

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
    struct ddtexdef_s {
        char            name[9];
        int             id;
        byte            flags; // MATF_* flags
    } static const ddtexdefs[NUM_DD_TEXTURES] =
    {
        {"DDT_UNKN", DDT_UNKNOWN},
        {"DDT_MISS", DDT_MISSING},
        {"DDT_BBOX", DDT_BBOX},
        {"DDT_GRAY", DDT_GRAY}
    };
    int                 i;

    if(!texInited)
        return;

    i = 0;
    for(i = 0; i < NUM_DD_TEXTURES; ++i)
    {
        material_t*         mat;
        const gltexture_t*  tex;

        tex = GL_CreateGLTexture(ddtexdefs[i].name, ddtexdefs[i].id, GLT_SYSTEM);

        mat = Materials_New(ddtexdefs[i].name, 64, 64, 0, tex->id,
                               MN_SYSTEM, NULL);
    }

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

    Rend_ParticleInitTextures(); // Load particle textures.
}

/**
 * System textures are loaded at startup and remain in memory all the time.
 * After clearing they must be manually reloaded.
 */
void GL_ClearSystemTextures(void)
{
    int                 i;

    if(!texInited)
        return;

    for(i = 0; i < NUM_LIGHTING_TEXTURES; ++i)
        glDeleteTextures(1, (const GLuint*) &lightingTextures[i].tex);
    memset(lightingTextures, 0, sizeof(lightingTextures));

    for(i = 0; i < NUM_SYSFLARE_TEXTURES; ++i)
        glDeleteTextures(1, (const GLuint*) &sysFlareTextures[i].tex);
    memset(sysFlareTextures, 0, sizeof(sysFlareTextures));

    Materials_DeleteTextures(MN_SYSTEM);
    UI_ClearTextures();

    // Delete the particle textures.
    Rend_ParticleShutdownTextures();
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

    // gltexture-wrapped GL textures; textures, flats, sprites, system...
    GL_DeleteAllTexturesForGLTextures(GLT_ANY);

    GL_DeleteRawImages();
}

void GL_ClearTextureMemory(void)
{
    if(!texInited)
        return;

    // Delete runtime textures (textures, flats, ...)
    GL_ClearRuntimeTextures();

    // Delete system textures.
    GL_ClearSystemTextures();
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
    byte* data = content->buffer;
    int width = content->width;
    int height = content->height;
    boolean alphaChannel = ((content->flags & TXCF_UPLOAD_ARG_ALPHACHANNEL) != 0);
    boolean generateMipmaps = ((content->flags & TXCF_MIPMAP) != 0);
    boolean RGBData = ((content->flags & TXCF_UPLOAD_ARG_RGBDATA) != 0);
    boolean noStretch = ((content->flags & TXCF_UPLOAD_ARG_NOSTRETCH) != 0);
    boolean noSmartFilter = ((content->flags & TXCF_UPLOAD_ARG_NOSMARTFILTER) != 0);
    boolean applyTexGamma = ((content->flags & TXCF_APPLY_GAMMACORRECTION) != 0);
    int i, levelWidth, levelHeight; // width and height at the current level
    int comps;
    byte* buffer, *rgbaOriginal, *idxBuffer;
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
        rgbaOriginal = M_Malloc(width * height * comps);
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

    // If smart filtering is enabled, all textures are magnified 2x.
    if(useSmartFilter && !noSmartFilter /* && comps == 3 */ )
    {
        byte* filtered = M_Malloc(4 * width * height * 4);

        if(comps == 3)
        {
            // Must convert to RGBA.
            byte* temp = M_Malloc(4 * width * height);

            GL_ConvertBuffer(width, height, 3, 4, rgbaOriginal, temp, 0, !load8bit);

            if(freeOriginal)
                M_Free(rgbaOriginal);

            rgbaOriginal = temp;
            freeOriginal = true;
            comps = 4;
            alphaChannel = true;
        }

        GL_SmartFilter(GL_PickSmartScaleMethod(width, height), rgbaOriginal, filtered, width, height);
        width *= 2;
        height *= 2;
        noStretch = GL_OptimalSize(width, height, &levelWidth, &levelHeight, noStretch, generateMipmaps);

        // The filtered copy is now the 'original' image data.
        if(freeOriginal)
            M_Free(rgbaOriginal);
        rgbaOriginal = filtered;
        freeOriginal = true;
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
        freeBuffer = true;
        buffer = M_Malloc(levelWidth * levelHeight * comps);
        if(noStretch)
        {
            // Copy the image into a buffer with power-of-two dimensions.
            memset(buffer, 0, levelWidth * levelHeight * comps);
            for(i = 0; i < height; ++i) // Copy line by line.
                memcpy(buffer + levelWidth * comps * i, rgbaOriginal + width * comps * i, comps * width);
        }
        else
        {
            // Stretch to fit into power-of-two.
            if(width != levelWidth || height != levelHeight)
            {
                GL_ScaleBuffer32(rgbaOriginal, width, height, buffer, levelWidth, levelHeight, comps);
            }
        }
    }

    // The RGB(A) copy of the source image is no longer needed.
    if(freeOriginal)
    {
        M_Free(rgbaOriginal);
        rgbaOriginal = NULL;
    }

    // Bind the texture so we can upload content.
    glBindTexture(GL_TEXTURE_2D, content->name);

    if(load8bit)
    {
        // We are unable to generate mipmaps for paletted textures.
        DGLuint pal = R_GetColorPalette(content->palette);
        int canGenMips = 0;

        // Prepare the palette indices buffer, to be handed over to DGL.
        idxBuffer = M_Malloc(levelWidth * levelHeight * (alphaChannel ? 2 : 1));

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

        M_Free(idxBuffer);
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
        M_Free(buffer);

    return content->name;
}

/**
 * @return              The outcome:
 *                      0 = not loaded.
 *                      1 = loaded data from a lump resource.
 *                      2 = loaded data from an external resource.
 */
byte GL_LoadDDTexture(image_t* image, const gltexture_inst_t* inst,
                      void* context)
{
    static const char* ddTexNames[NUM_DD_TEXTURES] = {
        "unknown",
        "missing",
        "bbox",
        "gray"
    };
    int                 num;
    byte                result = 0;
    filename_t          fileName;

    if(!image)
        return result; // Wha?

    num = inst->tex->ofTypeID;

    if(num < 0 || num > NUM_DD_TEXTURES)
        Con_Error("GL_LoadDDTexture: Internal error, "
                  "invalid ddtex id %i.", num);

    if(!(R_FindResource2(RT_GRAPHIC, DDRC_GRAPHICS, fileName,
                         ddTexNames[num], NULL, FILENAME_T_MAXLEN) &&
         GL_LoadImage(image, fileName)))
    {
        Con_Error("GL_LoadDDTexture: \"%s\" not found!\n", ddTexNames[num]);
    }

    result = 2; // Always external.

    return result;
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadDetailTexture(image_t* image, const gltexture_inst_t* inst, void* context)
{
    detailtex_t* dTex;

    if(!image)
        return 0; // Wha?

    dTex = detailTextures[inst->tex->ofTypeID];

    if(dTex->external != NULL)
    {
        filename_t fileName;

        if(!(R_FindResource2(RT_GRAPHIC, DDRC_TEXTURE, fileName, dTex->external, NULL, FILENAME_T_MAXLEN) &&
             GL_LoadImage(image, fileName)))
        {
            VERBOSE(Con_Message("GL_LoadDetailTexture: Failed to load: %s\n", dTex->external));
            return 0;
        }

        return 2;
    }
    else
    {
        const byte* data = W_CacheLumpNum(dTex->lump, PU_STATIC);

        // First try loading it as a PCX image.
        if(PCX_MemoryGetSize(data, &image->width, &image->height))
        {   // Nice...
            image->pixels = M_Malloc(image->width * image->height * 3);
            PCX_MemoryLoad(data, W_LumpLength(dTex->lump), image->width, image->height, image->pixels);
        }
        else // It must be a raw image.
        {
            size_t lumpLength = W_LumpLength(dTex->lump);

            /**
             * @fixme we should not error out here if the lump is not
             * of the required format! Perform this check much earlier,
             * when the definitions are read and mark which are valid.
             */

            // How big is it?
            if(lumpLength != 256 * 256)
            {
                if(lumpLength != 128 * 128)
                {
                    if(lumpLength != 64 * 64)
                    {
                        W_ChangeCacheTag(dTex->lump, PU_CACHE);
                        Con_Error("GL_LoadDetailTexture: Must be 256x256, 128x128 or 64x64.\n");
                    }
                    image->width = image->height = 64;
                }
                else
                {
                    image->width = image->height = 128;
                }
            }
            else
                image->width = image->height = 256;

            image->pixels = M_Malloc(image->width * image->height);
            memcpy(image->pixels, data, image->width * image->height);
            image->pixelSize = 1;
        }

        W_ChangeCacheTag(dTex->lump, PU_CACHE);
        return 1;
    }
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadLightMap(image_t* image, const gltexture_inst_t* inst,
                     void* context)
{
    lightmap_t*         lmap;
    filename_t          fileName;

    if(!image)
        return 0; // Wha?

    lmap = lightMaps[inst->tex->ofTypeID];

    // Search an external resource.
    if(!(R_FindResource2(RT_GRAPHIC, DDRC_LIGHTMAP, fileName, lmap->external,
                         "-ck", FILENAME_T_MAXLEN) &&
         GL_LoadImage(image, fileName)))
    {
        VERBOSE(
        Con_Message("GL_LoadLightMap: Failed to load: %s\n",
                    lmap->external));
        return 0;
    }

    return 2;
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadFlareTexture(image_t* image, const gltexture_inst_t* inst,
                         void* context)
{
    flaretex_t*         fTex;
    filename_t          fileName;

    if(!image)
        return 0; // Wha?

    fTex = flareTextures[inst->tex->ofTypeID];

    // Search an external resource.
    if(!(R_FindResource2(RT_GRAPHIC, DDRC_FLAREMAP, fileName, fTex->external,
                         "-ck", FILENAME_T_MAXLEN) &&
         GL_LoadImage(image, fileName)))
    {
        VERBOSE(
        Con_Message("GL_LoadFlareTexture: Failed to load: %s\n",
                    fTex->external));
        return 0;
    }

    return 2;
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadShinyTexture(image_t* image, const gltexture_inst_t* inst,
                         void* context)
{
    filename_t fileName;
    shinytex_t* sTex;

    if(!image)
        return 0; // Wha?

    sTex = shinyTextures[inst->tex->ofTypeID];

    if(!(R_FindResource2(RT_GRAPHIC, DDRC_LIGHTMAP, fileName, sTex->external, NULL, FILENAME_T_MAXLEN) &&
         GL_LoadImage(image, fileName)))
    {
        VERBOSE(Con_Printf("GL_LoadShinyTexture: %s not found!\n", sTex->external));
        return 0;
    }

    return 2; // Always external.
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadMaskTexture(image_t* image, const gltexture_inst_t* inst,
                        void* context)
{
    filename_t          fileName;
    masktex_t*          mTex;

    if(!image)
        return 0; // Wha?

    mTex = maskTextures[inst->tex->ofTypeID];

    if(!(R_FindResource2(RT_GRAPHIC, DDRC_LIGHTMAP, fileName, mTex->external,
                         NULL, FILENAME_T_MAXLEN) &&
         GL_LoadImage(image, fileName)))
    {
        VERBOSE(Con_Printf("GL_LoadMaskTexture: %s not found!\n",
                           mTex->external));
        return 0;
    }

    return 2; // Always external.
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadModelSkin(image_t* image, const gltexture_inst_t* inst,
                      void* context)
{
    const skinname_t*   sn;
    filename_t          fileName;

    if(!image)
        return 0; // Wha?

    sn = &skinNames[inst->tex->ofTypeID];

    if(R_FindResource2(RT_GRAPHIC, DDRC_MODEL, fileName, sn->path, NULL,
                       FILENAME_T_MAXLEN))
        if(GL_LoadImage(image, fileName))
        {
            return 2; // Always external.
        }

    VERBOSE(Con_Printf("GL_LoadModelSkin: %s not found!\n", sn->path));

    return 0;
}

/**
 * @return              The outcome:
 *                      0 = none loaded.
 *                      1 = a lump resource.
 *                      2 = an external resource.
 */
byte GL_LoadModelShinySkin(image_t* img, const gltexture_inst_t* inst,
                           void* context)
{
    const skinname_t*   sn;
    filename_t          resource, fileName;
    char*               ptr;

    if(!img)
        return 0; // Wha?

    sn = &skinNames[inst->tex->ofTypeID];

    /**
     * First sees if there is a color-keyed version. If there is load it.
     * Otherwise the 'regular' version is loaded.
     */
    strncpy(resource, sn->path, FILENAME_T_MAXLEN);

    // Append the "-ck" and try to load.
    if((ptr = strrchr(resource, '.')))
    {
        memmove(ptr + 3, ptr, strlen(ptr) + 1);
        ptr[0] = '-';
        ptr[1] = 'c';
        ptr[2] = 'k';

        if(R_FindResource2(RT_GRAPHIC, DDRC_MODEL, fileName, resource, NULL,
                           FILENAME_T_MAXLEN))
            if(GL_LoadImage(img, fileName))
                return 2;
    }

    if(R_FindResource2(RT_GRAPHIC, DDRC_MODEL, fileName, sn->path, NULL,
                       FILENAME_T_MAXLEN))
        if(GL_LoadImage(img, fileName))
            return 2;

    VERBOSE(Con_Printf("GL_LoadModelShinySkin: %s not found!\n",
                       sn->path));
    return 0;
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
        { "dLight", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE },
        { "wallglow", GL_REPEAT, GL_CLAMP_TO_EDGE },
        { "radioCO", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE },
        { "radioCC", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE },
        { "radioOO", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE },
        { "radioOE", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE }
    };

    if(which < 0 || which >= NUM_LIGHTING_TEXTURES)
        return 0;

    if(!lightingTextures[which].tex)
    {
        lightingTextures[which].tex =
            GL_PrepareExtTexture(DDRC_GRAPHICS, lstexes[which].name,
                                 LGM_WHITE_ALPHA, false, GL_LINEAR,
                                 GL_LINEAR, -1 /*best anisotropy*/,
                                 lstexes[which].wrapS, lstexes[which].wrapT,
                                 TXCF_NO_COMPRESSION);
    }

    return lightingTextures[which].tex;
}

DGLuint GL_PrepareSysFlareTexture(flaretexid_t flare)
{
    if(flare >= NUM_SYSFLARE_TEXTURES)
        return 0;

    if(!sysFlareTextures[flare].tex)
    {
        // We don't want to compress the flares (banding would be noticeable).
        sysFlareTextures[flare].tex =
            GL_PrepareExtTexture(DDRC_GRAPHICS,
                             flare == 0 ? "dlight" :
                             flare == 1 ? "flare" :
                             flare == 2 ? "brflare" : "bigflare",
                             LGM_WHITE_ALPHA, false, GL_NEAREST,
                             GL_LINEAR, 0 /*no anisotropy*/,
                             GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                             TXCF_NO_COMPRESSION);

        if(sysFlareTextures[flare].tex == 0)
        {
            Con_Error("GL_PrepareSysFlareTexture: flare texture %i not found!\n",
                      flare);
        }
    }

    return sysFlareTextures[flare].tex;
}

/**
 * @return              The outcome:
 *                      0 = not prepared
 *                      1 = found and prepared a lump resource.
 *                      2 = found and prepared an external resource.
 */
byte GL_LoadExtTexture(image_t* image, ddresourceclass_t resClass,
                       const char* name, gfxmode_t mode)
{
    filename_t          fileName;

    if(R_FindResource2(RT_GRAPHIC, resClass, fileName, name, NULL,
                       FILENAME_T_MAXLEN) &&
       GL_LoadImage(image, fileName))
    {
        // Too big for us?
        if(image->width  > GL_state.maxTexSize ||
           image->height > GL_state.maxTexSize)
        {
            int     newWidth = MIN_OF(image->width, GL_state.maxTexSize);
            int     newHeight = MIN_OF(image->height, GL_state.maxTexSize);
            byte   *tmp = M_Malloc(newWidth * newHeight * image->pixelSize);

            GL_ScaleBuffer32(image->pixels, image->width, image->height,
                             tmp, newWidth, newHeight, image->pixelSize);
            M_Free(image->pixels);
            image->pixels = tmp;
            image->width = newWidth;
            image->height = newHeight;
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

        return 2; // Always external.
    }

    return 0;
}

/**
 * @return              The outcome:
 *                      0 = not prepared
 *                      1 = found and prepared a lump resource.
 *                      2 = found and prepared an external resource.
 */
byte GL_LoadFlat(image_t* img, const gltexture_inst_t* inst,
                 void* context)
{
    const flat_t* flat;
    size_t lumpLength;

    if(!img)
        return 0; // Wha?

    flat = flats[inst->tex->ofTypeID];

    // Try to load a high resolution version of this flat?
    if(!noHighResTex && (loadExtAlways || highResWithPWAD || GLTexture_IsFromIWAD(inst->tex)))
    {
        const char* lumpName = W_LumpName(flat->lump);
        filename_t file;
        boolean found;

        // First try the Flats category.
        if(!(found = R_FindResource2(RT_GRAPHIC, DDRC_FLAT, file, lumpName, NULL, FILENAME_T_MAXLEN)))
        {   // Try the old-fashioned "Flat-NAME" in the Textures category.
            filename_t resource;

            dd_snprintf(resource, FILENAME_T_MAXLEN, "flat-%s", lumpName);

            found = R_FindResource2(RT_GRAPHIC, DDRC_TEXTURE, file, resource, NULL, FILENAME_T_MAXLEN);
        }

        if(found && GL_LoadImage(img, file))
            return 2;
    }

    if((lumpLength = W_LumpLength(flat->lump)) < 4096)
        return 0; // Too small.

    // Read in the flat.
    img->pixels = M_Malloc(lumpLength);
    W_ReadLump(flat->lump, img->pixels);

    img->width = flat->width;
    img->height = flat->height;
    img->isMasked = false;
    img->pixelSize = 1;

    return 1;
}

/**
 * Loads PCX, TGA and PNG images. The returned buffer must be freed
 * with M_Free. Color keying is done if "-ck." is found in the filename.
 * The allocated memory buffer always has enough space for 4-component
 * colors.
 */
static byte* loadImage(image_t* img, const char* imagefn)
{
    DFILE* file;
    char* p;
    int format;

    // We know how to load PCX, TGA and PNG.
    p = M_FindFileExtension(img->fileName);
    if(p)
    {
        if(!strcmp(p, "pcx"))
        {
            if(!PCX_GetSize(img->fileName, &img->width, &img->height))
                return NULL;

            img->pixels = M_Malloc(4 * img->width * img->height);
            PCX_Load(img->fileName, img->width, img->height, img->pixels);

            img->pixelSize = 3; // PCXs can't be masked.
            img->originalBits = 8;
        }
        else if(!strcmp(p, "tga"))
        {
            if(!TGA_GetSize(img->fileName, &img->width, &img->height))
                return NULL;

            if(!(file = F_Open(img->fileName, "rb")))
                return NULL;

            // Allocate a big enough buffer and read in the image.
            img->pixels = M_Malloc(4 * img->width * img->height);
            format = TGA_Load32_rgba8888(file, img->width, img->height, img->pixels);
            if(format == TGA_TARGA24)
            {
                img->pixelSize = 3;
                img->originalBits = 24;
            }
            else
            {
                img->pixelSize = 4;
                img->originalBits = 32;
            }
            F_Close(file);
        }
        else if(!strcmp(p, "png"))
        {
            img->pixels = PNG_Load(img->fileName, &img->width, &img->height, &img->pixelSize);
            img->originalBits = 8 * img->pixelSize;

            if(img->pixels == NULL)
                return NULL;
        }
    }

    VERBOSE(Con_Message("LoadImage: %s (%ix%i)\n", M_PrettyPath(img->fileName), img->width, img->height));

    // How about some color-keying?
    if(GL_IsColorKeyed(img->fileName))
    {
        byte* out;

        out = GL_ApplyColorKeying(img->pixels, img->pixelSize, img->width, img->height);
        if(out)
        {
            // Had to allocate a larger buffer, free the old and attach the new.
            M_Free(img->pixels);
            img->pixels = out;
        }

        // Color keying is done; now we have 4 bytes per pixel.
        img->pixelSize = 4;
        img->originalBits = 32; // Effectively...
    }

    // Any alpha pixels?
    img->isMasked = ImageHasAlpha(img);

    return img->pixels;
}

byte* GL_LoadImage(image_t* img, const char* imagefn)
{
    if(img)
    {
        // Clear any old values.
        memset(img, 0, sizeof(*img));

        if(imagefn && imagefn[0])
        {
            strncpy(img->fileName, imagefn, FILENAME_T_MAXLEN);

            return loadImage(img, imagefn);
        }
    }

    return NULL;
}

/**
 * Frees all memory associated with the image.
 */
void GL_DestroyImage(image_t* img)
{
    if(img)
    {
        if(img->pixels)
            M_Free(img->pixels);
        img->pixels = NULL;
    }
}

/**
 * Set mode to 2 to include an alpha channel. Set to 3 to make the
 * actual pixel colors all white.
 */
DGLuint GL_PrepareExtTexture(ddresourceclass_t resClass, const char* name,
                             gfxmode_t mode, int useMipmap, int minFilter,
                             int magFilter, int anisoFilter, int wrapS,
                             int wrapT, int otherFlags)
{
    image_t             image;
    DGLuint             texture = 0;

    if(GL_LoadExtTexture(&image, resClass, name, mode))
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

/**
 * Part of the Doomsday public API.
 */
DGLuint GL_LoadGraphics(ddresourceclass_t resClass, const char *name,
                        gfxmode_t mode, int useMipmap, boolean clamped,
                        int otherFlags)
{
    return GL_PrepareExtTexture(resClass, name, mode, useMipmap,
                            GL_LINEAR, GL_LINEAR, 0 /*no anisotropy*/,
                            clamped? GL_CLAMP_TO_EDGE : GL_REPEAT,
                            clamped? GL_CLAMP_TO_EDGE : GL_REPEAT,
                            otherFlags);
}

/**
 * Renders the given texture into the buffer.
 */
static boolean bufferTexture(const doomtexturedef_t* texDef, byte* buffer,
                             int width, int height, int* hasBigPatch)
{
    int                 i, len;
    boolean             alphaChannel = false;
    lumppatch_t*        patch;

    len = width * height;

    // Clear the buffer.
    memset(buffer, 0, 2 * len);

    // By default zero is put in the big patch height.
    if(hasBigPatch)
        *hasBigPatch = 0;

    // Draw all the patches. Check for alpha pixels after last patch has
    // been drawn.
    for(i = 0; i < texDef->patchCount; ++i)
    {
        const texpatch_t*   patchDef = &texDef->patches[i];

        patch = (lumppatch_t*) W_CacheLumpNum(patchDef->lump, PU_CACHE);

        // Check for big patches?
        if(SHORT(patch->height) > texDef->height && hasBigPatch &&
           *hasBigPatch < SHORT(patch->height))
        {
            *hasBigPatch = SHORT(patch->height);
        }

        // Draw the patch in the buffer.
        alphaChannel =
            DrawRealPatch(buffer, /*palette,*/ width, height, patch,
                          patchDef->offX, patchDef->offY,
                          false, i == texDef->patchCount - 1);
    }

    return alphaChannel;
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

            DrawRealPatch(imgdata, /*palette,*/ width, height,
                          W_CacheLumpNum(patchDef->lump, PU_CACHE),
                          patchDef->offX, patchDef->offY,
                          zeroMask, false);
        }
    }
    else
    {
        lumppatch_t *patch =
            (lumppatch_t*) W_CacheLumpNum(texDef->patches[0].lump, PU_CACHE);
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
        DrawRealPatch(imgdata, /*palette,*/ width, bufHeight, patch, 0, 0,
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
        filename_t fileName;

        if(R_FindResource2(RT_GRAPHIC, DDRC_TEXTURE, fileName, texDef->name, "-ck", FILENAME_T_MAXLEN))
            if(GL_LoadImage(image, fileName))
                return 2; // High resolution texture loaded.
    }

    // None found. Load the lowres version.
    image->width = texDef->width;
    image->height = texDef->height;
    image->pixelSize = 1;

    if(loadAsSky)
    {
        bufferSkyTexture(texDef, &image->pixels, image->width, image->height, zeroMask);
        image->isMasked = zeroMask;
    }
    else
    {
        /**
         * \todo if we are resizing masked textures re match patches then
         * we are needlessly duplicating work.
         */
        image->pixels = M_Malloc(2 * image->width * image->height);
        image->isMasked = bufferTexture(texDef, image->pixels, image->width, image->height, &i);

        // The -bigmtex option allows the engine to resize masked
        // textures whose patches are too big to fit the texture.
        if(allowMaskedTexEnlarge && image->isMasked && i)
        {
            // Adjust the defined height to fit the largest patch.
            texDef->height = image->height = i;
            // Get a new buffer.
            M_Free(image->pixels);
            image->pixels = M_Malloc(2 * image->width * image->height);
            image->isMasked = bufferTexture(texDef, image->pixels, image->width, image->height, 0);
        }

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
    if(!noHighResTex && (loadExtAlways || highResWithPWAD || W_IsFromIWAD(p->lump)))
    {
        filename_t fileName;
        if(R_FindResource2(RT_GRAPHIC, DDRC_PATCH, fileName, W_LumpName(p->lump), "-ck", FILENAME_T_MAXLEN))
            if(GL_LoadImage(image, fileName))
                return 2; // External replacement patch loaded.
    }

    // Use data from the normal lump.
    if(p->width * p->height)
    {
        const lumppatch_t* patch = W_CacheLumpNum(p->lump, PU_STATIC);
        image->pixelSize = 1;
        image->width = SHORT(p->width);
        image->height = SHORT(p->height);
        if(scaleSharp)
        {
            image->width += 2;
            image->height += 2;
        }
        image->pixels = M_Calloc(2 * image->width * image->height);
        image->isMasked = DrawRealPatch(image->pixels, image->width, image->height, patch, scaleSharp? 1 : 0, scaleSharp? 1 : 0, false, true);
        W_ChangeCacheTag(p->lump, PU_CACHE);
        return 1;
    }

    return 0;
}

/**
 * @return              The outcome:
 *                      0 = not loaded.
 *                      1 = loaded data from a lump resource.
 *                      2 = loaded data from an external resource.
 */
byte GL_LoadSprite(image_t* image, const gltexture_inst_t* inst,
                   void* context)
{
    material_load_params_t* params = (material_load_params_t*) context;
    const spritetex_t*  sprTex;
    int tmap = 0, tclass = 0;
    boolean pSprite = false;
    byte border = 0;

    if(!image)
        return 0; // Wha?

    sprTex = spriteTextures[inst->tex->ofTypeID];

    if(params)
    {
        tmap = params->tmap;
        tclass = params->tclass;
        pSprite = params->pSprite;
        border = params->tex.border;
    }

    // Attempt to load a high resolution version of this sprite?
    if(!noHighResPatches)
    {
        filename_t resource, fileName;
        const char* lumpName = W_LumpName(sprTex->lump);
        boolean found;

        // Compose a resource name.
        if(pSprite)
        {
            dd_snprintf(resource, FILENAME_T_MAXLEN, "%s-hud", lumpName);
        }
        else
        {
            if(tclass || tmap)
            {   // Translated.
                dd_snprintf(resource, FILENAME_T_MAXLEN, "%s-table%i%i", lumpName, tclass, tmap);
            }
            else
            {   // Not translated; use the normal resource.
                strncpy(resource, lumpName, FILENAME_T_MAXLEN);
            }
        }

        found = R_FindResource2(RT_GRAPHIC, DDRC_PATCH, fileName, resource, "-ck", FILENAME_T_MAXLEN);
        if(!found && pSprite)
            found = R_FindResource2(RT_GRAPHIC, DDRC_PATCH, fileName, lumpName, "-ck", FILENAME_T_MAXLEN);

        if(found && GL_LoadImage(image, fileName) != NULL)
            return 2; // Loaded high resolution sprite.
    }

    {
    boolean freePatch = false;
    const lumppatch_t*  patch;
    void* tmp = NULL;

    image->width = sprTex->width+border*2;
    image->height = sprTex->height+border*2;
    image->pixelSize = 1;
    image->pixels = M_Calloc(2 * image->width * image->height);

    if(tclass || tmap)
    {
        int offset = -256 + tclass * ((8-1) * 256) + tmap * 256;

        // We need to translate the patch.
        tmp = M_Malloc(W_LumpLength(sprTex->lump));
        W_ReadLump(sprTex->lump, tmp);
        GL_TranslatePatch((lumppatch_t*) tmp, &translationTables[MAX_OF(0, offset)]);

        patch = (lumppatch_t*) tmp;
        freePatch = true;
    }
    else
    {
        patch = W_CacheLumpNum(sprTex->lump, PU_STATIC);
    }

    image->isMasked = DrawRealPatch(image->pixels, image->width, image->height, patch, border, border, false, true);

    if(freePatch)
        M_Free(tmp);
    else
        W_ChangeCacheTag(sprTex->lump, PU_CACHE);

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
    byte                result = 0;
    filename_t          fileName;

    if(!image)
        return result; // Wha?

    // First try to find an external resource.
    if(R_FindResource2(RT_GRAPHIC, DDRC_PATCH, fileName,
                       W_LumpName(r->lump), NULL, FILENAME_T_MAXLEN) &&
       GL_LoadImage(image, fileName) != NULL)
    {   // High resolution rawtex loaded.
        result = 2;
    }
    else
    {
        // Must load the old-fashioned data lump.
        const byte*         lumpdata;
        size_t              bufSize, lumpLength = W_LumpLength(r->lump);

        // Load the raw image data.
        lumpdata = W_CacheLumpNum(r->lump, PU_STATIC);

        image->width = 320;
        image->height = 200;

        // Try to load it as a PCX image first.
        bufSize = 3 * image->width * image->height;
        image->pixels = M_Malloc(bufSize);

        if(PCX_MemoryLoad(lumpdata, lumpLength, image->width, image->height, image->pixels))
        {
            image->pixelSize = 3;
        }
        else
        {
            // PCX load failed. It must be an old-fashioned raw image.
            memcpy(image->pixels, lumpdata, MIN_OF(bufSize, lumpLength));

            image->height = (int) (lumpLength / image->width);
            image->pixelSize = 1;
        }

        W_ChangeCacheTag(r->lump, PU_CACHE);

        result = 1;
    }

    return result;
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

    if(raw->lump < 0 || raw->lump >= numLumps)
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

DGLuint GL_GetLightMapTexture(const char* name)
{
    lightmap_t*         lmap;

    if(name && name[0])
    {
        if(name[0] == '-')
            return 0;

        if((lmap = R_GetLightMap(name)))
        {
            const gltexture_inst_t* texInst;

            if((texInst = GL_PrepareGLTexture(lmap->id, NULL, NULL)))
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
DGLuint GL_GetFlareTexture(const char* name, int oldIdx)
{
    if(name && name[0])
    {
        flaretex_t*         fTex;

        if(name[0] == '-' || (name[0] == '0' && !name[1]))
            return 0; // Use the automatic selection logic.

        if(name[0] >= '1' && name[0] <= '4' && !name[1])
            return GL_PrepareSysFlareTexture(name[0] - '1');

        if((fTex = R_GetFlareTexture(name)))
        {
            const gltexture_inst_t* texInst;

            if((texInst = GL_PrepareGLTexture(fTex->id, NULL, NULL)))
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

    if(baMul == 1 && hiMul == 1 && loMul == 1)
        return; // Artist has God-like skillz.

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
 *
 * @param unused        Unused, must be signature compatible.
 */
void GL_DoUpdateTexParams(cvar_t *unused)
{
    GL_SetTextureParams(glmode[mipmapping], true, true);
}

static int doTexReset(void* parm)
{
    boolean             usingBusyMode = *((boolean*) parm);

    if(usingBusyMode)
        Con_SetProgress(100);

    /// \todo re-upload ALL textures currently in use.
    GL_LoadSystemTextures();

    if(usingBusyMode)
    {
        Con_SetProgress(200);

        Con_BusyWorkerEnd();
    }

    return 0;
}

void GL_TexReset(void)
{
    boolean             useBusyMode = !Con_IsBusy();

    GL_ClearTextureMemory();
    Con_Printf("All DGL textures deleted.\n");

    if(useBusyMode)
    {
        Con_InitProgress(200);
        Con_Busy(BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR
                 | (verbose? BUSYF_CONSOLE_OUTPUT : 0), "Reseting textures...",
                 doTexReset, &useBusyMode);
    }
    else
    {
        doTexReset(&useBusyMode);
    }
}

/**
 * Called when changing the value of the texture gamma cvar.
 */
void GL_DoUpdateTexGamma(cvar_t *unused)
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
void GL_DoTexReset(cvar_t* unused)
{
    GL_TexReset();
}

void GL_DoResetDetailTextures(cvar_t* unused)
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
if(type < GLT_SYSTEM || !(type < NUM_GLTEXTURE_TYPES))
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

    tex = Z_Malloc(sizeof(*tex), PU_STATIC, 0);
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
                  PU_STATIC);
    // Add the new gltexture;
    glTextures[numGLTextures - 1] = tex;

    return tex;
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

    if(tex->type < GLT_SYSTEM || tex->type >= NUM_GLTEXTURE_TYPES)
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

        memset(&image, 0, sizeof(image));

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
                    Con_Message("GLTexture_Prepare: Equalized detail texture \"%s\" (balance: %g, high amp: %g, low amp: %g).\n", texInst->tex->name, baMul, hiMul, loMul);
                }
            }

            if(scaleSharp)
            {
                int scaleMethod = GL_PickSmartScaleMethod(image.width, image.height);
                int numpels = image.width * image.height;
                byte* rgbaPixels = M_Malloc(numpels * 4 * 2);
                byte* upscaledPixels = M_Malloc(numpels * 4 * 4);

                GL_ConvertBuffer(image.width, image.height, image.isMasked? 2 : 1, 4, image.pixels, rgbaPixels, 0, false);

                if(monochrome && (tex->type == GLT_DOOMPATCH || tex->type == GLT_SPRITE))
                    Desaturate(rgbaPixels, image.width, image.height, 4);

                GL_SmartFilter(scaleMethod, rgbaPixels, upscaledPixels, image.width, image.height);
                image.width *= 2;
                image.height *= 2;

                EnhanceContrast(upscaledPixels, image.width,image.height);
                //SharpenPixels(upscaledPixels, image.width, image.height);
                //BlackOutlines(upscaledPixels, image.width, image.height);

                // Back to indexed+alpha?
                if(monochrome && (tex->type == GLT_DOOMPATCH || tex->type == GLT_SPRITE))
                {   // No. We'll convert from RGB(+A) to Luminance(+A) and upload as is.
                    // Replace the old buffer.
                    M_Free(rgbaPixels);
                    M_Free(image.pixels);
                    image.pixels = upscaledPixels;
                    image.pixelSize = 4;

                    GL_ConvertToLuminance(&image);
                    Amplify(image.pixels, image.width, image.height, image.pixelSize == 2);
                }
                else
                {   // Yes. Quantize down from RGA(+A) to Indexed(+A).
                    GL_ConvertBuffer(image.width, image.height, 4, image.isMasked? 2 : 1, upscaledPixels, rgbaPixels, 0, false);
                    // Replace the old buffer.
                    M_Free(upscaledPixels);
                    M_Free(image.pixels);
                    image.pixels = rgbaPixels;
                }

                // Lets not do this again.
                noSmartFilter = true;
            }

            if(fillOutlines /*&& !scaleSharp*/ && image.isMasked && image.pixelSize == 1)
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
                int newWidth = MIN_OF(image.width, GL_state.maxTexSize);
                int newHeight = MIN_OF(image.height, GL_state.maxTexSize);
                byte* temp = M_Malloc(newWidth * newHeight * image.pixelSize);

                GL_ScaleBuffer32(image.pixels, image.width, image.height, temp, newWidth, newHeight, image.pixelSize);
                M_Free(image.pixels);
                image.pixels = temp;
                image.width = newWidth;
                image.height = newHeight;
            }
            else
            {
                Con_Message("GLTexture_Prepare: Warning, non RGB(A) texture larger than max size (%ix%i bpp%i).\n",
                            image.width, image.height, image.pixelSize);
            }
        }

        // Lightmaps and flare textures should always be monochrome images.
        if((tex->type == GLT_LIGHTMAP || (tex->type == GLT_FLARE && image.pixelSize != 4)) &&
           !image.isMasked)
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
        else if(tmpResult == 2 && !(tex->type == GLT_SHINY || tex->type == GLT_MASK || tex->type == GLT_LIGHTMAP))
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
            alphaChannel = ((tmpResult == 2 && image.pixelSize == 4) || (tmpResult == 1 && image.isMasked))? true : false;
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
            texInst->isMasked = image.isMasked;
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
            node = Z_Malloc(sizeof(*node), PU_STATIC, 0);
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
        return W_IsFromIWAD(flats[tex->ofTypeID]->lump);

    case GLT_DOOMTEXTURE:
        return (R_GetDoomTextureDef(tex->ofTypeID)->flags & TXDF_IWAD)? true : false;

    case GLT_SPRITE:
        return W_IsFromIWAD(spriteTextures[tex->ofTypeID]->lump);

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

float GLTexture_GetWidth(const gltexture_t* tex)
{
    if(!tex)
        return 0;

    switch(tex->type)
    {
    case GLT_FLAT:
        return flats[tex->ofTypeID]->width;

    case GLT_DOOMTEXTURE:
        return R_GetDoomTextureDef(tex->ofTypeID)->width;

    case GLT_SPRITE:
        return spriteTextures[tex->ofTypeID]->width;

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
        return flats[tex->ofTypeID]->height;

    case GLT_DOOMTEXTURE:
        return R_GetDoomTextureDef(tex->ofTypeID)->height;

    case GLT_SPRITE:
        return spriteTextures[tex->ofTypeID]->height;

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

#ifdef _DEBUG
D_CMD(TranslateFont)
{
    char                name[32];
    int                 i, lump;
    size_t              size;
    lumppatch_t        *patch;
    byte                redToWhite[256];

    if(argc < 3)
        return false;

    // Prepare the red-to-white table.
    for(i = 0; i < 256; ++i)
    {
        if(i == 176)
            redToWhite[i] = 168; // Full red -> white.
        else if(i == 45)
            redToWhite[i] = 106;
        else if(i == 46)
            redToWhite[i] = 107;
        else if(i == 47)
            redToWhite[i] = 108;
        else if(i >= 177 && i <= 191)
        {
            redToWhite[i] = 80 + (i - 177) * 2;
        }
        else
            redToWhite[i] = i; // No translation for this.
    }

    // Translate everything.
    for(i = 0; i < 256; ++i)
    {
        sprintf(name, "%s%.3d", argv[1], i);
        if((lump = W_CheckNumForName(name)) != -1)
        {
            Con_Message("%s...\n", name);
            size = W_LumpLength(lump);
            patch = M_Malloc(size);
            memcpy(patch, W_CacheLumpNum(lump, PU_CACHE), size);
            GL_TranslatePatch(patch, redToWhite);
            sprintf(name, "%s%.3d.lmp", argv[2], i);
            M_WriteFile(name, patch, size);
            M_Free(patch);
        }
    }
    return true;
}
#endif

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
