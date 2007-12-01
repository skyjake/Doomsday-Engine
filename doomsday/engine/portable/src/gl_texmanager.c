/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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

#include "def_main.h"
#include "ui_main.h"

// MACROS ------------------------------------------------------------------

#define PALLUMPNAME     "PLAYPAL"

// TYPES -------------------------------------------------------------------

// A translated sprite.
typedef struct {
    int     patch;
    DGLuint tex;
    unsigned char *table;
} transspr_t;

// Model skin.
typedef struct {
    char    path[256];
    DGLuint tex;
} skintex_t;

// Detail texture instance.
typedef struct dtexinst_s {
    struct dtexinst_s *next;
    int     lump;
    float   contrast;
    DGLuint tex;
    const char *external;
} dtexinst_t;

// Sky texture topline colors.
typedef struct {
    int     texidx;
    float   rgb[3];
} skycol_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(LowRes);
D_CMD(ResetTextures);
D_CMD(MipMap);
D_CMD(SmoothRaw);

byte   *GL_LoadHighResFlat(image_t *img, char *name);
byte   *GL_LoadHighResPatch(image_t *img, char *name);
void    GL_DeleteDetailTexture(detailtex_t *dtex);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void LoadPalette(void);
static void GL_SetTexCoords(float *tc, int wid, int hgt);
static DGLuint getFlatInfo2(int index, boolean translate,
                            texinfo_t **texinfo);
static DGLuint getTextureInfo2(int index, boolean translate,
                               texinfo_t **texinfo);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int ratioLimit;
extern boolean palettedTextureExtAvailable;
extern boolean s3tcAvailable;

extern material_t skyMaskMaterial;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The current texture.
DGLuint curtex = 0;

int     glMaxTexSize;           // Maximum supported texture size.
int     ratioLimit = 0;         // Zero if none.
boolean filloutlines = true;
byte    loadExtAlways = false;  // Always check for extres (cvar)
byte    paletted = false;       // Use GL_EXT_paletted_texture (cvar)
boolean load8bit = false;       // Load textures as 8 bit? (w/paltex)
int     monochrome = 0;         // desaturate a patch (average colours)
int     upscaleAndSharpenPatches = 0;
int     useSmartFilter = 0;     // Smart filter mode (cvar: 1=hq2x)
int     mipmapping = 3, linearRaw = 1, texQuality = TEXQ_BEST;
int     filterSprites = true;
int     texMagMode = 1;         // Linear.
int     texAniso = -1;          // Use best.

float   texGamma = 0;
byte    gammatable[256];

// Convert a 18-bit RGB (666) value to a playpal index.
// \fixme 256kb - Too big?
byte    pal18to8[262144];
int     pallump;

// Names of the dynamic light textures.
ddtexture_t lightingTextures[NUM_LIGHTING_TEXTURES];

// Names of the "built-in" Doomsday textures.
ddtexture_t ddTextures[NUM_DD_TEXTURES];

// Names of the flare textures (halos).
ddtexture_t flareTextures[NUM_FLARE_TEXTURES];

skycol_t *skytop_colors = NULL;
int     num_skytop_colors = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean texInited = false;   // Init done.
static boolean allowMaskedTexEnlarge = false;
static boolean noHighResTex = false;
static boolean noHighResPatches = false;
static boolean highResWithPWAD = false;

// Skinnames will only *grow*. It will never be destroyed, not even
// at resets. The skin textures themselves will be deleted, though.
// This is because we want to have permanent ID numbers for skins,
// and the ID numbers are the same as indices to the skinnames array.
// Created in r_model.c, when registering the skins.
static int numskinnames;
static skintex_t *skinnames;

// Linked list of detail texture instances. A unique texture is generated
// for each (rounded) contrast level.
static dtexinst_t *dtinstances;

// The translated sprites.
static transspr_t **transsprites;
static int numtranssprites;

//static boolean    gammaSupport = false;

static int glmode[6] =          // Indexed by 'mipmapping'.
{
    DGL_NEAREST,
    DGL_LINEAR,
    DGL_NEAREST_MIPMAP_NEAREST,
    DGL_LINEAR_MIPMAP_NEAREST,
    DGL_NEAREST_MIPMAP_LINEAR,
    DGL_LINEAR_MIPMAP_LINEAR
};

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
    C_VAR_INT2("rend-tex-filter-sprite", &filterSprites, 0, 0, 1,
               GL_DoTexReset);
    C_VAR_INT2("rend-tex-filter-raw", &linearRaw, CVF_PROTECTED, 0, 1,
               GL_DoTexReset);
    C_VAR_INT2("rend-tex-filter-smart", &useSmartFilter, 0, 0, 1,
               GL_DoTexReset);
    C_VAR_INT2("rend-tex-filter-mag", &texMagMode, 0, 0, 1, GL_DoTexReset);
    C_VAR_INT2("rend-tex-filter-anisotropic", &texAniso, 0, -1, 4,
               GL_DoUpdateTexParams);
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
    int     i;

    // Initialize the smart texture filtering routines.
    GL_InitSmartFilter();

    // The palette lump, for color information (really??!!?!?!).
    pallump = W_GetNumForName(PALLUMPNAME);

    // Load the pal18to8 table from the lump PAL18TO8. We need it
    // when resizing textures.
    if((i = W_CheckNumForName("PAL18TO8")) == -1)
        CalculatePal18to8(pal18to8, GL_GetPalette());
    else
        memcpy(pal18to8, W_CacheLumpNum(i, PU_CACHE), sizeof(pal18to8));
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

    numtranssprites = 0;

    // Raw screen lump book-keeping.
    rawtextures = 0;
    numrawtextures = 0;

    // Do we need to generate a pal18to8 table?
    if(ArgCheck("-dump_pal18to8"))
    {
        CalculatePal18to8(pal18to8, GL_GetPalette());

        {
            FILE   *file = fopen("pal18to8.lmp", "wb");

            fwrite(pal18to8, sizeof(pal18to8), 1, file);
            fclose(file);
        }
    }

    GL_InitPalettedTexture();

    // DGL needs the palette information regardless of whether the
    // paletted textures are enabled or not.
    LoadPalette();

    // Detail textures.
    dtinstances = NULL;

    // System textures loaded in GL_LoadSystemTextures.
    memset(flareTextures, 0, sizeof(flareTextures));
    memset(lightingTextures, 0, sizeof(lightingTextures));
    memset(ddTextures, 0, sizeof(ddTextures));

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

    // Destroy all bookkeeping -- into the shredder, I say!!
    M_Free(skytop_colors);
    skytop_colors = 0;
    num_skytop_colors = 0;

    texInited = false;
}

/**
 * This is called at final shutdown.
 */
void GL_DestroySkinNames(void)
{
    M_Free(skinnames);
    skinnames = 0;
    numskinnames = 0;
}

static void LoadPalette(void)
{
    int     i, c;
    byte   *playpal;
    byte    paldata[256 * 3];
	double	invgamma;

    pallump = W_GetNumForName(PALLUMPNAME);
    playpal = GL_GetPalette();

    // Clamp to a sane range.
	invgamma = 1.0f - MINMAX_OF(0, texGamma, 1);
	for(i = 0; i < 256; ++i)
		gammatable[i] = (byte)(255.0f * pow(i / 255.0f, invgamma));

    // Prepare the color table.
    for(i = 0; i < 256; ++i)
    {
        // Adjust the values for the appropriate gamma level.
        for(c = 0; c < 3; ++c)
            paldata[i * 3 + c] = gammatable[playpal[i * 3 + c]];
    }
    gl.Palette(DGL_RGB, paldata);
}

byte *GL_GetPalette(void)
{
    return W_CacheLumpNum(pallump, PU_CACHE);
}

byte *GL_GetPal18to8(void)
{
    return pal18to8;
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

    gl.Enable(DGL_PALETTED_TEXTURES);

    // Check if the operation was a success.
    if(gl.GetInteger(DGL_PALETTED_TEXTURES) == DGL_FALSE)
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
    image_t image;
    filename_t resource;

    if(map->tex)
        return;                 // Already loaded.

    // Default texture name.
    map->tex = lightingTextures[LST_DYNAMIC].tex;

    if(!strcmp(map->id, "-"))
    {
        // No lightmap, if we don't know where to find the map.
        map->tex = 0;
    }
    else if(map->id[0])         // Not an empty string.
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

void GL_DeleteLightMap(ded_lightmap_t *map)
{
    if(map->tex != lightingTextures[LST_DYNAMIC].tex)
    {
        gl.DeleteTextures(1, &map->tex);
    }
    map->tex = 0;
}

/**
 * Flaremaps are normally monochrome images but we'll allow full color.
 */
void GL_LoadFlareMap(ded_flaremap_t *map, int oldidx)
{
    image_t image;
    filename_t resource;
    boolean loaded = false;

    if(map->tex)
        return;                 // Already loaded.

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
    else if(map->id[0])         // Not an empty string.
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
        char   *end;
        int     id = 0, pass;
        boolean ok;

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
                    map->tex = (id? GL_PrepareLSTexture(LST_DYNAMIC, NULL) : 0);
                    map->custom = false;
                    map->disabled = false;
                    loaded = true;
                }
                else
                {
                    id -= 2;
                    if(id >= 0 && id < NUM_FLARE_TEXTURES)
                    {
                        map->tex = GL_PrepareFlareTexture(id, NULL);
                        map->custom = false;
                        map->disabled = false;
                        loaded = true;
                    }
                }
            }
        }
    }
}

void GL_DeleteFlareMap(ded_flaremap_t *map)
{
    if(map->tex != flareTextures[FXT_FLARE].tex)
    {
        gl.DeleteTextures(1, &map->tex);
    }
    map->tex = 0;
}

/**
 * Loads both the shiny texture and the mask.  Returns true if there is
 * a reflection map to can be used.
 */
boolean GL_LoadReflectionMap(ded_reflection_t *loading_ref)
{
    ded_reflection_t *ref;

    if(loading_ref == NULL)
    {
        return false;
    }

    // First try the shiny texture map.
    ref = loading_ref->use_shiny;
    if(!ref)
    {
        // Not shiny at all.
        return false;
    }

    if(ref->shiny_tex == 0)
    {
        // Need to load the shiny texture.
        ref->shiny_tex = GL_LoadGraphics2(RC_LIGHTMAP, ref->shiny_map.path,
                                          LGM_NORMAL, DGL_FALSE, true, 0);
        if(ref->shiny_tex == 0)
        {
            VERBOSE(Con_Printf("GL_LoadReflectionMap: %s not found!\n",
                               ref->shiny_map.path));
        }
    }

    // Also load the mask, if one has been specified.
    if(loading_ref->use_mask)
    {
        ref = loading_ref->use_mask;

        if(ref->mask_tex == 0)
        {
            ref->mask_tex = GL_LoadGraphics2(RC_LIGHTMAP,
                                             ref->mask_map.path,
                                             LGM_NORMAL, DGL_TRUE, true, 0);
            if(ref->mask_tex == 0)
            {
                VERBOSE(Con_Printf("GL_LoadReflectionMap: %s not found!\n",
                                   ref->mask_map.path));
            }
        }
    }

    return true;
}

void GL_DeleteReflectionMap(ded_reflection_t *ref)
{
    if(ref->shiny_tex)
    {
        gl.DeleteTextures(1, &ref->shiny_tex);
        ref->shiny_tex = 0;
    }

    if(ref->mask_tex)
    {
        gl.DeleteTextures(1, &ref->mask_tex);
        ref->mask_tex = 0;
    }
}

/**
 * Called from GL_LoadSystemTextures.
 */
void GL_LoadDDTextures(void)
{
    GL_PrepareMaterial(DDT_UNKNOWN, MAT_DDTEX, NULL);
    GL_PrepareMaterial(DDT_MISSING, MAT_DDTEX, NULL);
    GL_PrepareMaterial(DDT_BBOX, MAT_DDTEX, NULL);
    GL_PrepareMaterial(DDT_GRAY, MAT_DDTEX, NULL);
}

void GL_ClearDDTextures(void)
{
    uint        i;

    for(i = 0; i < NUM_DD_TEXTURES; ++i)
        gl.DeleteTextures(1, &ddTextures[i].tex);
    memset(ddTextures, 0, sizeof(ddTextures));
}

/**
 * Prepares all the system textures (dlight, ptcgens).
 */
void GL_LoadSystemTextures(boolean loadLightMaps, boolean loadFlares)
{
    int     i, k;
    ded_decor_t *decor;

    if(!texInited)
        return;

    GL_LoadDDTextures(); // missing etc
    UI_LoadTextures();

    // Preload lighting system textures.
    GL_PrepareLSTexture(LST_DYNAMIC, NULL);
    GL_PrepareLSTexture(LST_GRADIENT, NULL);

    // Preload flares
    GL_PrepareFlareTexture(FXT_FLARE, NULL);
    if(!haloRealistic)
    {
        GL_PrepareFlareTexture(FXT_BRFLARE, NULL);
        GL_PrepareFlareTexture(FXT_BIGFLARE, NULL);
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
                                    decor->lights[k].flare_texture);

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
    int     i, k;
    ded_decor_t *decor;

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
        gl.DeleteTextures(1, &lightingTextures[i].tex);
    memset(lightingTextures, 0, sizeof(lightingTextures));

    for(i = 0; i < NUM_FLARE_TEXTURES; ++i)
        gl.DeleteTextures(1, &flareTextures[i].tex);
    memset(flareTextures, 0, sizeof(flareTextures));

    GL_ClearDDTextures();
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
    dtexinst_t *dtex;
    int     i;

    if(!texInited)
        return;

    // The rendering lists contain persistent references to texture names.
    // Which, obviously, can't persist any longer...
    RL_DeleteLists();

    // Textures, flats and sprite lumps.
    for(i = 0; i < numtextures; ++i)
        R_DeleteMaterial(i, MAT_TEXTURE);
    for(i = 0; i < numflats; ++i)
        R_DeleteMaterial(i, MAT_FLAT);
    for(i = 0; i < numSpriteLumps; ++i)
        GL_DeleteSprite(i);

    // The translated sprite textures.
    for(i = 0; i < numtranssprites; ++i)
    {
        gl.DeleteTextures(1, &transsprites[i]->tex);
        transsprites[i]->tex = 0;
    }
    numtranssprites = 0;

    // Delete skins.
    for(i = 0; i < numskinnames; ++i)
    {
        gl.DeleteTextures(1, &skinnames[i].tex);
        skinnames[i].tex = 0;
    }

    // Delete detail textures.
    i = 0;
    while(dtinstances)
    {
        dtex = dtinstances->next;
        gl.DeleteTextures(1, &dtinstances->tex);
        M_Free(dtinstances);
        dtinstances = dtex;
        i++;
    }
    VERBOSE(Con_Message
            ("GL_ClearRuntimeTextures: %i detail texture " "instances.\n", i));
    for(i = 0; i < defs.count.details.num; ++i)
        details[i].gltex = 0;

    // Surface reflection textures and masks.
    for(i = 0; i < defs.count.reflections.num; ++i)
    {
        GL_DeleteReflectionMap(&defs.reflections[i]);
    }

    {
    patch_t **patches, **ptr;
    patches = R_CollectPatches(NULL);
    for(ptr = patches; *ptr; ptr++)
    {
        if((*ptr)->tex)
        {
            gl.DeleteTextures(1, &(*ptr)->tex);
            (*ptr)->tex = 0;
        }
        if((*ptr)->tex2)
        {
            gl.DeleteTextures(1, &(*ptr)->tex2);
            (*ptr)->tex2 = 0;
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
 * Binds the texture if necessary.
 */
void GL_BindTexture(DGLuint texname)
{
    if(Con_IsBusy())
        return;

    /*if(curtex != texname)
       { */
    gl.Bind(texname);
    curtex = texname;
    //}
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
    noStretch =
        GL_OptimalSize(width, height, &levelWidth, &levelHeight, noStretch);

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
                           noStretch);

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
    gl.Bind(content->name);

    if(load8bit)
    {
        int     canGenMips;

        gl.GetIntegerv(DGL_PALETTED_GENMIPS, &canGenMips);

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
            if(gl.TexImage(alphaChannel ? DGL_COLOR_INDEX_8_PLUS_A8 :
                           DGL_COLOR_INDEX_8, levelWidth, levelHeight,
                           generateMipmaps &&
                           canGenMips ? DGL_TRUE : generateMipmaps ? -i :
                           DGL_FALSE, idxBuffer) != DGL_OK)
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
        if(gl.TexImage(alphaChannel ? DGL_RGBA : DGL_RGB, levelWidth, levelHeight,
                       generateMipmaps ? DGL_TRUE : DGL_FALSE, buffer) != DGL_OK)
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

    for(i = dtinstances; i; i = i->next)
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
    i->next = dtinstances;
    dtinstances = i;
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
            if(lumpinfo[num].size != 256 * 256)
            {
                if(lumpinfo[num].size != 128 * 128)
                {
                    if(lumpinfo[num].size != 64 * 64)
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
    int     i;
    detailtex_t *dt;
    ded_detailtexture_t *def;

    // Search through the assignments.
    for(i = defs.count.details.num - 1; i >= 0; i--)
    {
        def = defs.details + i;
        dt = &details[i];

        // Is there a detail texture assigned for this?
        if(dt->detail_lump < 0 && !def->is_external)
            continue;

        if((is_wall_texture && index == dt->wall_texture) ||
           (!is_wall_texture && index == dt->flat_texture))
        {
            if(dtdef)
            {
                *dtdef = def;
            }

            // Hey, a match. Load this?
            if(!dt->gltex)
            {
                dt->gltex =
                    GL_LoadDetailTexture(dt->detail_lump, def->strength,
                                         (def->is_external?
                                          def->detail_lump.path : NULL));
            }
            return dt->gltex;
        }
    }
    return 0;                   // There is no detail texture for this.
}

/**
 * @return              The OpenGL name of the texture.
 */
static unsigned int prepareFlat2(int idx, boolean translate,
                                 texinfo_t **info)
{
    byte   *flatptr;
    int     originalIndex = idx;
    int     width, height, pixSize = 3;
    boolean RGBData = false, freeptr = false;
    ded_detailtexture_t *def;
    ded_decor_t *dec;
    image_t image;
    boolean hasExternal = false;
    flat_t *flat;

    if(idx < 0 || idx >= numflats) // No texture?
        return 0;

    if(translate)
    {
        idx = flattranslation[idx].current;
    }

    if(idx < 0 || idx >= numflats) // No texture?
        return 0;

    flat = flats[idx];
    if(!flat->tex)
    {
        // Try to load a high resolution version of this flat.
        if((loadExtAlways || highResWithPWAD ||
            !R_IsCustomMaterial(idx, MAT_FLAT)) &&
           (flatptr = GL_LoadHighResFlat(&image, lumpinfo[flat->lump].name)) != NULL)
        {
            RGBData = true;
            freeptr = true;
            width = image.width;
            height = image.height;
            pixSize = image.pixelSize;

            hasExternal = true;
        }
        else
        {
            if(lumpinfo[flat->lump].size < 4096)
                return 0;           // Too small.
            // Get a pointer to the texture data.
            flatptr = W_CacheLumpNum(flat->lump, PU_CACHE);
            width = height = 64;
        }
        flat->info.width = flat->info.height = 64;
        flat->info.masked = false;

        // Is there a detail texture for this?
        if((flat->info.detail.tex = GL_PrepareDetailTexture(idx, false, &def)))
        {
            // The width and height could be used for something meaningful.
            flat->info.detail.width = 128;
            flat->info.detail.height = 128;
            flat->info.detail.scale = def->scale;
            flat->info.detail.strength = def->strength;
            flat->info.detail.maxdist = def->maxdist;
        }

        // Load the texture.
        flat->tex = GL_UploadTexture(flatptr, width, height,
                                pixSize == 4, true, RGBData, false, false,
                                glmode[mipmapping], glmode[texMagMode],
                                texAniso,
                                DGL_REPEAT, DGL_REPEAT, 0);

        // Average color for glow planes.
        if(RGBData)
        {
            averageColorRGB(flat->color, flatptr, width, height);
        }
        else
        {
            averageColorIdx(flat->color, flatptr, width, height,
                            GL_GetPalette(), false);
        }

        if(freeptr)
            M_Free(flatptr);

        // Is there a surface decoration for this flat?
        dec = Def_GetDecoration(idx, false, hasExternal);
        if(dec)
        {
            // A glowing flat?
            if(dec->glow)
                flat->flags |= TXF_GLOW;
            else
                flat->flags &= ~TXF_GLOW;

            flat->decoration = dec;
        }

        // Get the surface reflection for this flat.
        flat->reflection = Def_GetReflection(idx, false);
    }
    return getFlatInfo2(originalIndex, translate, info);
}

/**
 * Prepares one of the "Doomsday Textures" 'which' must be one
 * of the DDT_* constants.
 */
static DGLuint prepareDDTexture(ddtextureid_t which, texinfo_t **texinfo)
{
    static const char *ddTexNames[NUM_DD_TEXTURES] = {
        "unknown",
        "missing",
        "bbox",
        "gray"
    };

    if(which < NUM_DD_TEXTURES)
    {
        if(!ddTextures[which].tex)
        {
            ddTextures[which].tex =
                GL_LoadGraphics2(RC_GRAPHICS, ddTexNames[which], LGM_NORMAL,
                                 DGL_TRUE, false, 0);

            if(!ddTextures[which].tex)
                Con_Error("prepareDDTexture: \"%s\" not found!\n",
                          ddTexNames[which]);

            ddTextures[which].info.width =
                ddTextures[which].info.height = 64;
        }
    }
    else
        Con_Error("prepareDDTexture: Invalid ddtexture %i\n", which);

    if(texinfo)
        *texinfo = &ddTextures[which].info;

    return ddTextures[which].tex;
}

/**
 * Loads PCX, TGA and PNG images. The returned buffer must be freed
 * with M_Free. Color keying is done if "-ck." is found in the filename.
 * The allocated memory buffer always has enough space for 4-component
 * colors.
 */
byte *GL_LoadImage(image_t *img, const char *imagefn, boolean useModelPath)
{
    DFILE  *file;
    char    ext[40];
    int     format;

    // Clear any old values.
    memset(img, 0, sizeof(*img));

    if(useModelPath)
    {
        if(!R_FindModelFile(imagefn, img->fileName))
            return NULL;        // Not found.
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
            ("LoadImage: %s (%ix%i)\n", M_Pretty(img->fileName), img->width,
             img->height));

    // How about some color-keying?
    if(GL_IsColorKeyed(img->fileName))
    {
        byte *out;

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
    char    keyFileName[256];
    byte   *pixels;
    char   *ptr;

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
byte *GL_LoadHighRes(image_t *img, char *name, char *prefix,
                     boolean allowColorKey, resourceclass_t resClass)
{
    filename_t resource, fileName;

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
byte *GL_LoadTexture(image_t * img, char *name)
{
    return GL_LoadHighRes(img, name, "", true, RC_TEXTURE);
}

/**
 * Use this when loading high-res wall textures.
 * The returned buffer must be freed with M_Free.
 */
byte *GL_LoadHighResTexture(image_t *img, char *name)
{
    if(noHighResTex)
        return NULL;
    return GL_LoadTexture(img, name);
}

/**
 * The returned buffer must be freed with M_Free.
 */
byte *GL_LoadHighResFlat(image_t *img, char *name)
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
    return GL_LoadGraphics2(RC_GRAPHICS, name, mode, DGL_FALSE, true, 0);
}

DGLuint GL_LoadGraphics2(resourceclass_t resClass, const char *name,
                         gfxmode_t mode, int useMipmap, boolean clamped,
                         int otherFlags)
{
    return GL_LoadGraphics4(resClass, name, mode, useMipmap,
                            DGL_LINEAR, glmode[texMagMode], 0 /*no anisotropy*/,
                            clamped? DGL_CLAMP : DGL_REPEAT,
                            clamped? DGL_CLAMP : DGL_REPEAT, otherFlags);
}

DGLuint GL_LoadGraphics3(const char *name, gfxmode_t mode,
                         int minFilter, int magFilter, int anisoFilter,
                         int wrapS, int wrapT, int otherFlags)
{
    return GL_LoadGraphics4(RC_GRAPHICS, name, mode, DGL_FALSE,
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
        texture = gl.NewTexture();
        if(image.width < 128 && image.height < 128)
        {
            // Small textures will never be compressed.
            gl.Disable(DGL_TEXTURE_COMPRESSION);
        }
        gl.TexImage(image.pixelSize ==
                    2 ? DGL_LUMINANCE_PLUS_A8 : image.pixelSize ==
                    3 ? DGL_RGB : image.pixelSize ==
                    4 ? DGL_RGBA : DGL_LUMINANCE, image.width, image.height,
                    useMipmap, image.pixels);
        gl.Enable(DGL_TEXTURE_COMPRESSION);
        gl.TexParameter(DGL_MAG_FILTER, glmode[texMagMode]);
        gl.TexParameter(DGL_MIN_FILTER,
                        useMipmap ? glmode[mipmapping] : DGL_LINEAR);
        if(clamped)
        {
            gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
            gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
        }*/

        texture = GL_NewTextureWithParams2(( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                                             image.pixelSize == 3 ? DGL_RGB :
                                             image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
                                           image.width, image.height, image.pixels,
                                           ( otherFlags | (useMipmap? TXCF_MIPMAP : 0) |
                                             (useMipmap == DGL_GRAY_MIPMAP? TXCF_GRAY_MIPMAP : 0) |
                                             (image.width < 128 && image.height < 128? TXCF_NO_COMPRESSION : 0) ),
                                           (useMipmap ? glmode[mipmapping] : DGL_LINEAR),
                                           glmode[texMagMode], texAniso,
                                           wrapS, wrapT);

        GL_DestroyImage(&image);
    }
    return texture;
}

/**
 * Renders the given texture into the buffer.
 */
boolean GL_BufferTexture(texture_t *tex, byte *buffer, int width, int height,
                         int *has_big_patch)
{
    int     i, len;
    boolean alphaChannel = false;
    lumppatch_t *patch;

    len = width * height;

    // Clear the buffer.
    memset(buffer, 0, 2 * len);

    // By default zero is put in the big patch height.
    if(has_big_patch)
        *has_big_patch = 0;

    // Draw all the patches. Check for alpha pixels after last patch has
    // been drawn.
    for(i = 0; i < tex->patchcount; ++i)
    {
        patch = (lumppatch_t*) W_CacheLumpNum(tex->patches[i].patch, PU_CACHE);

        // Check for big patches?
        if(SHORT(patch->height) > tex->info.height && has_big_patch &&
           *has_big_patch < SHORT(patch->height))
        {
            *has_big_patch = SHORT(patch->height);
        }
        // Draw the patch in the buffer.
        alphaChannel =
            DrawRealPatch(buffer, /*palette,*/ width, height, patch,
                          tex->patches[i].originx, tex->patches[i].originy,
                          false, 0, i == tex->patchcount - 1);
    }
    W_ChangeCacheTag(pallump, PU_CACHE);
    return alphaChannel;
}

/**
 * @return              The DGL texture name.
 */
static unsigned int prepareTexture2(int idx, boolean translate, texinfo_t **info)
{
    ded_detailtexture_t *def;
    ded_decor_t *dec;
    int     originalIndex = idx;
    texture_t *tex;
    boolean alphaChannel = false, RGBData = false;
    int     i;
    image_t image;
    boolean hasExternal = false;

    if(idx < 0 || idx >= numtextures) // No texture?
        return 0;

    if(translate)
    {
        idx = texturetranslation[idx].current;
    }

    if(idx < 0 || idx >= numtextures) // No texture?
        return 0;

    tex = textures[idx];
    if(!tex->tex)
    {
        // Try to load a high resolution version of this texture.
        if((loadExtAlways || highResWithPWAD ||
            !R_IsCustomMaterial(idx, MAT_TEXTURE)) &&
           GL_LoadHighResTexture(&image, tex->name) != NULL)
        {
            // High resolution texture loaded.
            RGBData = true;
            alphaChannel = (image.pixelSize == 4);
            hasExternal = true;
        }
        else
        {
            image.width = tex->info.width;
            image.height = tex->info.height;
            image.pixels = M_Malloc(2 * image.width * image.height);
            image.isMasked =
                GL_BufferTexture(tex, image.pixels, image.width, image.height,
                                 &i);

            // The -bigmtex option allows the engine to resize masked
            // textures whose patches are too big to fit the texture.
            if(allowMaskedTexEnlarge && image.isMasked && i)
            {
                // Adjust height to fit the largest patch.
                tex->info.height = image.height = i;
                // Get a new buffer.
                M_Free(image.pixels);
                image.pixels = M_Malloc(2 * image.width * image.height);
                image.isMasked =
                    GL_BufferTexture(tex, image.pixels, image.width,
                                     image.height, 0);
            }
            // "alphaChannel" and "masked" are the same thing for indexed
            // images.
            alphaChannel = image.isMasked;
        }

        // Load a detail texture (if one is defined).
        if((tex->info.detail.tex =
            GL_PrepareDetailTexture(idx, true, &def)))
        {
            // The width and height could be used for something meaningful.
            tex->info.detail.width = 128;
            tex->info.detail.height = 128;
            tex->info.detail.scale = def->scale;
            tex->info.detail.strength = def->strength;
            tex->info.detail.maxdist = def->maxdist;
        }

        tex->tex =
            GL_UploadTexture(image.pixels, image.width, image.height,
                             alphaChannel, true, RGBData, false, false,
                             glmode[mipmapping], glmode[texMagMode], texAniso,
                             DGL_REPEAT, DGL_REPEAT, 0);

        // Average color for glow planes.
        if(RGBData)
        {
            averageColorRGB(tex->color, image.pixels, image.width,
                            image.height);
        }
        else
        {
            averageColorIdx(tex->color, image.pixels, image.width,
                            image.height, GL_GetPalette(), false);
        }

        tex->info.masked = (image.isMasked != 0);

        GL_DestroyImage(&image);

        // Is there a decoration for this surface?
        dec = Def_GetDecoration(idx, true, hasExternal);
        if(dec)
        {
            // A glowing texture?
            if(dec->glow)
                tex->flags |= TXF_GLOW;
            else
                tex->flags &= ~TXF_GLOW;

            tex->decoration = dec;
        }

        // Get the reflection for this surface.
        tex->reflection = Def_GetReflection(idx, true);
    }
    return getTextureInfo2(originalIndex, translate, info);
}

/**
 * Draws the given sky texture in a buffer. The returned buffer must be
 * freed by the caller. Idx must be a valid texture number.
 */
void GL_BufferSkyTexture(int idx, byte **outbuffer, int *width, int *height,
                         boolean zeroMask)
{
    texture_t *tex = textures[idx];
    byte   *imgdata;
    int     i, numpels;

    *width = tex->info.width;
    *height = tex->info.height;

    if(tex->patchcount > 1)
    {
        numpels = tex->info.width * tex->info.height;
        imgdata = M_Calloc(2 * numpels);
        /*for(i = 0; i < tex->width; i++)
           {
           colptr = R_GetColumn(idx, i);
           for(k = 0; k < tex->height; k++)
           {
           if(!zeroMask)
           imgdata[k*tex->width + i] = colptr[k];
           else if(colptr[k])
           {
           byte *imgpos = imgdata + (k*tex->width + i);
           *imgpos = colptr[k];
           imgpos[numpels] = 0xff;  // Not transparent, this pixel.
           }
           }
           } */
        for(i = 0; i < tex->patchcount; ++i)
        {
            DrawRealPatch(imgdata, /*palette,*/ tex->info.width, tex->info.height,
                          W_CacheLumpNum(tex->patches[i].patch, PU_CACHE),
                          tex->patches[i].originx, tex->patches[i].originy,
                          zeroMask, 0, false);
        }
    }
    else
    {
        lumppatch_t *patch =
            (lumppatch_t*) W_CacheLumpNum(tex->patches[0].patch, PU_CACHE);
        int     bufHeight =
            SHORT(patch->height) > tex->info.height ? SHORT(patch->height)
            : tex->info.height;

        if(bufHeight > *height)
        {
            // Heretic sky textures are reported to be 128 tall, even if the
            // data is 200. We'll adjust the real height of the texture up to
            // 200 pixels (remember Caldera?).
            *height = bufHeight;
            if(*height > 200)
                *height = 200;
        }

        // Allocate a large enough buffer.
        numpels = tex->info.width * bufHeight;
        imgdata = M_Calloc(2 * numpels);
        DrawRealPatch(imgdata, /*palette,*/ tex->info.width, bufHeight, patch, 0, 0,
                      zeroMask, 0, false);
    }
    if(zeroMask && filloutlines)
        ColorOutlines(imgdata, *width, *height);
    *outbuffer = imgdata;
}

/**
 * Sky textures are usually 256 pixels wide.
 */
unsigned int GL_PrepareSky(int idx, boolean zeroMask, texinfo_t **info)
{
    return GL_PrepareSky2(idx, zeroMask, true, info);
}

/**
 * Sky textures are usually 256 pixels wide.
 */
unsigned int GL_PrepareSky2(int idx, boolean zeroMask, boolean translate,
                            texinfo_t **texinfo)
{
    boolean RGBData, alphaChannel;
    image_t image;

    if(idx >= numtextures)
        return 0;
    /*
       #if _DEBUG
       if(idx != texturetranslation[idx])
       Con_Error("Skytex: %d, translated: %d\n", idx, texturetranslation[idx]);
       #endif
     */
    if(translate)
    {
        idx = texturetranslation[idx].current;
    }

    if(!textures[idx]->tex)
    {
        // Try to load a high resolution version of this texture.
        if((loadExtAlways || highResWithPWAD ||
            !R_IsCustomMaterial(idx, MAT_TEXTURE)) &&
           GL_LoadHighResTexture(&image, textures[idx]->name) != NULL)
        {
            // High resolution texture loaded.
            RGBData = true;
            alphaChannel = (image.pixelSize == 4);
        }
        else
        {
            RGBData = false;
            GL_BufferSkyTexture(idx, &image.pixels, &image.width,
                                &image.height, zeroMask);
            alphaChannel = image.isMasked = zeroMask;
        }

        // Always disable compression on sky textures.
        // Upload it.
        textures[idx]->tex =
            GL_UploadTexture(image.pixels, image.width, image.height,
                             alphaChannel, true, RGBData, false, false,
                             glmode[mipmapping], glmode[texMagMode], texAniso,
                             DGL_REPEAT, DGL_REPEAT, TXCF_NO_COMPRESSION);

        // Do we have a masked texture?
        textures[idx]->info.masked = (image.isMasked != 0);

        // Sky textures don't support detail textures.
        textures[idx]->info.detail.tex = 0;

        // Free the buffer.
        GL_DestroyImage(&image);

    }

    if(texinfo)
        *texinfo = &textures[idx]->info;

    return textures[idx]->tex;
}

transspr_t *GL_NewTranslatedSprite(int pnum, unsigned char *table)
{
    transspr_t **newlist, *ptr;
    int i;

    newlist = Z_Malloc(sizeof(transspr_t*) * ++numtranssprites, PU_SPRITE, 0);
    if(numtranssprites > 1)
    {
        for(i = 0; i < numtranssprites -1; ++i)
            newlist[i] = transsprites[i];

        Z_Free(transsprites);
    }

    transsprites = newlist;
    ptr = transsprites[numtranssprites - 1] = Z_Calloc(sizeof(transspr_t), PU_SPRITE, 0);
    ptr->patch = pnum;
    ptr->tex = 0;
    ptr->table = table;
    return ptr;
}

transspr_t *GL_GetTranslatedSprite(int pnum, unsigned char *table)
{
    int     i;

    for(i = 0; i < numtranssprites; ++i)
        if(transsprites[i]->patch == pnum && transsprites[i]->table == table)
            return transsprites[i];
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

    if(!isPsprite)
    {
        spritelump_t *slump = spritelumps[pnum];
        lumppatch_t *patch =
            (lumppatch_t*) W_CacheLumpNum(slump->lump, PU_CACHE);

        // Calculate light source properties.
        GL_CalcLuminance(pnum, image->pixels, image->width,
                         image->height, image->pixelSize);

        if(patch)
        {
            slump->flarex *= SHORT(patch->width) / (float) image->width;
            slump->flarey *= SHORT(patch->height) / (float) image->height;
        }
    }

    if(image->pixelSize == 1 && filloutlines)
        ColorOutlines(image->pixels, image->width, image->height);

    texture =
        GL_UploadTexture(image->pixels, image->width, image->height,
                         image->pixelSize != 3, true, image->pixelSize > 1,
                         true, false, glmode[mipmapping],
                         (filterSprites ? DGL_LINEAR : DGL_NEAREST), texAniso,
                         DGL_CLAMP, DGL_CLAMP, 0);

    // Determine coordinates for the texture.
    GL_SetTexCoords(spritelumps[pnum]->tc[isPsprite], image->width,
                    image->height);

    return texture;
}

unsigned int GL_PrepareTranslatedSprite(int pnum, int tmap, int tclass)
{
    unsigned char *table =
        translationtables - 256 + tclass * ((8 - 1) * 256) + tmap * 256;
    transspr_t *tspr = GL_GetTranslatedSprite(pnum, table);
    image_t image;

    if(!tspr)
    {
        filename_t resource, fileName;
        lumppatch_t *patch =
            (lumppatch_t*) W_CacheLumpNum(spritelumps[pnum]->lump, PU_CACHE);

        // Compose a resource name.
        if(tclass || tmap)
        {
            sprintf(resource, "%s-table%i%i",
                    lumpinfo[spritelumps[pnum]->lump].name, tclass, tmap);
        }
        else
        {
            // Not actually translated? Use the normal resource.
            strcpy(resource, lumpinfo[spritelumps[pnum]->lump].name);
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

        GL_DestroyImage(&image);
    }
    return tspr->tex;
}

/**
 * Spritemodes:
 * 0 = Normal sprite
 * 1 = Psprite (HUD)
 */
unsigned int GL_PrepareSprite(int pnum, int spriteMode)
{
    DGLuint *texture;
    int     lumpNum;
    spritelump_t *slump;

    if(pnum < 0)
        return 0;

    slump = spritelumps[pnum];
    lumpNum = slump->lump;

    // Normal sprites and HUD sprites are stored separately.
    // This way a sprite can be used both in the game and the HUD, and
    // the textures can be different. (Very few sprites are used both
    // in the game and the HUD.)
    texture = (spriteMode == 0 ? &slump->tex : &slump->hudtex);

    if(!*texture)
    {
        image_t image;
        filename_t hudResource, fileName;
        lumppatch_t *patch =
                        (lumppatch_t*) W_CacheLumpNum(lumpNum, PU_CACHE);

        // Compose a resource for the psprite.
        if(spriteMode == 1)
            sprintf(hudResource, "%s-hud", lumpinfo[lumpNum].name);

        // Is there an external resource for this image? For HUD sprites,
        // first try the HUD version of the resource.
        if(!noHighResPatches &&
           ((spriteMode == 1 &&
             R_FindResource(RC_PATCH, hudResource, "-ck", fileName)) ||
            R_FindResource(RC_PATCH, lumpinfo[lumpNum].name, "-ck", fileName))
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

        *texture = GL_PrepareSpriteBuffer(pnum, &image, spriteMode == 1);
        GL_DestroyImage(&image);
    }

    return *texture;
}

void GL_DeleteSprite(int spritelump)
{
    if(spritelump < 0 || spritelump >= numSpriteLumps)
        return;

    gl.DeleteTextures(1, &spritelumps[spritelump]->tex);
    spritelumps[spritelump]->tex = 0;

    if(spritelumps[spritelump]->hudtex)
    {
        gl.DeleteTextures(1, &spritelumps[spritelump]->hudtex);
        spritelumps[spritelump]->hudtex = 0;
    }
}

void GL_GetSpriteColorf(int pnum, float *rgb)
{
    if(pnum > numSpriteLumps - 1)
        return;

    memcpy(rgb, spritelumps[pnum]->color, sizeof(float) * 3);
}

/**
 * 0 = Normal sprite
 * 1 = Psprite (HUD)
 */
void GL_SetSprite(int pnum, int spriteType)
{
    GL_BindTexture(GL_PrepareSprite(pnum, spriteType));
}

void GL_SetTranslatedSprite(int pnum, int tmap, int tclass)
{
    GL_BindTexture(GL_PrepareTranslatedSprite(pnum, tmap, tclass));
}

DGLuint GL_GetPatchOtherPart(int idx, texinfo_t **info)
{
    patch_t *patch = R_GetPatch(idx);

    if(!patch->tex)
    {
        // The patch isn't yet bound with OpenGL.
        patch->tex = GL_BindTexPatch(patch);
    }

    return GL_GetPatchInfo(idx, true, info);
}

DGLuint GL_GetRawOtherPart(int idx, texinfo_t **info)
{
    rawtex_t *raw = R_GetRawTex(idx);

    if(!raw->tex)
    {
        // The raw isn't yet bound with OpenGL.
        raw->tex = GL_BindTexRaw(raw);
    }

    return GL_GetRawTexInfo(idx, true, info);
}

/**
 * Raw images are always 320x200.
 *
 * 2003-05-30 (skyjake): External resources can be larger than 320x200,
 * but they're never split into two parts.
 */
DGLuint GL_BindTexRaw(rawtex_t *raw)
{
    image_t image;
    int     lump = raw->lump;

    if(lump < 0 || lump >= numlumps)
    {
        GL_BindTexture(0);
        return 0;
    }

    if(!raw->tex)
    {
        // First try to find an external resource.
        filename_t fileName;

        if(R_FindResource(RC_PATCH, lumpinfo[lump].name, NULL, fileName) &&
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

            raw->info.width = 320;
            raw->info2.width = 0;
            raw->tex2 = 0;
            raw->info.height = raw->info2.height = 200;

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
            if(PCX_MemoryLoad(lumpdata, lumpinfo[lump].size, 320, 200, image))
            {
                rgbdata = true;
                comps = 3;
            }
            else
            {
                // PCX load failed. It must be an old-fashioned raw image.
                need_free_image = false;
                M_Free(image);
                height = lumpinfo[lump].size / 320;
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

                raw->info.width = 256;
                raw->info2.width = 64;
                raw->info.height = raw->info2.height = height;
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

                raw->info.width = 256;
                raw->info.height = height;

                raw->tex2 = 0;
                raw->info2.width = raw->info2.height = 0;
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
 * (idx is really a lumpnum)
 */
DGLuint GL_PrepareRawTex(uint idx, boolean part2, texinfo_t **info)
{
    rawtex_t *raw = R_GetRawTex(idx);

    if(!raw->tex)
    {
        // The rawtex isn't yet bound with OpenGL.
        raw->tex = GL_BindTexRaw(raw);
    }
    return GL_GetRawTexInfo(idx, part2, info);
}

unsigned int GL_SetRawImage(unsigned int idx, boolean part2)
{
    unsigned int    tex;

    // We don't track the current texture with raw images.
    curtex = 0;

    gl.Bind(tex = GL_PrepareRawTex(idx, part2, NULL));
    return tex;
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
DGLuint GL_BindTexPatch(patch_t *p)
{
    lumppatch_t *patch;
    byte   *patchptr;
    int     lump = p->lump;
    image_t image;

    if(lump < 0 || lump >= numlumps)
    {
        GL_BindTexture(0);
        return 0;
    }

    patch = (lumppatch_t*) W_CacheLumpNum(lump, PU_CACHE);
    p->offx = -SHORT(patch->leftoffset);
    p->offy = -SHORT(patch->topoffset);

    // Let's first try the resource locator and see if there is a
    // 'high-resolution' version available.
    if((loadExtAlways || highResWithPWAD || W_IsFromIWAD(lump)) &&
       (patchptr = GL_LoadHighResPatch(&image, lumpinfo[lump].name)) != NULL)
    {
        // This is our texture! No mipmaps are generated.
        p->tex =
            GL_UploadTexture(image.pixels, image.width, image.height,
                             image.pixelSize == 4, true, true, false, false,
                             DGL_NEAREST, glmode[texMagMode], texAniso,
                             DGL_CLAMP, DGL_CLAMP, 0);

        p->info.width = SHORT(patch->width);
        p->info.height = SHORT(patch->height);

        // The original image is no longer needed.
        GL_DestroyImage(&image);
    }
    else
    {
        // Use data from the normal lump.
        boolean scaleSharp = (upscaleAndSharpenPatches ||
                              (p->info.modFlags & TXIF_UPSCALE_AND_SHARPEN));
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

        if(filloutlines && !scaleSharp)
            ColorOutlines(buffer, patchWidth, patchHeight);

        if(monochrome || (p->info.modFlags & TXIF_MONOCHROME))
        {
            DeSaturate(buffer, GL_GetPalette(), patchWidth, patchHeight);
            p->info.modFlags |= TXIF_MONOCHROME;
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
                        p->lump, p->info.modFlags & TXIF_MONOCHROME);
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
            p->info.modFlags |= TXIF_UPSCALE_AND_SHARPEN;
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
                                 alphaChannel, true, false, false, false,
                                 DGL_NEAREST, DGL_LINEAR, texAniso,
                                 DGL_CLAMP, DGL_CLAMP, 0);

            // Then part two.
            pixBlt(buffer, patchWidth, patchHeight, tempbuff,
                   part2width, patchHeight, alphaChannel, glMaxTexSize,
                   0, 0, 0, part2width, patchHeight);
            p->tex2 =
                GL_UploadTexture(tempbuff, part2width, patchHeight,
                                 alphaChannel, true, false, false, false,
                                 DGL_NEAREST, glmode[texMagMode], texAniso,
                                 DGL_CLAMP, DGL_CLAMP, 0);

            p->info.width = glMaxTexSize;
            p->info2.width = SHORT(patch->width) - glMaxTexSize;
            p->info.height = p->info2.height = SHORT(patch->height);

            M_Free(tempbuff);
        }
        else // We can use the normal one-part method.
        {
            // Generate a texture.
            p->tex =
                GL_UploadTexture(buffer, patchWidth, patchHeight,
                                 alphaChannel, true, false, false, false,
                                 DGL_NEAREST, glmode[texMagMode], texAniso,
                                 DGL_CLAMP, DGL_CLAMP, 0);

            p->info.width = SHORT(patch->width) + addBorder*2;
            p->info.height = SHORT(patch->height) + addBorder*2;
            p->info.offsetX = -addBorder;
            p->info.offsetY = -addBorder;

            p->tex2 = 0;
            p->info2.width = p->info2.height = 0;
        }
        M_Free(buffer);
    }

    return p->tex;
}

/**
 * Returns the OpenGL name of the texture.
 * (idx is really a lumpnum)
 */
DGLuint GL_PreparePatch(int idx, texinfo_t **info)
{
    patch_t *patch = R_GetPatch(idx);

    if(!patch)
        return 0;

    if(!patch->tex)
    {
        // The patch isn't yet bound with OpenGL.
        patch->tex = GL_BindTexPatch(patch);
    }
    return GL_GetPatchInfo(idx, false, info);
}

void GL_SetPatch(int idx)
{
    gl.Bind(curtex = GL_PreparePatch(idx, NULL));
}

/**
 * You should use Disable(DGL_TEXTURING) instead of this.
 */
void GL_SetNoTexture(void)
{
    gl.Bind(0);
    curtex = 0;
}

/**
 * Prepare a texture used in the lighting system. 'which' must be one
 * of the LST_* constants.
 */
DGLuint GL_PrepareLSTexture(lightingtexid_t which, texinfo_t **texinfo)
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

DGLuint GL_PrepareFlareTexture(flaretexid_t flare, texinfo_t **texinfo)
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

/**
 * Updates the textures, flats and sprites (gameTex) or the user
 * interface textures (patches and raw screens).
 */
void GL_SetTextureParams(int minMode, int magMode, int anisoLevel,
                         int gameTex, int uiTex)
{
    int     i;

    if(gameTex)
    {
        // Textures.
        for(i = 0; i < numtextures; ++i)
            if(textures[i]->tex)    // Is the texture loaded?
            {
                gl.Bind(textures[i]->tex);
                gl.TexParameter(DGL_MIN_FILTER, minMode);
                gl.TexParameter(DGL_MAG_FILTER, magMode);
                gl.TexParameter(DGL_ANISO_FILTER, anisoLevel);
            }

        // Flats.
        for(i = 0; i < numflats; ++i)
            if(flats[i]->tex)    // Is the flat loaded?
            {
                gl.Bind(flats[i]->tex);
                gl.TexParameter(DGL_MIN_FILTER, minMode);
                gl.TexParameter(DGL_MAG_FILTER, magMode);
                gl.TexParameter(DGL_ANISO_FILTER, anisoLevel);
            }

        // Sprites.
        for(i = 0; i < numSpriteLumps; ++i)
            if(spritelumps[i]->tex)
            {
                gl.Bind(spritelumps[i]->tex);
                gl.TexParameter(DGL_MIN_FILTER, minMode);
                gl.TexParameter(DGL_MAG_FILTER, magMode);
                gl.TexParameter(DGL_ANISO_FILTER, anisoLevel);
            }

        // Translated sprites.
        for(i = 0; i < numtranssprites; ++i)
        {
            gl.Bind(transsprites[i]->tex);
            gl.TexParameter(DGL_MIN_FILTER, minMode);
            gl.TexParameter(DGL_MAG_FILTER, magMode);
            gl.TexParameter(DGL_ANISO_FILTER, anisoLevel);
        }
    }

    if(uiTex)
    {
        uint            j;

        // Patches.
        {
        patch_t **patches, **ptr;
        patches = R_CollectPatches(NULL);
        for(ptr = patches; *ptr; ptr++)
        {
            if((*ptr)->tex)    // Is the texture loaded?
            {
                gl.Bind((*ptr)->tex);
                gl.TexParameter(DGL_MIN_FILTER, minMode);
                gl.TexParameter(DGL_MAG_FILTER, magMode);
            }
            if((*ptr)->tex2)    // Is the texture loaded?
            {
                gl.Bind((*ptr)->tex2);
                gl.TexParameter(DGL_MIN_FILTER, minMode);
                gl.TexParameter(DGL_MAG_FILTER, magMode);
            }
        }
        Z_Free(patches);
        }

        // Raw textures.
        for(j = 0; j < numrawtextures; ++j)
        {
            if(rawtextures[j].tex)
            {
                gl.Bind(rawtextures[j].tex);
                gl.TexParameter(DGL_MIN_FILTER, minMode);
                gl.TexParameter(DGL_MAG_FILTER, magMode);
            }
            if(rawtextures[j].tex2)
            {
                gl.Bind(rawtextures[j].tex2);
                gl.TexParameter(DGL_MIN_FILTER, minMode);
                gl.TexParameter(DGL_MAG_FILTER, magMode);
            }
        }
    }
}

void GL_UpdateTexParams(int mipmode)
{
    mipmapping = mipmode;
    GL_SetTextureParams(glmode[mipmode], glmode[texMagMode], texAniso, true, false);
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
    GL_SetTextureParams(glmode[mipmapping], glmode[texMagMode], texAniso, true, true);
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
    GL_SetTextureParams(DGL_NEAREST, DGL_NEAREST, 0, true, true);

    // And do a texreset so everything is updated.
    GL_TexReset();
}

/**
 * To save texture memory, delete all raw image textures. Raw images are
 * used as interlude backgrounds, title screens, etc. Called from
 * DD_SetupLevel.
 */
void GL_DeleteRawImages(void)
{
    uint        i;

    for(i = 0; i < numrawtextures; ++i)
    {
        gl.DeleteTextures(1, &rawtextures[i].tex);
        gl.DeleteTextures(1, &rawtextures[i].tex2);
        rawtextures[i].tex = rawtextures[i].tex2;
    }

    M_Free(rawtextures);
    rawtextures = 0;
    numrawtextures = 0;
}

/**
 * Updates the raw screen smoothing (linear magnification).
 */
void GL_UpdateRawScreenParams(int smoothing)
{
    uint        i;
    int         glmode = smoothing ? DGL_LINEAR : DGL_NEAREST;

    linearRaw = smoothing;
    for(i = 0; i < numrawtextures; ++i)
    {
        // First part 1.
        gl.Bind(rawtextures[i].tex);
        gl.TexParameter(DGL_MAG_FILTER, glmode);
        // Then part 2.
        gl.Bind(rawtextures[i].tex2);
        gl.TexParameter(DGL_MAG_FILTER, glmode);
    }
}

void GL_TextureFilterMode(int target, int parm)
{
    if(target == DD_TEXTURES)
        GL_UpdateTexParams(parm);
    if(target == DD_RAWSCREENS)
        GL_UpdateRawScreenParams(parm);
}

skintex_t *GL_GetSkinTex(const char *skin)
{
    int     i;
    skintex_t *st;
    char    realpath[256];

    if(!skin[0])
        return NULL;

    // Convert the given skin file to a full pathname.
    // \fixme Why is this done here and not during init??
    _fullpath(realpath, skin, 255);

    for(i = 0; i < numskinnames; ++i)
        if(!stricmp(skinnames[i].path, realpath))
            return skinnames + i;

    // We must allocate a new skintex_t.
    skinnames = M_Realloc(skinnames, sizeof(*skinnames) * ++numskinnames);
    st = skinnames + (numskinnames - 1);
    strcpy(st->path, realpath);
    st->tex = 0;                // Not yet prepared.

    if(verbose)
    {
        Con_Message("SkinTex: %s => %li\n", M_Pretty(skin),
                    (long) (st - skinnames));
    }
    return st;
}

skintex_t *GL_GetSkinTexByIndex(int id)
{
    if(id < 0 || id >= numskinnames)
        return NULL;            // No such thing, pal.
    return skinnames + id;
}

int GL_GetSkinTexIndex(const char *skin)
{
    skintex_t *sk = GL_GetSkinTex(skin);

    if(!sk)
        return -1;              // 'S no good, fellah!
    return sk - skinnames;
}

unsigned int GL_PrepareSkin(model_t * mdl, int skin)
{
    int     width, height, size;
    byte   *image;
    skintex_t *st;

    if(skin < 0 || skin >= mdl->info.numSkins)
        skin = 0;
    st = GL_GetSkinTexByIndex(mdl->skins[skin].id);
    if(!st)
        return 0;               // Urgh.

    // If the texture has already been loaded, we don't need to
    // do anything.
    if(!st->tex)
    {
        // Load the texture. R_LoadSkin allocates enough memory with M_Malloc.
        image = R_LoadSkin(mdl, skin, &width, &height, &size);
        if(!image)
        {
            Con_Error("GL_PrepareSkin: %s not found.\n",
                      mdl->skins[skin].name);
        }

        st->tex =
            GL_UploadTexture(image, width, height, size == 4, true, true, false, false,
                             glmode[mipmapping], DGL_LINEAR, texAniso,
                             DGL_REPEAT, DGL_REPEAT,
                             (!mdl->allowTexComp? TXCF_NO_COMPRESSION : 0));

        // We don't need the image data any more.
        M_Free(image);
    }
    return st->tex;
}

unsigned int GL_PrepareShinySkin(modeldef_t * md, int sub)
{
    //  model_t *mdl = modellist[md->sub[sub].model];
    skintex_t *stp = GL_GetSkinTexByIndex(md->sub[sub].shinyskin);
    image_t image;

    if(!stp)
        return 0;               // Does not have a shiny skin.
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
 * @return              The texture name, if it has been prepared.
 */
static DGLuint getTextureInfo2(int index, boolean translate,
                               texinfo_t **texinfo)
{
    texture_t *tex;

    if(index < 0 || index >= numtextures)
        return 0;

    // Translate the texture.
    if(translate)
    {
        index = texturetranslation[index].current;
    }

    if(index < 0 || index >= numtextures)
        return 0;

    tex = textures[index];

    if(texinfo)
        *texinfo = &textures[index]->info;

    return tex->tex;
}

/**
 * @return              The flat name, if it has been prepared.
 */
static DGLuint getFlatInfo2(int index, boolean translate,
                            texinfo_t **texinfo)
{
    flat_t         *flat;

    if(index < 0 || index >= numflats)
        return 0;

    // Translate the flat.
    if(translate)
    {
        index = flattranslation[index].current;
    }

    if(index < 0 || index >= numflats)
        return 0;

    flat = flats[index];

    if(texinfo)
        *texinfo = &flats[index]->info;

    return flat->tex;
}

/**
 * @return          The patch name, if it has been prepared.
 */
DGLuint GL_GetPatchInfo(int idx, boolean part2, texinfo_t **texinfo)
{
    patch_t *patch = R_GetPatch(idx);

    if(!patch)
        return 0;

    if(texinfo)
        *texinfo = (part2? &patch->info2 : &patch->info);

    return (part2? patch->tex2 : patch->tex);
}

/**
 * @return          The ddtexture name, if it has been prepared.
 */
static DGLuint getDDTextureInfo(ddtextureid_t which,
                                texinfo_t **texinfo)
{
    if(which < NUM_DD_TEXTURES)
    {
        if(ddTextures[which].tex)
        {
            if(texinfo)
                *texinfo = &ddTextures[which].info;

            return ddTextures[which].tex;
        }
    }

    return 0;
}

/**
 * @return          The rawtex name, if it has been prepared.
 */
DGLuint GL_GetRawTexInfo(uint idx, boolean part2, texinfo_t **texinfo)
{
    rawtex_t *rawtex = R_GetRawTex(idx);

    if(!rawtex)
        return 0;

    if(texinfo)
        *texinfo = (part2? &rawtex->info2 : &rawtex->info);

    return (part2? rawtex->tex2 : rawtex->tex);
}

/**
 * Calculates texture coordinates based on the given dimensions. The
 * coordinates are calculated as width/CeilPow2(width), or 1 if the
 * CeilPow2 would go over glMaxTexSize.
 */
static void GL_SetTexCoords(float *tc, int wid, int hgt)
{
    int     pw = M_CeilPow2(wid), ph = M_CeilPow2(hgt);

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
    char    buf[256];

    strcpy(buf, path);
    strlwr(buf);
    return strstr(buf, "-ck.") != NULL;
}

DGLuint GL_GetMaterialInfo2(int idx, materialtype_t type,
                            boolean translate, texinfo_t **info)
{
    switch(type)
    {
    case MAT_FLAT:
        return getFlatInfo2(idx, translate, info);

    case MAT_TEXTURE:
        return getTextureInfo2(idx, translate, info);

    case MAT_DDTEX:
        return getDDTextureInfo(idx, info);

    default:
        Con_Error("GL_GetMaterialInfo2: Unknown material type %i.",
                  type);
    }

    return 0;
}

DGLuint GL_GetMaterialInfo(int idx, materialtype_t type,
                           texinfo_t **info)
{
    return GL_GetMaterialInfo2(idx, type, true, info);
}

DGLuint GL_PrepareMaterial2(int idx, materialtype_t type,
                            boolean translate, texinfo_t **info)
{
    switch(type)
    {
    case MAT_FLAT:
        return prepareFlat2(idx, translate, info);

    case MAT_TEXTURE:
        return prepareTexture2(idx, translate, info);

    case MAT_DDTEX:
        return prepareDDTexture(idx, info);

    default:
        Con_Error("GL_PrepareMaterial2: Unknown material type %i.",
                  type);
    }

    return 0;
}

DGLuint GL_PrepareMaterial(int idx, materialtype_t type,
                           texinfo_t **info)
{
    return GL_PrepareMaterial2(idx, type, true, info);
}

void GL_SetMaterial(int idx, materialtype_t type)
{
    DGLuint         texID;

    switch(type)
    {
    case MAT_FLAT:
        texID = prepareFlat2(idx, true, NULL);
        break;

    case MAT_TEXTURE:
        texID = prepareTexture2(idx, true, NULL);
        break;

    case MAT_DDTEX:
        texID = prepareDDTexture(idx, NULL);
        break;

    default:
        Con_Error("GL_SetMaterial: Invalid material type %i.", type);
    }

    gl.Bind(texID);
}

/**
 * @return              A skycol_t for texidx.
 */
skycol_t *GL_GetSkyColor(int texidx)
{
    int     i, width, height;
    skycol_t *skycol;
    byte   *imgdata, *pald, rgb[3];

    if(texidx < 0 || texidx >= numtextures)
        return NULL;

    // Try to find a skytop color for this.
    for(i = 0; i < num_skytop_colors; ++i)
        if(skytop_colors[i].texidx == texidx)
            return skytop_colors + i;

    // There was no skycol for the specified texidx!
    skytop_colors =
        M_Realloc(skytop_colors, sizeof(skycol_t) * ++num_skytop_colors);
    skycol = skytop_colors + num_skytop_colors - 1;
    skycol->texidx = texidx;

    // Calculate the color.
    pald = W_CacheLumpNum(pallump, PU_STATIC);
    GL_BufferSkyTexture(texidx, &imgdata, &width, &height, false);
    LineAverageRGB(imgdata, width, height, 0, rgb, pald, false);
    for(i = 0; i < 3; ++i)
        skycol->rgb[i] = rgb[i] / 255.f;
    M_Free(imgdata);            // Free the temp buffer created by GL_BufferSkyTexture.
    W_ChangeCacheTag(pallump, PU_CACHE);
    return skycol;
}

/**
 * Retrieves the sky fadeout color of the given texture.
 */
void GL_GetSkyTopColor(int texidx, float *rgb)
{
    skycol_t *skycol = GL_GetSkyColor(texidx);

    if(!skycol)
    {
        // Must be an invalid texture, default to black.
        rgb[0] = rgb[1] = rgb[2] = 0;
    }
    else
        memcpy(rgb, skycol->rgb, sizeof(float) * 3);
}

D_CMD(LowRes)
{
    GL_LowRes();
    return true;
}

#ifdef _DEBUG
D_CMD(TranslateFont)
{
    char    name[32];
    int     i, lump;
    size_t  size;
    lumppatch_t *patch;
    byte    redToWhite[256];

    if(argc < 3)
        return false;

    // Prepare the red-to-white table.
    for(i = 0; i < 256; ++i)
    {
        if(i == 176)
            redToWhite[i] = 168;    // Full red -> white.
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
            redToWhite[i] = i;  // No translation for this.
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
    GL_UpdateRawScreenParams(strtol(argv[1], NULL, 0));
    return true;
}
