/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
#include "de_dgl.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_misc.h"

#include "def_main.h"
#include "ui_main.h"
#include "p_particle.h"

// MACROS ------------------------------------------------------------------

#define PALLUMPNAME     "PLAYPAL"

// TYPES -------------------------------------------------------------------

// A translated sprite.
typedef struct {
    int             patch;
    DGLuint         tex;
    unsigned char  *table;
    float           flareX, flareY;
    rgbcol_t        color;
} transspr_t;

// Detail texture instance.
typedef struct dtexinst_s {
    struct dtexinst_s *next;
    int             lump;
    float           contrast;
    DGLuint         tex;
    const char     *external;
} dtexinst_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(LowRes);
D_CMD(ResetTextures);
D_CMD(MipMap);
D_CMD(SmoothRaw);

byte *GL_LoadHighResFlat(image_t *img, const char *name);
byte *GL_LoadHighResPatch(image_t *img, char *name);
void GL_DeleteDetailTexture(detailtex_t *dtex);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void LoadPalette(void);
static void GL_SetTexCoords(float *tc, int wid, int hgt);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int ratioLimit;
extern boolean palettedTextureExtAvailable;
extern boolean s3tcAvailable;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The current texture.
DGLuint curTex = 0;

int glMaxTexSize; // Maximum supported texture size.
int ratioLimit = 0; // Zero if none.
boolean fillOutlines = true;
byte loadExtAlways = false; // Always check for extres (cvar)
byte paletted = false; // Use GL_EXT_paletted_texture (cvar)
boolean load8bit = false; // Load textures as 8 bit? (w/paltex)
int monochrome = 0; // desaturate a patch (average colours)
int upscaleAndSharpenPatches = 0;
int useSmartFilter = 0; // Smart filter mode (cvar: 1=hq2x)
int mipmapping = 3, linearRaw = 1, texQuality = TEXQ_BEST;
int filterSprites = true;
int texMagMode = 1; // Linear.
int texAniso = -1; // Use best.

float texGamma = 0;
byte gammaTable[256];

// Convert a 18-bit RGB (666) value to a playpal index.
// \fixme 256kb - Too big?
byte pal18To8[262144];
int palLump;

int glmode[6] = // Indexed by 'mipmapping'.
{
    DGL_NEAREST,
    DGL_LINEAR,
    DGL_NEAREST_MIPMAP_NEAREST,
    DGL_LINEAR_MIPMAP_NEAREST,
    DGL_NEAREST_MIPMAP_LINEAR,
    DGL_LINEAR_MIPMAP_LINEAR
};

// Names of the dynamic light textures.
ddtexture_t lightingTextures[NUM_LIGHTING_TEXTURES];

// Names of the flare textures (halos).
ddtexture_t flareTextures[NUM_FLARE_TEXTURES];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean texInited = false; // Init done.
static boolean allowMaskedTexEnlarge = false;
static boolean noHighResTex = false;
static boolean noHighResPatches = false;
static boolean highResWithPWAD = false;

// Linked list of detail texture instances. A unique texture is generated
// for each (rounded) contrast level.
static dtexinst_t *dtInstances;

// The translated sprites.
static transspr_t **transSprites;
static int numTransSprites;

// CODE --------------------------------------------------------------------

void GL_TexRegister(void)
{
    // Cvars
    C_VAR_INT("rend-tex", &renderTextures, CVF_NO_ARCHIVE, 0, 2);
    C_VAR_FLOAT2("rend-tex-gamma", &texGamma, 0, 0, 1,
                 GL_DoUpdateTexGamma);
    C_VAR_INT2("rend-tex-mipmap", &mipmapping, CVF_PROTECTED, 0, 5,
               GL_DoUpdateTexParams);
    C_VAR_BYTE2("rend-tex-paletted", &paletted, CVF_PROTECTED, 0, 1,
                GL_DoTexReset);
    C_VAR_BYTE2("rend-tex-external-always", &loadExtAlways, 0, 0, 1,
                GL_DoTexReset);
    C_VAR_INT2("rend-tex-quality", &texQuality, 0, 0, 8,
               GL_DoTexReset);
    C_VAR_INT("rend-tex-filter-mag", &texMagMode, 0, 0, 1);
    C_VAR_INT("rend-tex-filter-sprite", &filterSprites, 0, 0, 1);
    C_VAR_INT("rend-tex-filter-raw", &linearRaw, 0, 0, 1);
    C_VAR_INT2("rend-tex-filter-smart", &useSmartFilter, 0, 0, 1,
               GL_DoTexReset);
    C_VAR_INT("rend-tex-filter-anisotropic", &texAniso, 0, -1, 4);
    C_VAR_INT("rend-tex-detail", &r_detail, 0, 0, 1);
    C_VAR_FLOAT("rend-tex-detail-scale", &detailScale,
                CVF_NO_MIN | CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-tex-detail-strength", &detailFactor, 0, 0, 10);
    C_VAR_INT("rend-tex-detail-multitex", &useMultiTexDetails, 0, 0, 1);

    // Ccmds
    C_CMD_FLAGS("lowres", "", LowRes, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("mipmap", "i", MipMap, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("smoothscr", "i", SmoothRaw, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("texreset", "", ResetTextures, CMDF_NO_DEDICATED);
}

/**
 * Called before real texture management is up and running, during engine early init.
 */
void GL_EarlyInitTextureManager(void)
{
    int                 i;

    // Initialize the smart texture filtering routines.
    GL_InitSmartFilter();

    // The palette lump, for color information (really??!!?!?!).
    palLump = W_GetNumForName(PALLUMPNAME);

    // Load the pal18To8 table from the lump PAL18TO8. We need it
    // when resizing textures.
    if((i = W_CheckNumForName("PAL18TO8")) == -1)
        CalculatePal18to8(pal18To8, GL_GetPalette());
    else
        memcpy(pal18To8, W_CacheLumpNum(i, PU_CACHE), sizeof(pal18To8));
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

    numTransSprites = 0;

    // Do we need to generate a pal18To8 table?
    if(ArgCheck("-dump_pal18to8"))
    {
        CalculatePal18to8(pal18To8, GL_GetPalette());

        {
            FILE   *file = fopen("pal18To8.lmp", "wb");

            fwrite(pal18To8, sizeof(pal18To8), 1, file);
            fclose(file);
        }
    }

    GL_InitPalettedTexture();

    // DGL needs the palette information regardless of whether the
    // paletted textures are enabled or not.
    LoadPalette();

    // Detail textures.
    dtInstances = NULL;

    // System textures loaded in GL_LoadSystemTextures.
    memset(flareTextures, 0, sizeof(flareTextures));
    memset(lightingTextures, 0, sizeof(lightingTextures));

    // Initialization done.
    texInited = true;
}

/**
 * Call this if a full cleanup of the textures is required (engine update).
 */
void GL_ShutdownTextureManager(void)
{
    if(!texInited)
        return;

    GL_ClearTextureMemory();

    texInited = false;
}

static void LoadPalette(void)
{
    int                 i, c;
    byte               *playPal;
    byte                palData[256 * 3];
	double	            invGamma;

    palLump = W_GetNumForName(PALLUMPNAME);
    playPal = GL_GetPalette();

    // Clamp to a sane range.
	invGamma = 1.0f - MINMAX_OF(0, texGamma, 1);
	for(i = 0; i < 256; ++i)
		gammaTable[i] = (byte)(255.0f * pow(i / 255.0f, invGamma));

    // Prepare the color table.
    for(i = 0; i < 256; ++i)
    {
        // Adjust the values for the appropriate gamma level.
        for(c = 0; c < 3; ++c)
            palData[i * 3 + c] = gammaTable[playPal[i * 3 + c]];
    }

    DGL_Palette(DGL_RGB, palData);
}

byte *GL_GetPalette(void)
{
    return W_CacheLumpNum(palLump, PU_CACHE);
}

byte *GL_GetPal18to8(void)
{
    return pal18To8;
}

/**
 * Initializes the paletted texture extension.
 * Returns true if successful.
 */
int GL_InitPalettedTexture(void)
{
    // Should the extension be used?
    if(!paletted && !ArgCheck("-paltex"))
        return true;

    DGL_Enable(DGL_PALETTED_TEXTURES);

    // Check if the operation was a success.
    if(DGL_GetInteger(DGL_PALETTED_TEXTURES) == false)
    {
        Con_Message("\nPaletted textures init failed!\n");
        return false;
    }

    // Textures must be uploaded as 8-bit, now.
    load8bit = true;
    return true;
}

/**
 * Lightmaps should be monochrome images.
 */
void GL_LoadLightMap(ded_lightmap_t *map)
{
    image_t             image;
    filename_t          resource;

    if(map->tex)
        return; // Already loaded.

    // Default texture name.
    map->tex = lightingTextures[LST_DYNAMIC].tex;

    if(!strcmp(map->id, "-"))
    {
        // No lightmap, if we don't know where to find the map.
        map->tex = 0;
    }
    else if(map->id[0]) // Not an empty string.
    {
        // Search an external resource.
        if(R_FindResource(RC_LIGHTMAP, map->id, "-ck", resource) &&
           GL_LoadImage(&image, resource, false))
        {
            if(!image.isMasked)
            {
                // An alpha channel is required. If one is not in the
                // image data, we'll generate it.
                GL_ConvertToAlpha(&image, true);
            }

            map->tex = GL_NewTextureWithParams(image.pixelSize ==
                                               2 ? DGL_LUMINANCE_PLUS_A8 : image.pixelSize ==
                                               3 ? DGL_RGB : DGL_RGBA,
                                               image.width, image.height, image.pixels,
                                               TXCF_NO_COMPRESSION);
            GL_DestroyImage(&image);

            // Copy this to all defs with the same lightmap.
            Def_LightMapLoaded(map->id, map->tex);
        }
    }
}

void GL_DeleteLightMap(ded_lightmap_t* map)
{
    if(map->tex != lightingTextures[LST_DYNAMIC].tex)
    {
        DGL_DeleteTextures(1, &map->tex);
    }
    map->tex = 0;
}

/**
 * Flaremaps are normally monochrome images but we'll allow full color.
 */
void GL_LoadFlareMap(ded_flaremap_t* map, int oldidx)
{
    image_t             image;
    filename_t          resource;
    boolean             loaded = false;

    if(map->tex)
        return; // Already loaded.

    // Default texture (automatic).
    map->tex = 0;

    if(!strcmp(map->id, "-"))
    {
        // No flaremap, if we don't know where to find the map.
        map->tex = 0;
        map->disabled = true;
        map->custom = false;
        loaded = true;
    }
    else if(map->id[0]) // Not an empty string.
    {
        // Search an external resource.
        if(R_FindResource(RC_FLAREMAP, map->id, "-ck", resource) &&
           GL_LoadImage(&image, resource, false))
        {
            // A custom flare texture
            map->custom = true;
            map->disabled = false;

            if(!image.isMasked || image.pixelSize != 4)
            {
                // An alpha channel is required. If one is not in the
                // image data, we'll generate it.
                GL_ConvertToAlpha(&image, true);
            }

            map->tex = GL_NewTextureWithParams2(image.pixelSize ==
                                                2 ? DGL_LUMINANCE_PLUS_A8 : image.pixelSize ==
                                                3 ? DGL_RGB : DGL_RGBA,
                                                image.width, image.height, image.pixels,
                                                TXCF_NO_COMPRESSION, DGL_NEAREST, DGL_LINEAR,
                                                0 /*no anisotropy*/,
                                                DGL_CLAMP, DGL_CLAMP);

            // Upload the texture.
            // No mipmapping or resizing is needed, upload directly.
            GL_DestroyImage(&image);

            // Copy this to all defs with the same flaremap.
            Def_FlareMapLoaded(map->id, map->tex, map->disabled, map->custom);
            loaded = true;
        }
    }

    if(!loaded)
    {   // External resource not found.
        // Perhaps a "built-in" flare texture id?
        char               *end;
        int                 id = 0, pass;
        boolean             ok;

        // First pass:
        // Try to convert str "map->tex" to a flare tex constant idx
        // Second pass:
        // Use oldidx (if available) as a flare tex constant idx
        for(pass = 0; pass < 2 && !loaded; ++pass)
        {
            ok = false;
            if(pass == 0 && map->id[0])
            {
                id = strtol(map->id, &end, 0);
                if(!(*end && !isspace(*end)))
                    ok = true;
            }
            else if(pass == 1 && oldidx != -1)
            {
                id = oldidx;
                ok = true;
            }

            if(ok)
            {   // Yes it is!
                // Maybe Automatic OR dynlight?
                if(id == 0 || id == 1)
                {
                    map->tex = (id? GL_PrepareLSTexture(LST_DYNAMIC) : 0);
                    map->custom = false;
                    map->disabled = false;
                    loaded = true;
                }
                else
                {
                    id -= 2;
                    if(id >= 0 && id < NUM_FLARE_TEXTURES)
                    {
                        map->tex = GL_PrepareFlareTexture(id);
                        map->custom = false;
                        map->disabled = false;
                        loaded = true;
                    }
                }
            }
        }
    }
}

void GL_DeleteFlareMap(ded_flaremap_t* map)
{
    if(map->tex != flareTextures[FXT_FLARE].tex)
    {
        DGL_DeleteTextures(1, &map->tex);
    }
    map->tex = 0;
}

/**
 * Loads both the shiny texture and the mask.  Returns true if there is
 * a reflection map available for use.
 */
boolean GL_LoadReflectionMap(ded_reflection_t* loadingRef)
{
    ded_reflection_t* ref;

    if(!loadingRef)
        return false;

    // First try the shiny texture map.
    ref = loadingRef->useShiny;
    if(!ref)
    {   // Not shiny at all.
        return false;
    }

    if(ref->shinyTex == 0)
    {
        // Need to load the shiny texture.
        ref->shinyTex = GL_LoadGraphics2(RC_LIGHTMAP, ref->shinyMap.path,
                                          LGM_NORMAL, false, true, 0);
        if(ref->shinyTex == 0)
        {
            VERBOSE(Con_Printf("GL_LoadReflectionMap: %s not found!\n",
                               ref->shinyMap.path));
        }
    }

    // Also load the mask, if one has been specified.
    if(loadingRef->useMask)
    {
        ref = loadingRef->useMask;

        if(ref->maskTex == 0)
        {
            ref->maskTex = GL_LoadGraphics2(RC_LIGHTMAP,
                                            ref->maskMap.path,
                                            LGM_NORMAL, true, true, 0);
            if(ref->maskTex == 0)
            {
                VERBOSE(Con_Printf("GL_LoadReflectionMap: %s not found!\n",
                                   ref->maskMap.path));
            }
        }
    }

    return true;
}

void GL_DeleteReflectionMap(ded_reflection_t *ref)
{
    if(ref->shinyTex)
    {
        DGL_DeleteTextures(1, &ref->shinyTex);
        ref->shinyTex = 0;
    }

    if(ref->maskTex)
    {
        DGL_DeleteTextures(1, &ref->maskTex);
        ref->maskTex = 0;
    }
}

/**
 * Called from GL_LoadSystemTextures.
 */
void GL_LoadDDTextures(void)
{
    GL_PrepareMaterial(R_GetMaterial(DDT_UNKNOWN, MAT_DDTEX));
    GL_PrepareMaterial(R_GetMaterial(DDT_MISSING, MAT_DDTEX));
    GL_PrepareMaterial(R_GetMaterial(DDT_BBOX, MAT_DDTEX));
    GL_PrepareMaterial(R_GetMaterial(DDT_GRAY, MAT_DDTEX));
}

/**
 * Prepares all the system textures (dlight, ptcgens).
 */
void GL_LoadSystemTextures(boolean loadLightMaps, boolean loadFlares)
{
    struct ddtexdef_s {
        char            name[9];
        int             id;
    } static const ddtexdefs[NUM_DD_TEXTURES] =
    {
        {"DDT_UNKN", DDT_UNKNOWN},
        {"DDT_MISS", DDT_MISSING},
        {"DDT_BBOX", DDT_BBOX},
        {"DDT_GRAY", DDT_GRAY}
    };
    int                 i, k;
    ded_decor_t*        decor;

    if(!texInited)
        return;

    i = 0;
    for(i = 0; i < NUM_DD_TEXTURES; ++i)
    {
        material_t*         mat =
            R_MaterialCreate(ddtexdefs[i].name, ddtexdefs[i].id, MAT_DDTEX);
        mat->width = mat->height = 64;
    }

    GL_LoadDDTextures(); // missing etc
    UI_LoadTextures();

    // Preload lighting system textures.
    GL_PrepareLSTexture(LST_DYNAMIC);
    GL_PrepareLSTexture(LST_GRADIENT);

    // Preload flares
    GL_PrepareFlareTexture(FXT_FLARE);
    if(!haloRealistic)
    {
        GL_PrepareFlareTexture(FXT_BRFLARE);
        GL_PrepareFlareTexture(FXT_BIGFLARE);
    }

    if(loadLightMaps || loadFlares)
    {
        // Load lightmaps and flaremaps.
        for(i = 0; i < defs.count.lights.num; ++i)
        {
            if(loadLightMaps)
            {
                GL_LoadLightMap(&defs.lights[i].up);
                GL_LoadLightMap(&defs.lights[i].down);
                GL_LoadLightMap(&defs.lights[i].sides);
            }

            if(loadFlares)
                GL_LoadFlareMap(&defs.lights[i].flare, -1);
        }
        for(i = 0, decor = defs.decorations; i < defs.count.decorations.num;
            ++i, decor++)
        {
            for(k = 0; k < DED_DECOR_NUM_LIGHTS; ++k)
            {
                if(loadFlares)
                    GL_LoadFlareMap(&decor->lights[k].flare,
                                    decor->lights[k].flareTexture);

                if(!R_IsValidLightDecoration(&decor->lights[k]))
                    break;

                if(loadLightMaps)
                {
                    GL_LoadLightMap(&decor->lights[k].up);
                    GL_LoadLightMap(&decor->lights[k].down);
                    GL_LoadLightMap(&decor->lights[k].sides);
                }
            }

            // Generate RGB lightmaps for decorations.
            //R_GenerateDecorMap(decor);
        }
    }

    // Load particle textures.
    PG_InitTextures();
}

/**
 * System textures are loaded at startup and remain in memory all the time.
 * After clearing they must be manually reloaded.
 */
void GL_ClearSystemTextures(void)
{
    int                 i, k;
    ded_decor_t        *decor;

    if(!texInited)
        return;

    for(i = 0; i < defs.count.lights.num; ++i)
    {
        GL_DeleteLightMap(&defs.lights[i].up);
        GL_DeleteLightMap(&defs.lights[i].down);
        GL_DeleteLightMap(&defs.lights[i].sides);

        GL_DeleteFlareMap(&defs.lights[i].flare);
    }
    for(i = 0, decor = defs.decorations; i < defs.count.decorations.num;
        ++i, decor++)
    {
        for(k = 0; k < DED_DECOR_NUM_LIGHTS; ++k)
        {
            if(!R_IsValidLightDecoration(&decor->lights[k]))
                break;
            GL_DeleteLightMap(&decor->lights[k].up);
            GL_DeleteLightMap(&decor->lights[k].down);
            GL_DeleteLightMap(&decor->lights[k].sides);

            GL_DeleteFlareMap(&decor->lights[k].flare);
        }
    }

    for(i = 0; i < NUM_LIGHTING_TEXTURES; ++i)
        DGL_DeleteTextures(1, &lightingTextures[i].tex);
    memset(lightingTextures, 0, sizeof(lightingTextures));

    for(i = 0; i < NUM_FLARE_TEXTURES; ++i)
        DGL_DeleteTextures(1, &flareTextures[i].tex);
    memset(flareTextures, 0, sizeof(flareTextures));

    R_DeleteMaterialTextures(MAT_DDTEX);
    UI_ClearTextures();

    // Delete the particle textures.
    PG_ShutdownTextures();
}

/**
 * Runtime textures are not loaded until precached or actually needed.
 * They may be cleared, in which case they will be reloaded when needed.
 */
void GL_ClearRuntimeTextures(void)
{
    dtexinst_t         *dtex;
    int                 i;

    if(!texInited)
        return;

    // The rendering lists contain persistent references to texture names.
    // Which, obviously, can't persist any longer...
    RL_DeleteLists();

    // Textures, flats and sprites.
    R_DeleteMaterialTextures(MAT_TEXTURE);
    R_DeleteMaterialTextures(MAT_FLAT);
    R_DeleteMaterialTextures(MAT_SPRITE);

    R_DeleteSkyTextures();

    for(i = 0; i < numSpriteTextures; ++i)
    {
        GL_DeleteHUDSprite(i);
    }

    // The translated sprite textures.
    for(i = 0; i < numTransSprites; ++i)
    {
        DGL_DeleteTextures(1, &transSprites[i]->tex);
        transSprites[i]->tex = 0;
    }
    numTransSprites = 0;

    R_DeleteSkinTextures();

    // Delete detail textures.
    i = 0;
    while(dtInstances)
    {
        dtex = dtInstances->next;
        DGL_DeleteTextures(1, &dtInstances->tex);
        M_Free(dtInstances);
        dtInstances = dtex;
        i++;
    }
    VERBOSE(Con_Message
            ("GL_ClearRuntimeTextures: %i detail texture " "instances.\n", i));
    for(i = 0; i < defs.count.details.num; ++i)
        details[i].glTex = 0;

    // Surface reflection textures and masks.
    for(i = 0; i < defs.count.reflections.num; ++i)
    {
        GL_DeleteReflectionMap(&defs.reflections[i]);
    }

    {
    patchtex_t** patches, **ptr;
    patches = R_CollectPatchTexs(NULL);
    for(ptr = patches; *ptr; ptr++)
    {
        patchtex_t*         p = (*ptr);

        if(p->tex)
        {
            DGL_DeleteTextures(1, &p->tex);
            p->tex = 0;
        }

        if(p->tex2)
        {
            DGL_DeleteTextures(1, &p->tex2);
            p->tex2 = 0;
        }
    }
    Z_Free(patches);
    }

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

    DGL_Bind(texname);
    DGL_TexFilter(DGL_MAG_FILTER, magMode);
    DGL_TexFilter(DGL_ANISO_FILTER, DGL_GetTexAnisoMul(texAniso));
    curTex = texname;
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
    return GL_NewTexture(&content);
}

/**
 * Can be rather time-consuming due to scaling operations and mipmap generation.
 * The texture parameters will NOT be set here.
 * @param data  contains indices to the playpal.
 * @param alphaChannel  If true, 'data' also contains the alpha values (after the indices).
 * @return The name of the texture (same as 'texName').
 */
DGLuint GL_UploadTexture2(texturecontent_t *content)
{
    byte   *data = content->buffer;
    int     width = content->width;
    int     height = content->height;
    boolean alphaChannel = ((content->flags & TXCF_UPLOAD_ARG_ALPHACHANNEL) != 0);
    boolean generateMipmaps = ((content->flags & TXCF_MIPMAP) != 0);
    boolean RGBData = ((content->flags & TXCF_UPLOAD_ARG_RGBDATA) != 0);
    boolean noStretch = ((content->flags & TXCF_UPLOAD_ARG_NOSTRETCH) != 0);
    boolean noSmartFilter = ((content->flags & TXCF_UPLOAD_ARG_NOSMARTFILTER) != 0);
    byte   *palette = GL_GetPalette();
    int     i, levelWidth, levelHeight; // width and height at the current level
    int     comps;
    byte   *buffer, *rgbaOriginal, *idxBuffer;
    boolean freeOriginal;
    boolean freeBuffer;

    // Number of color components in the destination image.
    comps = (alphaChannel ? 4 : 3);

    // Calculate the real dimensions for the texture, as required by
    // the graphics hardware.
    noStretch = GL_OptimalSize(width, height, &levelWidth, &levelHeight,
                               noStretch, generateMipmaps);

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
        freeOriginal = true;
        rgbaOriginal = M_Malloc(width * height * comps);
        GL_ConvertBuffer(width, height, alphaChannel ? 2 : 1, comps, data,
                         rgbaOriginal, palette, !load8bit);
    }

    // If smart filtering is enabled, all textures are magnified 2x.
    if(useSmartFilter && !noSmartFilter /* && comps == 3 */ )
    {
        byte   *filtered = M_Malloc(4 * width * height * 4);

        if(comps == 3)
        {
            // Must convert to RGBA.
            byte   *temp = M_Malloc(4 * width * height);

            GL_ConvertBuffer(width, height, 3, 4, rgbaOriginal, temp,
                             palette, !load8bit);
            if(freeOriginal)
                M_Free(rgbaOriginal);
            rgbaOriginal = temp;
            freeOriginal = true;
            comps = 4;
            alphaChannel = true;
        }

        GL_SmartFilter2x(rgbaOriginal, filtered, width, height, width * 8);
        width *= 2;
        height *= 2;
        noStretch =
            GL_OptimalSize(width, height, &levelWidth, &levelHeight,
                           noStretch, generateMipmaps);

        /*memcpy(filtered, rgbaOriginal, comps * width * height); */

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
                memcpy(buffer + levelWidth * comps * i,
                       rgbaOriginal + width * comps * i, comps * width);
        }
        else
        {
            // Stretch to fit into power-of-two.
            if(width != levelWidth || height != levelHeight)
            {
                GL_ScaleBuffer32(rgbaOriginal, width, height, buffer,
                                 levelWidth, levelHeight, comps);
            }
        }
    }

    // The RGB(A) copy of the source image is no longer needed.
    if(freeOriginal)
        M_Free(rgbaOriginal);
    rgbaOriginal = NULL;

    // Bind the texture so we can upload content.
    DGL_Bind(content->name);

    if(load8bit)
    {
        int     canGenMips;

        DGL_GetIntegerv(DGL_PALETTED_GENMIPS, &canGenMips);

        // Prepare the palette indices buffer, to be handed over to DGL.
        idxBuffer =
            M_Malloc(levelWidth * levelHeight * (alphaChannel ? 2 : 1));

        // Since this is a paletted texture, we may need to manually
        // generate the mipmaps.
        for(i = 0; levelWidth || levelHeight; ++i)
        {
            if(!levelWidth)
                levelWidth = 1;
            if(!levelHeight)
                levelHeight = 1;

            // Convert to palette indices.
            GL_ConvertBuffer(levelWidth, levelHeight, comps,
                             alphaChannel ? 2 : 1, buffer, idxBuffer,
                             palette, false);

            // Upload it.
            if(!DGL_TexImage(alphaChannel ? DGL_COLOR_INDEX_8_PLUS_A8 :
                             DGL_COLOR_INDEX_8, levelWidth, levelHeight,
                             generateMipmaps &&
                             canGenMips ? true : generateMipmaps ? -i :
                             false, idxBuffer))
            {
                Con_Error
                    ("GL_UploadTexture: TexImage failed (%i x %i) as 8-bit, alpha:%i\n",
                     levelWidth, levelHeight, alphaChannel);
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
        if(!DGL_TexImage(alphaChannel ? DGL_RGBA : DGL_RGB, levelWidth, levelHeight,
                         generateMipmaps ? true : false, buffer))
        {
            Con_Error
                ("GL_UploadTexture: TexImage failed (%i x %i), alpha:%i\n",
                 levelWidth, levelHeight, alphaChannel);
        }
    }

    if(freeBuffer)
        M_Free(buffer);

    return content->name;
}

/**
 * The contrast is rounded.
 */
dtexinst_t *GL_GetDetailInstance(int lump, float contrast,
                                 const char *external)
{
    dtexinst_t *i;

    // Round off the contrast to nearest 0.1.
    contrast = (int) ((contrast + .05) * 10) / 10.0;

    for(i = dtInstances; i; i = i->next)
    {
        if(i->lump == lump && i->contrast == contrast &&
           ( (i->external == NULL && external == NULL) ||
             (i->external && external && !stricmp(i->external, external)) ))
        {
            return i;
        }
    }

    // Create a new instance.
    i = M_Malloc(sizeof(dtexinst_t));
    i->next = dtInstances;
    dtInstances = i;
    i->lump = lump;
    i->external = external;
    i->contrast = contrast;
    i->tex = 0;
    return i;
}

/**
 * Detail textures are grayscale images.
 */
DGLuint GL_LoadDetailTexture(int num, float contrast, const char *external)
{
    byte   *lumpData, *image = 0;
    //int     w = 256, h = 256;
    texturecontent_t content;
    dtexinst_t *inst;

    GL_InitTextureContent(&content);
    content.width = 256;
    content.height = 256;

    if(num < 0 && external == NULL)
        return 0;               // No such lump?!

    // Apply the global detail contrast factor.
    contrast *= detailFactor;

    // Have we already got an instance of this texture loaded?
    inst = GL_GetDetailInstance(num, contrast, external);
    if(inst->tex != 0)
    {
        return inst->tex;
    }

    // Detail textures are faded to gray depending on the contrast factor.
    // The texture is also progressively faded towards gray when each
    // mipmap level is loaded.
    content.grayMipmap = MINMAX_OF(0, contrast * 255, 255);

    // Try external first.
    if(external != NULL)
    {
        //inst->tex = GL_LoadGraphics2(RC_TEXTURE, external, LGM_NORMAL,
        //    DGL_GRAY_MIPMAP, true, (content.grayMipmap << TXCF_GRAY_MIPMAP_LEVEL_SHIFT));
        inst->tex = GL_LoadGraphics4(RC_TEXTURE, external, LGM_NORMAL,
                                     DGL_GRAY_MIPMAP, DGL_LINEAR_MIPMAP_LINEAR,
                                     DGL_LINEAR, -1, DGL_REPEAT, DGL_REPEAT,
                                     (content.grayMipmap << TXCF_GRAY_MIPMAP_LEVEL_SHIFT));

        if(inst->tex == 0)
        {
            VERBOSE(Con_Message("GL_LoadDetailTexture: "
                                "Failed to load: %s\n", external));
        }
        return inst->tex; // We're done.
    }
    else
    {
        lumpData = W_CacheLumpNum(num, PU_STATIC);

        // First try loading it as a PCX image.
        if(PCX_MemoryGetSize(lumpData, &content.width, &content.height))
        {
            // Nice...
            image = M_Malloc(content.width * content.height * 3);
            PCX_MemoryLoad(lumpData, W_LumpLength(num), content.width, content.height, image);
            content.format = DGL_RGB;
            content.buffer = image;
        }
        else // It must be a raw image.
        {
            // How big is it?
            if(lumpInfo[num].size != 256 * 256)
            {
                if(lumpInfo[num].size != 128 * 128)
                {
                    if(lumpInfo[num].size != 64 * 64)
                    {
                        Con_Message
                            ("GL_LoadDetailTexture: Must be 256x256, "
                             "128x128 or 64x64.\n");
                        W_ChangeCacheTag(num, PU_CACHE);
                        return 0;
                    }
                    content.width = content.height = 64;
                }
                else
                {
                    content.width = content.height = 128;
                }
            }

            image = M_Malloc(content.width * content.height);
            memcpy(image, W_CacheLumpNum(num, PU_CACHE), content.width * content.height);
            content.format = DGL_LUMINANCE;
            content.buffer = image;
        }

        W_ChangeCacheTag(num, PU_CACHE);
    }

    // Set texture parameters.
    content.minFilter = DGL_LINEAR_MIPMAP_LINEAR;
    content.magFilter = DGL_LINEAR;
    content.anisoFilter = -1; // Best
    content.wrap[0] = content.wrap[1] = DGL_REPEAT;
    content.flags |= TXCF_GRAY_MIPMAP | (content.grayMipmap << TXCF_GRAY_MIPMAP_LEVEL_SHIFT);

    inst->tex = GL_NewTexture(&content);

    // Free allocated memory.
    M_Free(image);

    return inst->tex;
}

/**
 * This is only called when loading a wall texture or a flat
 * (not too time-critical).
 */
DGLuint GL_PrepareDetailTexture(int index, boolean is_wall_texture,
                                ded_detailtexture_t **dtdef)
{
    int             i;
    detailtex_t    *dt;
    ded_detailtexture_t *def;

    // Search through the assignments.
    for(i = defs.count.details.num - 1; i >= 0; i--)
    {
        def = defs.details + i;
        dt = &details[i];

        // Is there a detail texture assigned for this?
        if(dt->detailLump < 0 && !def->isExternal)
            continue;

        if((is_wall_texture && index == dt->wallTexture) ||
           (!is_wall_texture && index == dt->flatTexture))
        {
            if(dtdef)
            {
                *dtdef = def;
            }

            // Hey, a match. Load this?
            if(!dt->glTex)
            {
                dt->glTex =
                    GL_LoadDetailTexture(dt->detailLump, def->strength,
                                         (def->isExternal?
                                          def->detailLump.path : NULL));
            }
            return dt->glTex;
        }
    }

    return 0; // There is no detail texture for this.
}

/**
 * @param result        If not @c NULL, the outcome will be written back
 *                      to here:
 *                      0 = already prepared
 *                      1 = found and prepared a lump resource.
 *                      2 = found and prepared an external resource.
 *
 * @return              The OpenGL name of the texture.
 */
static unsigned int prepareFlat2(material_t* mat, byte* result)
{
    if(!mat)
        return 0;

    if(mat->dgl.tex)
    {   // Already prepared.
        if(result)
            *result = 0;
    }
    else
    {
        byte*               flatptr;
        int                 width, height, pixSize = 3;
        boolean             RGBData = false, freeptr = false;
        ded_detailtexture_t* def;
        image_t             image;
        int                 lump = flats[mat->ofTypeID]->lump;

        // Try to load a high resolution version of this flat.
        if((loadExtAlways || highResWithPWAD ||
            !R_IsCustomMaterial(mat->ofTypeID, MAT_FLAT)) &&
           (flatptr = GL_LoadHighResFlat(&image, mat->name)) != NULL)
        {
            RGBData = true;
            freeptr = true;
            width = image.width;
            height = image.height;
            pixSize = image.pixelSize;

            if(result)
                *result = 2;
        }
        else
        {
            if(lumpInfo[lump].size < 4096)
                return 0; // Too small.

            // Get a pointer to the texture data.
            flatptr = W_CacheLumpNum(lump, PU_CACHE);
            width = height = 64;

            if(result)
                *result = 1;
        }

        mat->dgl.masked = false;

        // Is there a detail texture for this?
        if((mat->detail.tex =
            GL_PrepareDetailTexture(mat->ofTypeID, false, &def)))
        {
            // The width and height could be used for something meaningful.
            mat->detail.width = 128;
            mat->detail.height = 128;
            mat->detail.scale = def->scale;
            mat->detail.strength = def->strength;
            mat->detail.maxDist = def->maxDist;
        }

        // Load the texture.
        mat->dgl.tex =
            GL_UploadTexture(flatptr, width, height,
                             pixSize == 4, true, RGBData, false, false,
                             glmode[mipmapping], glmode[texMagMode],
                             texAniso,
                             DGL_REPEAT, DGL_REPEAT, 0);

        // Average color for glow planes.
        if(RGBData)
        {
            averageColorRGB(mat->dgl.color, flatptr, width, height);
        }
        else
        {
            averageColorIdx(mat->dgl.color, flatptr, width, height,
                            GL_GetPalette(), false);
        }

        if(freeptr)
            M_Free(flatptr);
    }

    return mat->dgl.tex;
}

/**
 * Prepares one of the "Doomsday Textures" 'which' must be one
 * of the DDT_* constants.
 */
static DGLuint prepareDDTexture(material_t* mat)
{
    int                 which = mat->ofTypeID;
    static const char *ddTexNames[NUM_DD_TEXTURES] = {
        "unknown",
        "missing",
        "bbox",
        "gray"
    };

    if(which < NUM_DD_TEXTURES)
    {
        if(!mat->dgl.tex)
        {
            mat->dgl.tex =
                GL_LoadGraphics2(RC_GRAPHICS, ddTexNames[which], LGM_NORMAL,
                                 true, false, 0);
            if(!mat->dgl.tex)
                Con_Error("prepareDDTexture: \"%s\" not found!\n",
                          ddTexNames[which]);
        }
    }
    else
        Con_Error("prepareDDTexture: Invalid ddtexture %i\n", which);

    return mat->dgl.tex;
}

/**
 * Loads PCX, TGA and PNG images. The returned buffer must be freed
 * with M_Free. Color keying is done if "-ck." is found in the filename.
 * The allocated memory buffer always has enough space for 4-component
 * colors.
 */
byte *GL_LoadImage(image_t *img, const char *imagefn, boolean useModelPath)
{
    DFILE              *file;
    char                ext[40];
    int                 format;

    // Clear any old values.
    memset(img, 0, sizeof(*img));

    if(useModelPath)
    {
        if(!R_FindModelFile(imagefn, img->fileName))
            return NULL; // Not found.
    }
    else
    {
        strcpy(img->fileName, imagefn);
    }

    // We know how to load PCX, TGA and PNG.
    M_GetFileExt(img->fileName, ext);
    if(!strcmp(ext, "pcx"))
    {
        img->pixels =
            PCX_AllocLoad(img->fileName, &img->width, &img->height, NULL);
        img->pixelSize = 3;     // PCXs can't be masked.
        img->originalBits = 8;
    }
    else if(!strcmp(ext, "tga"))
    {
        if(!TGA_GetSize(img->fileName, &img->width, &img->height))
            return NULL;

        file = F_Open(img->fileName, "rb");
        if(!file)
            return NULL;

        // Allocate a big enough buffer and read in the image.
        img->pixels = M_Malloc(4 * img->width * img->height);
        format =
            TGA_Load32_rgba8888(file, img->width, img->height, img->pixels);
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
    else if(!strcmp(ext, "png"))
    {
        img->pixels =
            PNG_Load(img->fileName, &img->width, &img->height,
                     &img->pixelSize);
        img->originalBits = 8 * img->pixelSize;

        if(img->pixels == NULL)
            return NULL;
    }

    VERBOSE(Con_Message
            ("LoadImage: %s (%ix%i)\n", M_PrettyPath(img->fileName), img->width,
             img->height));

    // How about some color-keying?
    if(GL_IsColorKeyed(img->fileName))
    {
        byte               *out;

        out = GL_ApplyColorKeying(img->pixels, img->pixelSize, img->width,
                                  img->height);
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

/**
 * First sees if there is a color-keyed version of the given image. If
 * there is it is loaded. Otherwise the 'regular' version is loaded.
 */
byte *GL_LoadImageCK(image_t * img, const char *name, boolean useModelPath)
{
    char                keyFileName[256];
    byte               *pixels;
    char               *ptr;

    strcpy(keyFileName, name);

    // Append the "-ck" and try to load.
    ptr = strrchr(keyFileName, '.');
    if(ptr)
    {
        memmove(ptr + 3, ptr, strlen(ptr) + 1);
        ptr[0] = '-';
        ptr[1] = 'c';
        ptr[2] = 'k';
        if((pixels = GL_LoadImage(img, keyFileName, useModelPath)) != NULL)
            return pixels;
    }

    return GL_LoadImage(img, name, useModelPath);
}

/**
 * Frees all memory associated with the image.
 */
void GL_DestroyImage(image_t * img)
{
    M_Free(img->pixels);
    img->pixels = NULL;
}

/**
 * Name must end in \0.
 */
byte *GL_LoadHighRes(image_t* img, const char* name, char* prefix,
                     boolean allowColorKey, resourceclass_t resClass)
{
    filename_t          resource, fileName;

    // Form the resource name.
    sprintf(resource, "%s%s", prefix, name);

    if(!R_FindResource
       (resClass, resource, allowColorKey ? "-ck" : NULL, fileName))
    {
        // There is no such external resource file.
        return NULL;
    }

    return GL_LoadImage(img, fileName, false);
}

/**
 * Use this when loading custom textures from the Data\*\Textures dir.
 * The returned buffer must be freed with M_Free.
 */
byte *GL_LoadTexture(image_t* img, const char* name)
{
    return GL_LoadHighRes(img, name, "", true, RC_TEXTURE);
}

/**
 * Use this when loading high-res wall textures.
 * The returned buffer must be freed with M_Free.
 */
byte *GL_LoadHighResTexture(image_t *img, const char* name)
{
    if(noHighResTex)
        return NULL;
    return GL_LoadTexture(img, name);
}

/**
 * The returned buffer must be freed with M_Free.
 */
byte *GL_LoadHighResFlat(image_t *img, const char *name)
{
    byte *ptr;

    if(noHighResTex)
        return NULL;

    // First try the Flats category.
    if((ptr = GL_LoadHighRes(img, name, "", false, RC_FLAT)) != NULL)
        return ptr;

    // Try the old-fashioned "Flat-NAME" in the Textures category.
    return GL_LoadHighRes(img, name, "flat-", false, RC_TEXTURE);
}

/**
 * The returned buffer must be freed with M_Free.
 */
byte *GL_LoadHighResPatch(image_t *img, char *name)
{
    if(noHighResTex)
        return NULL;
    return GL_LoadHighRes(img, name, "", true, RC_PATCH);
}

DGLuint GL_LoadGraphics(const char *name, gfxmode_t mode)
{
    return GL_LoadGraphics2(RC_GRAPHICS, name, mode, false, true, 0);
}

DGLuint GL_LoadGraphics2(resourceclass_t resClass, const char *name,
                         gfxmode_t mode, int useMipmap, boolean clamped,
                         int otherFlags)
{
    return GL_LoadGraphics4(resClass, name, mode, useMipmap,
                            DGL_LINEAR, DGL_LINEAR, 0 /*no anisotropy*/,
                            clamped? DGL_CLAMP : DGL_REPEAT,
                            clamped? DGL_CLAMP : DGL_REPEAT, otherFlags);
}

DGLuint GL_LoadGraphics3(const char *name, gfxmode_t mode,
                         int minFilter, int magFilter, int anisoFilter,
                         int wrapS, int wrapT, int otherFlags)
{
    return GL_LoadGraphics4(RC_GRAPHICS, name, mode, false,
                            minFilter, magFilter, anisoFilter, wrapS, wrapT,
                            otherFlags);
}

/**
 * Extended version that uses a custom resource class.
 * Set mode to 2 to include an alpha channel. Set to 3 to make the
 * actual pixel colors all white.
 */
DGLuint GL_LoadGraphics4(resourceclass_t resClass, const char *name,
                         gfxmode_t mode, int useMipmap,
                         int minFilter, int magFilter, int anisoFilter,
                         int wrapS, int wrapT, int otherFlags)
{
    image_t image;
    filename_t fileName;
    DGLuint texture = 0;

    if(R_FindResource(resClass, name, NULL, fileName) &&
       GL_LoadImage(&image, fileName, false))
    {
        // Too big for us?
        if(image.width > glMaxTexSize || image.height > glMaxTexSize)
        {
            int     newWidth = MIN_OF(image.width, glMaxTexSize);
            int     newHeight = MIN_OF(image.height, glMaxTexSize);
            byte   *temp = M_Malloc(newWidth * newHeight * image.pixelSize);

            GL_ScaleBuffer32(image.pixels, image.width, image.height, temp,
                             newWidth, newHeight, image.pixelSize);
            M_Free(image.pixels);
            image.pixels = temp;
            image.width = newWidth;
            image.height = newHeight;
        }

        // Force it to grayscale?
        if(mode == LGM_GRAYSCALE_ALPHA || mode == LGM_WHITE_ALPHA)
        {
            GL_ConvertToAlpha(&image, mode == LGM_WHITE_ALPHA);
        }
        else if(mode == LGM_GRAYSCALE)
        {
            GL_ConvertToLuminance(&image);
        }

        /*
        texture = DGL_NewTexture();
        if(image.width < 128 && image.height < 128)
        {
            // Small textures will never be compressed.
            DGL_Disable(DGL_TEXTURE_COMPRESSION);
        }
        DGL_TexImage(image.pixelSize ==
                    2 ? DGL_LUMINANCE_PLUS_A8 : image.pixelSize ==
                    3 ? DGL_RGB : image.pixelSize ==
                    4 ? DGL_RGBA : DGL_LUMINANCE, image.width, image.height,
                    useMipmap, image.pixels);
        DGL_Enable(DGL_TEXTURE_COMPRESSION);
        DGL_TexFilter(DGL_MAG_FILTER, glmode[texMagMode]);
        DGL_TexFilter(DGL_MIN_FILTER,
                        useMipmap ? glmode[mipmapping] : DGL_LINEAR);
        if(clamped)
        {
            DGL_TexFilter(DGL_WRAP_S, DGL_CLAMP);
            DGL_TexFilter(DGL_WRAP_T, DGL_CLAMP);
        }*/

        texture = GL_NewTextureWithParams2(( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                                             image.pixelSize == 3 ? DGL_RGB :
                                             image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
                                           image.width, image.height, image.pixels,
                                           ( otherFlags | (useMipmap? TXCF_MIPMAP : 0) |
                                             (useMipmap == DGL_GRAY_MIPMAP? TXCF_GRAY_MIPMAP : 0) |
                                             (image.width < 128 && image.height < 128? TXCF_NO_COMPRESSION : 0) ),
                                           (useMipmap ? glmode[mipmapping] : DGL_LINEAR),
                                           magFilter, texAniso,
                                           wrapS, wrapT);

        GL_DestroyImage(&image);
    }
    return texture;
}

/**
 * Renders the given texture into the buffer.
 */
static boolean bufferTexture(material_t* mat, byte *buffer, int width,
                             int height, int* has_big_patch)
{
    int                 i, len;
    boolean             alphaChannel = false;
    lumppatch_t*        patch;
    const texturedef_t* texDef = R_GetTextureDef(mat->ofTypeID);

    len = width * height;

    // Clear the buffer.
    memset(buffer, 0, 2 * len);

    // By default zero is put in the big patch height.
    if(has_big_patch)
        *has_big_patch = 0;

    // Draw all the patches. Check for alpha pixels after last patch has
    // been drawn.
    for(i = 0; i < texDef->patchCount; ++i)
    {
        const texpatch_t*   patchDef = &texDef->patches[i];

        patch = (lumppatch_t*) W_CacheLumpNum(patchDef->lump, PU_CACHE);

        // Check for big patches?
        if(SHORT(patch->height) > mat->height && has_big_patch &&
           *has_big_patch < SHORT(patch->height))
        {
            *has_big_patch = SHORT(patch->height);
        }

        // Draw the patch in the buffer.
        alphaChannel =
            DrawRealPatch(buffer, /*palette,*/ width, height, patch,
                          patchDef->offX, patchDef->offY,
                          false, 0, i == texDef->patchCount - 1);
    }

    W_ChangeCacheTag(palLump, PU_CACHE);
    return alphaChannel;
}

/**
 * Draws the given sky texture in a buffer. The returned buffer must be
 * freed by the caller. Idx must be a valid texture number.
 */
static void bufferSkyTexture(skytexture_t* skyTex, byte **outbuffer, int width,
                             int height, boolean zeroMask)
{
    int                 idx = skyTex->texture;
    const texturedef_t* tex = R_GetTextureDef(idx);
    byte*               imgdata;
    int                 i, numpels;

    if(tex->patchCount > 1)
    {
        numpels = width * height;
        imgdata = M_Calloc(2 * numpels);

        for(i = 0; i < tex->patchCount; ++i)
        {
            const texpatch_t*   patchDef = &tex->patches[i];

            DrawRealPatch(imgdata, /*palette,*/ width, height,
                          W_CacheLumpNum(patchDef->lump, PU_CACHE),
                          patchDef->offX, patchDef->offY,
                          zeroMask, 0, false);
        }
    }
    else
    {
        lumppatch_t *patch =
            (lumppatch_t*) W_CacheLumpNum(tex->patches[0].lump, PU_CACHE);
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
                      zeroMask, 0, false);
    }

    if(zeroMask && fillOutlines)
        ColorOutlines(imgdata, width, height);
    *outbuffer = imgdata;
}

/**
 * @return              The DGL texture name.
 */
static DGLuint prepareTexture2(material_t* mat, byte* result)
{
    if(!mat)
        return 0;

    if(mat->dgl.tex)
    {   // Already prepared.
        if(result)
            *result = 0;
    }
    else
    {
        int                 i;
        image_t             image;
        ded_detailtexture_t *def;
        boolean             alphaChannel = false, RGBData = false;

        // Try to load a high resolution version of this texture.
        if((loadExtAlways || highResWithPWAD ||
            !R_IsCustomMaterial(mat->ofTypeID, MAT_TEXTURE)) &&
           GL_LoadHighResTexture(&image, mat->name) != NULL)
        {
            // High resolution texture loaded.
            RGBData = true;
            alphaChannel = (image.pixelSize == 4);
            if(result)
                *result = 2;
        }
        else
        {
            image.width = mat->width;
            image.height = mat->height;
            image.pixels = M_Malloc(2 * image.width * image.height);
            image.isMasked =
                bufferTexture(mat, image.pixels, image.width, image.height,
                                 &i);
            if(result)
                *result = 1;

            // The -bigmtex option allows the engine to resize masked
            // textures whose patches are too big to fit the texture.
            if(allowMaskedTexEnlarge && image.isMasked && i)
            {
                // Adjust the defined height to fit the largest patch.
                mat->height = image.height = i;
                // Get a new buffer.
                M_Free(image.pixels);
                image.pixels = M_Malloc(2 * image.width * image.height);
                image.isMasked =
                    bufferTexture(mat, image.pixels, image.width,
                                     image.height, 0);
            }
            // "alphaChannel" and "masked" are the same thing for indexed
            // images.
            alphaChannel = image.isMasked;
        }

        // Load a detail texture (if one is defined).
        if((mat->detail.tex =
            GL_PrepareDetailTexture(mat->ofTypeID, true, &def)))
        {
            // The width and height could be used for something meaningful.
            mat->detail.width = 128;
            mat->detail.height = 128;
            mat->detail.scale = def->scale;
            mat->detail.strength = def->strength;
            mat->detail.maxDist = def->maxDist;
        }

        mat->dgl.tex =
            GL_UploadTexture(image.pixels, image.width, image.height,
                             alphaChannel, true, RGBData, false, false,
                             glmode[mipmapping], glmode[texMagMode], texAniso,
                             DGL_REPEAT, DGL_REPEAT, 0);

        // Average color for glow planes.
        if(RGBData)
        {
            averageColorRGB(mat->dgl.color, image.pixels, image.width,
                            image.height);
        }
        else
        {
            averageColorIdx(mat->dgl.color, image.pixels, image.width,
                            image.height, GL_GetPalette(), false);
        }

        mat->dgl.masked = (image.isMasked != 0);

        GL_DestroyImage(&image);
    }

    return mat->dgl.tex;
}

static DGLuint prepareSky2(skytexture_t* skyTex, boolean zeroMask,
                           byte* result)
{
    if(skyTex->tex)
    {   // Already prepared.
        if(result)
            *result = 0;
    }
    else
    {
        image_t             image;
        const texturedef_t* texDef = R_GetTextureDef(skyTex->texture);
        boolean             RGBData = false, alphaChannel = false;

        // Try to load a high resolution version of this texture.
        if((loadExtAlways || highResWithPWAD ||
            R_TextureIsFromIWAD(skyTex->texture)) &&
            GL_LoadHighResTexture(&image, texDef->name) != NULL)
        {
            // High resolution texture loaded.
            RGBData = true;
            alphaChannel = (image.pixelSize == 4);
            if(result)
                *result = 2;
        }
        else
        {
            image.width = texDef->width;
            image.height = texDef->height;
            bufferSkyTexture(skyTex, &image.pixels, image.width,
                             image.height, zeroMask);

            if(result)
                *result = 1;

            // "alphaChannel" and "masked" are the same thing for indexed
            // images.
            alphaChannel = image.isMasked = zeroMask;
        }

        // Always disable compression on sky textures.
        // Upload it.
        skyTex->tex =
            GL_UploadTexture(image.pixels, image.width, image.height,
                             alphaChannel, true, RGBData, false, false,
                             glmode[mipmapping], glmode[texMagMode], texAniso,
                             DGL_REPEAT, DGL_REPEAT, TXCF_NO_COMPRESSION);

        // Calculate the averaged top line color.
        if(RGBData)
        {
            lineAverageColorRGB(skyTex->topRGB, image.pixels, image.width,
                                image.height, 0);
        }
        else
        {
            lineAverageColorIdx(skyTex->topRGB, image.pixels, image.width,
                                image.height, 0, GL_GetPalette(), false);
        }

        // Free the buffer.
        GL_DestroyImage(&image);
    }

    return skyTex->tex;
}

/**
 * @return              The DGL texture name.
 */
DGLuint GL_PrepareSky(skytexture_t* skyTex, boolean zeroMask)
{
    return prepareSky2(skyTex, zeroMask, NULL);
}

transspr_t *GL_NewTranslatedSprite(int pnum, unsigned char *table)
{
    int             i;
    transspr_t    **newlist, *ptr;

    newlist = Z_Malloc(sizeof(transspr_t*) * ++numTransSprites, PU_SPRITE, 0);
    if(numTransSprites > 1)
    {
        for(i = 0; i < numTransSprites -1; ++i)
            newlist[i] = transSprites[i];

        Z_Free(transSprites);
    }

    transSprites = newlist;
    ptr = transSprites[numTransSprites - 1] = Z_Calloc(sizeof(transspr_t), PU_SPRITE, 0);
    ptr->patch = pnum;
    ptr->tex = 0;
    ptr->table = table;
    return ptr;
}

transspr_t *GL_GetTranslatedSprite(int pnum, unsigned char *table)
{
    int     i;

    for(i = 0; i < numTransSprites; ++i)
        if(transSprites[i]->patch == pnum && transSprites[i]->table == table)
            return transSprites[i];
    return 0;
}

/**
 * Uploads the sprite in the buffer and sets the appropriate texture
 * parameters.
 */
unsigned int GL_PrepareSpriteBuffer(int pnum, image_t *image,
                                    boolean isPsprite)
{
    unsigned int texture = 0;

    if(image->pixelSize == 1 && fillOutlines)
        ColorOutlines(image->pixels, image->width, image->height);

    texture =
        GL_UploadTexture(image->pixels, image->width, image->height,
                         image->pixelSize != 3, true, image->pixelSize > 1,
                         true, false, glmode[mipmapping],
                         (filterSprites ? DGL_LINEAR : DGL_NEAREST), texAniso,
                         DGL_CLAMP, DGL_CLAMP, 0);

    return texture;
}

unsigned int GL_PrepareTranslatedSprite(int pnum, int tmap, int tclass)
{
    unsigned char *table =
        translationTables - 256 + tclass * ((8 - 1) * 256) + tmap * 256;
    transspr_t *tspr = GL_GetTranslatedSprite(pnum, table);
    image_t image;

    if(!tspr)
    {
        filename_t resource, fileName;
        lumppatch_t *patch =
            (lumppatch_t*) W_CacheLumpNum(spriteTextures[pnum]->lump, PU_CACHE);

        // Compose a resource name.
        if(tclass || tmap)
        {
            sprintf(resource, "%s-table%i%i",
                    lumpInfo[spriteTextures[pnum]->lump].name, tclass, tmap);
        }
        else
        {
            // Not actually translated? Use the normal resource.
            strcpy(resource, lumpInfo[spriteTextures[pnum]->lump].name);
        }

        if(!noHighResPatches &&
           R_FindResource(RC_PATCH, resource, "-ck", fileName) &&
           GL_LoadImage(&image, fileName, false) != NULL)
        {
            // The buffer is now filled with the data.
        }
        else
        {
            // Must load from the normal lump.
            image.width = SHORT(patch->width);
            image.height = SHORT(patch->height);
            image.pixelSize = 1;
            image.pixels = M_Calloc(2 * image.width * image.height);

            DrawRealPatch(image.pixels, image.width, image.height, patch,
                          0, 0, false, table, false);
        }

        tspr = GL_NewTranslatedSprite(pnum, table);
        tspr->tex = GL_PrepareSpriteBuffer(pnum, &image, false);
        // Determine coordinates for the texture.
        GL_SetTexCoords(spriteTextures[pnum]->texCoord, image.width,
                        image.height);
        {
        spritetex_t        *sprTex = spriteTextures[pnum];
        lumppatch_t        *patch =
            (lumppatch_t*) W_CacheLumpNum(sprTex->lump, PU_CACHE);

        // Calculate light source properties.
        GL_CalcLuminance(pnum, image.pixels, image.width,
                         image.height, image.pixelSize,
                         &tspr->flareX, &tspr->flareY,
                         &tspr->color);
        }

        GL_DestroyImage(&image);
    }
    return tspr->tex;
}

static DGLuint prepareSprite(material_t* mat)
{
    int                 lumpNum;
    spritetex_t*        sprTex;
    int                 pnum = mat->ofTypeID;

    if(pnum < 0)
        return 0;

    sprTex = spriteTextures[pnum];
    lumpNum = sprTex->lump;

    if(!mat->dgl.tex)
    {
        image_t             image;
        filename_t          fileName;
        lumppatch_t*        patch =
            (lumppatch_t*) W_CacheLumpNum(lumpNum, PU_CACHE);
        boolean             RGBData = false;

        // Is there an external resource for this image?
        if(!noHighResPatches &&
           R_FindResource(RC_PATCH, lumpInfo[lumpNum].name, "-ck", fileName)
           && GL_LoadImage(&image, fileName, false) != NULL)
        {
            // A high-resolution version of this sprite has been found.
            // The buffer has been filled with the data we need.
            RGBData = true;
        }
        else
        {
            // There's no name for this patch, load it in.
            image.width = SHORT(patch->width);
            image.height = SHORT(patch->height);
            image.pixels = M_Calloc(2 * image.width * image.height);
            image.pixelSize = 1;

            DrawRealPatch(image.pixels, image.width, image.height, patch,
                          0, 0, false, 0, false);
        }

        // Average color for glow planes.
        if(RGBData)
        {
            averageColorRGB(mat->dgl.color, image.pixels, image.width,
                            image.height);
        }
        else
        {
            averageColorIdx(mat->dgl.color, image.pixels, image.width,
                            image.height, GL_GetPalette(), false);
        }

        // Calculate light source properties.
        GL_CalcLuminance(pnum, image.pixels, image.width,
                         image.height, image.pixelSize,
                         &sprTex->flareX, &sprTex->flareY,
                         &sprTex->autoLightColor);

        mat->dgl.tex = GL_PrepareSpriteBuffer(pnum, &image, false);
        mat->dgl.masked = 1;
        // Determine coordinates for the texture.
        GL_SetTexCoords(sprTex->texCoord, image.width, image.height);

        GL_DestroyImage(&image);
    }

    return mat->dgl.tex;
}

unsigned int GL_PreparePSprite(int pnum)
{
    DGLuint*            texture;
    int                 lumpNum;
    spritetex_t*        sprTex;

    if(pnum < 0 || pnum >= numSpriteTextures)
        return 0;

    sprTex = spriteTextures[pnum];
    lumpNum = sprTex->lump;

    // Normal sprites and HUD sprites are stored separately.
    // This way a sprite can be used both in the game and the HUD, and
    // the textures can be different. (Very few sprites are used both
    // in the game and the HUD.)
    texture = &sprTex->pspriteTex;

    if(!*texture)
    {
        image_t             image;
        filename_t          hudResource, fileName;
        lumppatch_t*        patch =
            (lumppatch_t*) W_CacheLumpNum(lumpNum, PU_CACHE);

        // Compose a resource for the psprite.
        sprintf(hudResource, "%s-hud", lumpInfo[lumpNum].name);

        // Is there an external resource for this hud sprite?
        // First try the HUD version of the resource.
        if(!noHighResPatches &&
           (R_FindResource(RC_PATCH, hudResource, "-ck", fileName) ||
            R_FindResource(RC_PATCH, lumpInfo[lumpNum].name, "-ck", fileName))
           && GL_LoadImage(&image, fileName, false) != NULL)
        {
            // A high-resolution version of this sprite has been found.
            // The buffer has been filled with the data we need.
        }
        else
        {
            // There's no name for this patch, load it in.
            image.width = SHORT(patch->width);
            image.height = SHORT(patch->height);
            image.pixels = M_Calloc(2 * image.width * image.height);
            image.pixelSize = 1;

            DrawRealPatch(image.pixels, image.width, image.height, patch,
                          0, 0, false, 0, false);
        }

        *texture = GL_PrepareSpriteBuffer(pnum, &image, true);
        // Determine coordinates for the texture.
        GL_SetTexCoords(sprTex->texCoord, image.width, image.height);
        GL_DestroyImage(&image);
    }

    return *texture;
}

void GL_DeleteHUDSprite(int idx)
{
    spritetex_t*        sprite;

    if(idx < 0 || idx >= numSpriteTextures)
        return;
    sprite = spriteTextures[idx];

    if(sprite->pspriteTex)
    {
        DGL_DeleteTextures(1, &sprite->pspriteTex);
        sprite->pspriteTex = 0;
    }
}

void GL_SetPSprite(int pnum)
{
    GL_BindTexture(GL_PreparePSprite(pnum),
                   filterSprites? DGL_LINEAR : DGL_NEAREST);
}

void GL_SetTranslatedSprite(int pnum, int tmap, int tclass)
{
    GL_BindTexture(GL_PrepareTranslatedSprite(pnum, tmap, tclass),
                   filterSprites? DGL_LINEAR : DGL_NEAREST);
}

DGLuint GL_GetPatchOtherPart(int idx)
{
    patchtex_t*       patchTex = R_GetPatchTex(idx);

    if(!patchTex->tex)
    {
        // The patch isn't yet bound with OpenGL.
        patchTex->tex = GL_BindTexPatch(patchTex);
    }

    return patchTex->tex2;
}

DGLuint GL_GetRawOtherPart(int idx)
{
    rawtex_t*               raw = R_GetRawTex(idx);

    if(!raw->tex)
    {
        // The raw isn't yet bound with OpenGL.
        raw->tex = GL_BindTexRaw(raw);
    }

    return GL_GetRawTexInfo(idx, true);
}

/**
 * Raw images are always 320x200.
 *
 * 2003-05-30 (skyjake): External resources can be larger than 320x200,
 * but they're never split into two parts.
 */
DGLuint GL_BindTexRaw(rawtex_t *raw)
{
    image_t             image;
    int                 lump = raw->lump;

    if(lump < 0 || lump >= numLumps)
    {
        GL_BindTexture(0, 0);
        return 0;
    }

    if(!raw->tex)
    {
        // First try to find an external resource.
        filename_t          fileName;

        if(R_FindResource(RC_PATCH, lumpInfo[lump].name, NULL, fileName) &&
           GL_LoadImage(&image, fileName, false) != NULL)
        {
            // We have the image in the buffer. We'll upload it as one
            // big texture.
            raw->tex =
                GL_UploadTexture(image.pixels, image.width, image.height,
                                 image.pixelSize == 4, false, true, false, false,
                                 DGL_NEAREST, (linearRaw ? DGL_LINEAR : DGL_NEAREST),
                                 0 /*no anisotropy*/,
                                 DGL_CLAMP, DGL_CLAMP, 0);

            raw->width = 320;
            raw->width2 = 0;
            raw->tex2 = 0;
            raw->height = raw->height2 = 200;

            GL_DestroyImage(&image);
        }
        else
        {
            // Must load the old-fashioned data lump.
            byte   *lumpdata, *image;
            int     height, assumedWidth = 320;
            boolean need_free_image = true;
            boolean rgbdata;
            int     comps;

            // Load the raw image data.
            // It's most efficient to create two textures for it (256 + 64 = 320).
            lumpdata = W_CacheLumpNum(lump, PU_STATIC);
            height = 200;

            // Try to load it as a PCX image first.
            image = M_Malloc(3 * 320 * 200);
            if(PCX_MemoryLoad(lumpdata, lumpInfo[lump].size, 320, 200, image))
            {
                rgbdata = true;
                comps = 3;
            }
            else
            {
                // PCX load failed. It must be an old-fashioned raw image.
                need_free_image = false;
                M_Free(image);
                height = (int) (lumpInfo[lump].size / 320);
                rgbdata = false;
                comps = 1;
                image = lumpdata;
            }

            if(!(height < 200))
            {
                int     i, k, c, idx;
                byte   *dat1, *dat2;

                // Two pieces:
                dat1 = M_Calloc(comps * 256 * 256);
                dat2 = M_Calloc(comps * 64 * 256);

                // Image data loaded, divide it into two parts.
                for(k = 0; k < height; ++k)
                    for(i = 0; i < 256; ++i)
                    {
                        idx = k * assumedWidth + i;
                        // Part one.
                        for(c = 0; c < comps; ++c)
                            dat1[(k * 256 + i) * comps + c] = image[idx * comps + c];

                        // We can setup part two at the same time.
                        for(c = 0; c < comps; ++c)
                        {
                            dat2[(k * 64 + i) * comps + c] =
                                image[(idx + 256) * comps + c];
                        }
                    }

                // Upload part one.
                raw->tex =
                    GL_UploadTexture(dat1, 256, assumedWidth < 320 ? height : 256, false,
                                     false, rgbdata, false, false,
                                     DGL_NEAREST, (linearRaw ? DGL_LINEAR : DGL_NEAREST),
                                     0 /*no anisotropy*/,
                                     DGL_CLAMP, DGL_CLAMP, 0);

                // And the other part.
                raw->tex2 =
                    GL_UploadTexture(dat2, 64, 256, false, false, rgbdata, false, false,
                                     DGL_NEAREST, (linearRaw ? DGL_LINEAR : DGL_NEAREST),
                                     0 /*no anisotropy*/,
                                     DGL_CLAMP, DGL_CLAMP, 0);

                raw->width = 256;
                raw->width2 = 64;
                raw->height = raw->height2 = height;
                M_Free(dat1);
                M_Free(dat2);
            }
            else // We can use the normal one-part method.
            {
                assumedWidth = 256;

                // Generate a texture.
                raw->tex =
                    GL_UploadTexture(image, 256, height,
                                     false, false, rgbdata, false, false,
                                     DGL_NEAREST, (linearRaw ? DGL_LINEAR : DGL_NEAREST),
                                     0 /*no anisotropy*/,
                                     DGL_CLAMP, DGL_CLAMP, 0);

                raw->width = 256;
                raw->height = height;

                raw->tex2 = 0;
                raw->width2 = raw->height2 = 0;
            }

            if(need_free_image)
                M_Free(image);
            W_ChangeCacheTag(lump, PU_CACHE);
        }
    }

    return raw->tex;
}

/**
 * Returns the OpenGL name of the texture.
 */
DGLuint GL_PrepareRawTex(lumpnum_t lump, boolean part2)
{
    rawtex_t           *raw = R_GetRawTex(lump);

    if(!raw->tex)
    {
        // The rawtex isn't yet bound with OpenGL.
        raw->tex = GL_BindTexRaw(raw);
    }

    return GL_GetRawTexInfo(lump, part2);
}

unsigned int GL_SetRawImage(lumpnum_t lump, boolean part2, int wrapS,
                            int wrapT)
{
    DGLuint             tex;

    // We don't track the current texture with raw images.
    curTex = 0;

    GL_BindTexture(tex = GL_PrepareRawTex(lump, part2),
                   (linearRaw ? DGL_LINEAR : DGL_NEAREST));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    wrapS == DGL_CLAMP? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    wrapT == DGL_CLAMP? GL_CLAMP_TO_EDGE : GL_REPEAT);
    return (unsigned int) tex;
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
}

#if 0 // Currently unused.
static void EnhanceContrast(byte *pixels, int width, int height)
{
    int     i, c;
    byte*   pix;

    for(i = 0, pix = pixels; i < width * height; ++i, pix += 4)
    {
        for(c = 0; c < 3; ++c)
        {
            float f = pix[c];
            if(f < 90) // Darken dark parts.
                f = (f - 90) * 3 + 90;
            else if(f > 200) // Lighten light parts.
                f = (f - 200) * 2 + 200;

            pix[c] = MINMAX_OF(0, f, 255);
        }
    }
}
#endif

static void SharpenPixels(byte* pixels, int width, int height)
{
    int     x, y, c;
    byte*   result = M_Calloc(4 * width * height);
    const float strength = .05f;
    float   A, B, C;

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
 * NOTE: No mipmaps are generated for regular patches.
 */
DGLuint GL_BindTexPatch(patchtex_t* p)
{
    lumppatch_t        *patch;
    byte               *patchptr;
    image_t             image;
    lumpnum_t           lump = p->lump;

    if(lump < 0 || lump >= numLumps)
    {
        GL_BindTexture(0, 0);
        return 0;
    }

    patch = (lumppatch_t*) W_CacheLumpNum(lump, PU_CACHE);
    p->offX = -SHORT(patch->leftOffset);
    p->offY = -SHORT(patch->topOffset);
    p->extraOffset[VX] = p->extraOffset[VY] = 0;

    // Let's first try the resource locator and see if there is a
    // 'high-resolution' version available.
    if((loadExtAlways || highResWithPWAD || W_IsFromIWAD(lump)) &&
       (patchptr = GL_LoadHighResPatch(&image, lumpInfo[lump].name)) != NULL)
    {
        // This is our texture! No mipmaps are generated.
        p->tex =
            GL_UploadTexture(image.pixels, image.width, image.height,
                             image.pixelSize == 4, false, true, false, false,
                             DGL_NEAREST, glmode[texMagMode], texAniso,
                             DGL_CLAMP, DGL_CLAMP, 0);

        p->width = SHORT(patch->width);
        p->height = SHORT(patch->height);

        // The original image is no longer needed.
        GL_DestroyImage(&image);
    }
    else
    {
        // Use data from the normal lump.
        boolean scaleSharp = (upscaleAndSharpenPatches ||
                              (p->flags & PF_UPSCALE_AND_SHARPEN));
        int     addBorder = (scaleSharp? 1 : 0);
        int     patchWidth = SHORT(patch->width) + addBorder*2;
        int     patchHeight = SHORT(patch->height) + addBorder*2;
        int     numpels = patchWidth * patchHeight, alphaChannel;
        byte   *buffer;

        if(!numpels)
            return 0; // This won't do!

        // Allocate memory for the patch.
        buffer = M_Calloc(2 * numpels);

        alphaChannel =
            DrawRealPatch(buffer, patchWidth, patchHeight,
                          patch, addBorder, addBorder, false, 0, true);

        if(fillOutlines && !scaleSharp)
            ColorOutlines(buffer, patchWidth, patchHeight);

        if(monochrome || (p->flags & PF_MONOCHROME))
        {
            DeSaturate(buffer, GL_GetPalette(), patchWidth, patchHeight);
            p->flags |= PF_MONOCHROME;
        }

        if(scaleSharp)
        {
            byte* rgbaPixels = M_Malloc(numpels * 4 * 2); // also for the final output
            byte* upscaledPixels = M_Malloc(numpels * 4 * 4);

            GL_ConvertBuffer(patchWidth, patchHeight, 2, 4, buffer, rgbaPixels,
                             GL_GetPalette(), false);

            GL_SmartFilter2x(rgbaPixels, upscaledPixels, patchWidth, patchHeight,
                             patchWidth * 8);
            patchWidth *= 2;
            patchHeight *= 2;

            /*
            {
                static int counter = 1;
                FILE *f;
                char buf[100];
                sprintf(buf, "dumped-%s-%i.dat", W_LumpName(p->lump), counter++);
                f = fopen(buf, "wb");
                fwrite(upscaledPixels, 4 * 4 * numpels, 1, f);
                fclose(f);
            }
            */
            /*
            Con_Message("upscale and sharpen on %s (lump %i) monochrome:%i\n", W_LumpName(p->lump),
                        p->lump, p->flags & PF_MONOCHROME);
             */

            //EnhanceContrast(upscaledPixels, patchWidth, patchHeight);
            SharpenPixels(upscaledPixels, patchWidth, patchHeight);
            BlackOutlines(upscaledPixels, patchWidth, patchHeight);

            // Back to indexed+alpha.
            GL_ConvertBuffer(patchWidth, patchHeight, 4, 2, upscaledPixels, rgbaPixels,
                             GL_GetPalette(), false);

            // Replace the old buffer.
            M_Free(upscaledPixels);
            M_Free(buffer);
            buffer = rgbaPixels;

            // We'll sharpen it in the future as well.
            p->flags |= PF_UPSCALE_AND_SHARPEN;
        }

        // See if we have to split the patch into two parts.
        // This is done to conserve the quality of wide textures
        // (like the status bar) on video cards that have a pitifully
        // small maximum texture size. ;-)
        if(SHORT(patch->width) > glMaxTexSize)
        {
            // The width of the first part is glMaxTexSize.
            int     part2width = patchWidth - glMaxTexSize;
            byte   *tempbuff =
                M_Malloc(2 * MAX_OF(glMaxTexSize, part2width) *
                         patchHeight);

            // We'll use a temporary buffer for doing to splitting.
            // First, part one.
            pixBlt(buffer, patchWidth, patchHeight, tempbuff,
                   glMaxTexSize, patchHeight, alphaChannel,
                   0, 0, 0, 0, glMaxTexSize, patchHeight);
            p->tex =
                GL_UploadTexture(tempbuff, glMaxTexSize, patchHeight,
                                 alphaChannel, false, false, false, false,
                                 DGL_NEAREST, DGL_LINEAR, texAniso,
                                 DGL_CLAMP, DGL_CLAMP, 0);

            // Then part two.
            pixBlt(buffer, patchWidth, patchHeight, tempbuff,
                   part2width, patchHeight, alphaChannel, glMaxTexSize,
                   0, 0, 0, part2width, patchHeight);
            p->tex2 =
                GL_UploadTexture(tempbuff, part2width, patchHeight,
                                 alphaChannel, false, false, false, false,
                                 DGL_NEAREST, glmode[texMagMode], texAniso,
                                 DGL_CLAMP, DGL_CLAMP, 0);

            p->width = glMaxTexSize;
            p->width2 = SHORT(patch->width) - glMaxTexSize;
            p->height = p->height2 = SHORT(patch->height);

            M_Free(tempbuff);
        }
        else // We can use the normal one-part method.
        {
            // Generate a texture.
            p->tex =
                GL_UploadTexture(buffer, patchWidth, patchHeight,
                                 alphaChannel, false, false, false, false,
                                 DGL_NEAREST, glmode[texMagMode], texAniso,
                                 DGL_CLAMP, DGL_CLAMP, 0);

            p->width = SHORT(patch->width) + addBorder*2;
            p->height = SHORT(patch->height) + addBorder*2;
            p->extraOffset[VX] = -addBorder;
            p->extraOffset[VY] = -addBorder;

            p->tex2 = 0;
            p->width2 = p->height2 = 0;
        }
        M_Free(buffer);
    }

    return p->tex;
}

/**
 * Returns the OpenGL name of the texture.
 */
DGLuint GL_PreparePatch(lumpnum_t lump)
{
    patchtex_t*         patchTex = R_GetPatchTex(lump);

    if(!patchTex)
        return 0;

    if(!patchTex->tex)
    {   // The patch isn't yet bound with OpenGL.
        patchTex->tex = GL_BindTexPatch(patchTex);
    }

    return patchTex->tex;
}

void GL_SetPatch(lumpnum_t lump, int wrapS, int wrapT)
{
    GL_BindTexture(curTex = GL_PreparePatch(lump), glmode[texMagMode]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    wrapS == DGL_CLAMP? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    wrapT == DGL_CLAMP? GL_CLAMP_TO_EDGE : GL_REPEAT);
}

/**
 * You should use Disable(DGL_TEXTURING) instead of this.
 */
void GL_SetNoTexture(void)
{
    DGL_Bind(0);
    curTex = 0;
}

/**
 * Prepare a texture used in the lighting system. 'which' must be one
 * of the LST_* constants.
 */
DGLuint GL_PrepareLSTexture(lightingtexid_t which)
{
    switch(which)
    {
    case LST_DYNAMIC:
        // The dynamic light map is a 64x64 grayscale 8-bit image.
        if(!lightingTextures[LST_DYNAMIC].tex)
        {
            // We don't want to compress the flares (banding would be noticeable).
            lightingTextures[LST_DYNAMIC].tex =
                GL_LoadGraphics3("dLight", LGM_WHITE_ALPHA, DGL_LINEAR, DGL_LINEAR,
                                 -1 /*best anisotropy*/, DGL_CLAMP, DGL_CLAMP, TXCF_NO_COMPRESSION);
        }
        // Global tex variables not set! (scalable texture)
        return lightingTextures[LST_DYNAMIC].tex;

    case LST_GRADIENT:
        if(!lightingTextures[LST_GRADIENT].tex)
        {
            lightingTextures[LST_GRADIENT].tex =
                GL_LoadGraphics3("wallglow", LGM_WHITE_ALPHA, DGL_LINEAR,
                                 DGL_LINEAR, -1 /*best anisotropy*/,
                                 DGL_REPEAT, DGL_CLAMP, 0);
        }
        // Global tex variables not set! (scalable texture)
        return lightingTextures[LST_GRADIENT].tex;

    case LST_RADIO_CO:          // closed/open
    case LST_RADIO_CC:          // closed/closed
    case LST_RADIO_OO:          // open/open
    case LST_RADIO_OE:          // open/edge
        // FakeRadio corner shadows.
        if(!lightingTextures[which].tex)
        {
            switch(which)
            {
            case LST_RADIO_CO:
                lightingTextures[which].tex =
                    GL_LoadGraphics3("radioCO", LGM_WHITE_ALPHA, DGL_LINEAR,
                                     DGL_LINEAR, -1 /*best anisotropy*/,
                                     DGL_CLAMP, DGL_CLAMP, TXCF_NO_COMPRESSION);
                break;

            case LST_RADIO_CC:
                lightingTextures[which].tex =
                    GL_LoadGraphics3("radioCC", LGM_WHITE_ALPHA, DGL_LINEAR,
                                     DGL_LINEAR, -1 /*best anisotropy*/,
                                     DGL_CLAMP, DGL_CLAMP, TXCF_NO_COMPRESSION);
                break;

            case LST_RADIO_OO:
                lightingTextures[which].tex =
                    GL_LoadGraphics3("radioOO", LGM_WHITE_ALPHA, DGL_LINEAR,
                                     DGL_LINEAR, -1 /*best anisotropy*/,
                                     DGL_CLAMP, DGL_CLAMP, TXCF_NO_COMPRESSION);
                break;

            case LST_RADIO_OE:
                lightingTextures[which].tex =
                    GL_LoadGraphics3("radioOE", LGM_WHITE_ALPHA, DGL_LINEAR,
                                     DGL_LINEAR, -1 /*best anisotropy*/,
                                     DGL_CLAMP, DGL_CLAMP, TXCF_NO_COMPRESSION);
                break;

            default:
                break;
            }
        }
        // Global tex variables not set! (scalable texture)
        return lightingTextures[which].tex;

    default:
        // Failed to prepare anything.
        return 0;
    }
}

DGLuint GL_PrepareFlareTexture(flaretexid_t flare)
{
    // There are three flare textures.
    if(flare >= NUM_FLARE_TEXTURES)
        return 0;

    if(!flareTextures[flare].tex)
    {
        // We don't want to compress the flares (banding would be noticeable).
        flareTextures[flare].tex =
            GL_LoadGraphics3(flare == 0 ? "flare" : flare == 1 ? "brflare" :
                             "bigflare", LGM_WHITE_ALPHA,
                             DGL_NEAREST, DGL_LINEAR, 0 /*no anisotropy*/,
                             DGL_CLAMP, DGL_CLAMP,
                             TXCF_NO_COMPRESSION);

        if(flareTextures[flare].tex == 0)
        {
            Con_Error("GL_PrepareFlareTexture: flare texture %i not found!\n",
                      flare);
        }
    }

    return flareTextures[flare].tex;
}

static void setTextureMinMode(DGLuint tex, int minMode)
{
    DGL_Bind(tex);
    DGL_TexFilter(DGL_MIN_FILTER, minMode);
}

void GL_SetRawTextureParams(int minMode)
{
    rawtex_t**          rawTexs, **ptr;

    rawTexs = R_CollectRawTexs(NULL);
    for(ptr = rawTexs; *ptr; ptr++)
    {
        rawtex_t*       r = (*ptr);

        if(r->tex) // Is the texture loaded?
            setTextureMinMode(r->tex, minMode);

        if(r->tex2) // Is the texture loaded?
            setTextureMinMode(r->tex2, minMode);
    }
    Z_Free(rawTexs);
}

/**
 * Updates the textures, flats and sprites (gameTex) or the user
 * interface textures (patches and raw screens).
 */
void GL_SetTextureParams(int minMode, int gameTex, int uiTex)
{
    int                 i;

    if(gameTex)
    {
        R_SetMaterialMinMode(minMode);

        // Translated sprites.
        for(i = 0; i < numTransSprites; ++i)
            setTextureMinMode(transSprites[i]->tex, minMode);
    }

    if(uiTex)
    {
        // Patch textures.
        {
        patchtex_t **patches, **ptr;
        patches = R_CollectPatchTexs(NULL);
        for(ptr = patches; *ptr; ptr++)
        {
            patchtex_t*       p = (*ptr);

            if(p->tex) // Is the texture loaded?
                setTextureMinMode(p->tex, minMode);

            if(p->tex2) // Is the texture loaded?
                setTextureMinMode(p->tex2, minMode);
        }
        Z_Free(patches);
        }

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
 * @param   unused      Unused. Must be signature compatible.
 */
void GL_DoUpdateTexParams(cvar_t *unused)
{
    GL_SetTextureParams(glmode[mipmapping], true, true);
}

void GL_TexReset(void)
{
    GL_ClearTextureMemory();
    GL_LoadSystemTextures(true, true);
    Con_Printf("All DGL textures deleted.\n");
}

/**
 * Called when changing the value of the texture gamma cvar.
 */
void GL_DoUpdateTexGamma(cvar_t *unused)
{
    if(texInited)
    {
        GL_TexReset();
        LoadPalette();
    }

    Con_Printf("Gamma correction set to %f.\n", texGamma);
}

/**
 * Called when changing the value of any cvar affecting texture quality which
 * in turn calls GL_TexReset. Added to remove the need for reseting  manually.
 *
 * @param   unused      Unused. Must be signature compatible.
 */
void GL_DoTexReset(cvar_t *unused)
{
    GL_TexReset();
}

void GL_LowRes(void)
{
    // Set everything as low as they go.
    filterSprites = 0;
    linearRaw = 0;
    texMagMode = 0;

    // And do a texreset so everything is updated.
    GL_SetTextureParams(DGL_NEAREST, true, true);
    GL_TexReset();
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
        rawtex_t*         r = (*ptr);

        if(r->tex)
        {
            DGL_DeleteTextures(1, &r->tex);
            r->tex = 0;
        }

        if(r->tex2)
        {
            DGL_DeleteTextures(1, &r->tex2);
            r->tex2 = 0;
        }
    }
    Z_Free(rawTexs);
}

void GL_TextureFilterMode(int target, int parm)
{
    if(target == DD_TEXTURES)
        GL_UpdateTexParams(parm);
}

unsigned int GL_PrepareSkin(skintex_t *st, boolean allowTexComp)
{
    if(!st)
        return 0;

    // If the texture has already been loaded, we don't need to
    // do anything.
    if(!st->tex)
    {
        image_t             image;

        // Load the texture.
        if(!GL_LoadImage(&image, st->path, true))
            Con_Error("GL_PrepareSkin: %s not found.\n", st->path);

        st->tex =
            GL_UploadTexture(image.pixels, image.width, image.height,
                             image.pixelSize == 4, true, true, false, false,
                             glmode[mipmapping], DGL_LINEAR, texAniso,
                             DGL_REPEAT, DGL_REPEAT,
                             (!allowTexComp? TXCF_NO_COMPRESSION : 0));

        // We don't need the image data any more.
        M_Free(image.pixels);
    }

    return st->tex;
}

unsigned int GL_PrepareShinySkin(skintex_t *stp)
{
    image_t             image;

    if(!stp)
        return 0; // Does not have a shiny skin.

    if(!stp->tex)
    {
        // Load in the texture.
        if(!GL_LoadImageCK(&image, stp->path, true))
        {
            Con_Error("GL_PrepareShinySkin: Failed to load '%s'.\n",
                      stp->path);
        }

        stp->tex =
            GL_UploadTexture(image.pixels, image.width, image.height,
                             image.pixelSize == 4, true, true, false, false,
                             glmode[mipmapping], texAniso,
                             DGL_LINEAR, DGL_REPEAT, DGL_CLAMP, 0);

        // We don't need the image data any more.
        GL_DestroyImage(&image);
    }

    return stp->tex;
}

/**
 * @return              The rawtex name, if it has been prepared.
 */
DGLuint GL_GetRawTexInfo(lumpnum_t lump, boolean part2)
{
    rawtex_t           *rawtex = R_GetRawTex(lump);

    if(!rawtex)
        return 0;

    return (part2? rawtex->tex2 : rawtex->tex);
}

/**
 * Calculates texture coordinates based on the given dimensions. The
 * coordinates are calculated as width/CeilPow2(width), or 1 if the
 * CeilPow2 would go over glMaxTexSize.
 */
static void GL_SetTexCoords(float *tc, int wid, int hgt)
{
    int                 pw = M_CeilPow2(wid), ph = M_CeilPow2(hgt);

    if(pw > glMaxTexSize || ph > glMaxTexSize)
        tc[VX] = tc[VY] = 1;
    else
    {
        tc[VX] = wid / (float) pw;
        tc[VY] = hgt / (float) ph;
    }
}

/**
 * Returns true if the given path name refers to an image, which should
 * be color keyed. Color keying is done for both (0,255,255) and
 * (255,0,255).
 */
boolean GL_IsColorKeyed(const char *path)
{
    char                buf[256];

    strcpy(buf, path);
    strlwr(buf);
    return strstr(buf, "-ck.") != NULL;
}

DGLuint GL_PrepareMaterial2(struct material_s* mat)
{
    DGLuint             glTexName = 0;

    if(mat)
    {
        byte                result = 0;

        switch(mat->type)
        {
        case MAT_FLAT:
            glTexName = prepareFlat2(mat, &result);

            if(result)
            {   // We need to update the assocated enhancements.
                ded_decor_t        *def =
                    Def_GetDecoration(mat->ofTypeID, false, result == 2);

                mat->flags &= ~MATF_GLOW;
                if(def)
                {
                    mat->decoration = def;

                    // A glowing flat?
                    if(def->glow)
                        mat->flags |= MATF_GLOW;
                }

                // Get the surface reflection for this.
                mat->reflection = Def_GetReflection(mat->ofTypeID, false);

                // Get the particle generator definition for this.
                {
                ded_ptcgen_t       *def;
                int                 i, g;
                boolean             found = false;

                // The generator will be determined now.
                for(i = 0, def = defs.ptcGens; i < defs.count.ptcGens.num; ++i, def++)
                {
                    if(def->flags & PGF_GROUP)
                    {
                        // This generator is triggered by all the materials in
                        // the animation group.
                        material_t         *defMat = R_GetMaterial(def->surfaceIndex, (def->isTexture? MAT_TEXTURE : MAT_FLAT));

                        // We only need to search if we know both the real used flat
                        // and the flat of this definition belong in an animgroup.
                        if(defMat->inGroup && mat->inGroup)
                        {
                            for(g = 0; g < numgroups; ++g)
                            {
                                // Precache groups don't apply.
                                if(groups[g].flags & AGF_PRECACHE)
                                    continue;

                                if(R_IsInAnimGroup(groups[g].id, (def->isTexture? MAT_TEXTURE : MAT_FLAT), def->surfaceIndex) &&
                                    R_IsInAnimGroup(groups[g].id, mat->type, mat->ofTypeID))
                                {
                                    // Both are in this group! This def will do.
                                    mat->ptcGen = def;
                                    found = true;
                                }
                            }
                        }
                    }

                    if(mat->type == (def->isTexture? MAT_TEXTURE : MAT_FLAT) &&
                       def->surfaceIndex == mat->ofTypeID)
                    {
                        mat->ptcGen = def;
                        found = true;
                    }
                }

                if(!found)
                    mat->ptcGen = NULL;
                }
            }
            break;

        case MAT_TEXTURE:
            glTexName = prepareTexture2(mat, &result);

            if(result)
            {   // We need to update the associated enhancements.
                ded_decor_t        *def =
                    Def_GetDecoration(mat->ofTypeID, true, result == 2);

                mat->flags &= ~MATF_GLOW;
                if(def)
                {
                    mat->decoration = def;

                    // A glowing texture?
                    if(def->glow)
                        def->flags |= MATF_GLOW;
                }

                // Get the reflection for this surface.
                mat->reflection = Def_GetReflection(mat->ofTypeID, true);
            }
            break;

        case MAT_SPRITE:
            glTexName = prepareSprite(mat);
            break;

        case MAT_DDTEX:
            glTexName = prepareDDTexture(mat);
            break;
        }
    }

    return glTexName;
}

DGLuint GL_PrepareMaterial(struct material_s* mat)
{
    return GL_PrepareMaterial2(mat->current);
}

void GL_SetMaterial(int idx, materialtype_t type)
{
    material_t*         mat = R_GetMaterial(idx, type);

    if(mat)
    {
        int                 magMode;

        switch(mat->type)
        {
        case MAT_SPRITE:
            magMode = (filterSprites ? DGL_LINEAR : DGL_NEAREST);
            break;

        default:
            magMode = glmode[texMagMode];
            break;
        }

        GL_BindTexture(GL_PrepareMaterial(mat), magMode);
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
            TranslatePatch(patch, redToWhite);
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

D_CMD(SmoothRaw)
{
    linearRaw = strtol(argv[1], NULL, 0) ? DGL_LINEAR : DGL_NEAREST;
    return true;
}
