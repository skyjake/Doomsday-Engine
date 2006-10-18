/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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
    unsigned char rgb[3];
} skycol_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(LowRes);
D_CMD(ResetTextures);
D_CMD(MipMap);
D_CMD(SmoothRaw);

byte   *GL_LoadHighResFlat(image_t * img, char *name);
void    GL_DeleteDetailTexture(detailtex_t * dtex);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void LoadPalette(void);
static void GL_SetTexCoords(float *tc, int wid, int hgt);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int ratioLimit;
extern boolean palettedTextureExtAvailable;
extern boolean s3tcAvailable;

extern int skyflatnum;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     glMaxTexSize;           // Maximum supported texture size.
int     ratioLimit = 0;         // Zero if none.
boolean filloutlines = true;
byte    loadExtAlways = false;  // Always check for extres (cvar)
byte    paletted = false;       // Use GL_EXT_paletted_texture (cvar)
boolean load8bit = false;       // Load textures as 8 bit? (w/paltex)
int     monochrome = 0;         // desaturate a patch (average colours)
int     useSmartFilter = 0;     // Smart filter mode (cvar: 1=hq2x)

int     mipmapping = 3, linearRaw = 1, texQuality = TEXQ_BEST;
int     filterSprites = true;

// Properties of the current texture.
float   texw = 1, texh = 1;
int     texmask = 0;
DGLuint curtex = 0;
detailinfo_t *texdetail;

int     texMagMode = 1;         // Linear.

// Convert a 18-bit RGB (666) value to a playpal index.
// FIXME: 256kb - Too big?
byte    pal18to8[262144];

int     pallump;

// Names of the dynamic light textures.
DGLuint lightingTexNames[NUM_LIGHTING_TEXTURES];

// Names of the "built-in" Doomsday textures.
DGLuint ddTextures[NUM_DD_TEXTURES];

// Names of the flare textures (halos).
DGLuint flaretexnames[NUM_FLARE_TEXTURES];

skycol_t *skytop_colors = NULL;
int     num_skytop_colors = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean texInited = false;   // Init done.
static boolean allowMaskedTexEnlarge = false;
static boolean noHighResTex = false;
static boolean noHighResPatches = false;
static boolean highResWithPWAD = false;

// Raw screen lumps (just lump numbers).
static int *rawlumps, numrawlumps;

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
    C_VAR_INT2("rend-tex-gamma", &usegamma, CVF_PROTECTED, 0, 4,
               GL_DoTexReset);
    C_VAR_INT2("rend-tex-mipmap", &mipmapping, CVF_PROTECTED, 0, 5,
               GL_DoTexReset);
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
    C_VAR_INT("rend-tex-detail", &r_detail, 0, 0, 1);
    C_VAR_FLOAT("rend-tex-detail-scale", &detailScale,
                CVF_NO_MIN | CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-tex-detail-strength", &detailFactor, 0, 0, 10);
    C_VAR_INT("rend-tex-detail-multitex", &useMultiTexDetails, 0, 0, 1);

    // Ccmds
    C_CMD("lowres", "", LowRes);
    C_CMD("mipmap", "i", MipMap);
    C_CMD("smoothscr", "i", SmoothRaw);
    C_CMD("texreset", "", ResetTextures);
}

/*
 * This should be cleaned up once and for all.
 */
void GL_InitTextureManager(void)
{
    int     i;

    if(novideo)
        return;
    if(texInited)
        return;                 // Don't init again.

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
    rawlumps = 0;
    numrawlumps = 0;

    // The palette lump, for color information (really??!!?!?!).
    pallump = W_GetNumForName(PALLUMPNAME);

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

    // Load the pal18to8 table from the lump PAL18TO8. We need it
    // when resizing textures.
    if((i = W_CheckNumForName("PAL18TO8")) == -1)
        CalculatePal18to8(pal18to8, GL_GetPalette());
    else
        memcpy(pal18to8, W_CacheLumpNum(i, PU_CACHE), sizeof(pal18to8));

    // Detail textures.
    dtinstances = NULL;

    // System textures loaded in GL_LoadSystemTextures.
    memset(flaretexnames, 0, sizeof(flaretexnames));
    memset(lightingTexNames, 0, sizeof(lightingTexNames));
    memset(ddTextures, 0, sizeof(ddTextures));

    // Initialization done.
    texInited = true;

    // Initialize the smart texture filtering routines.
    GL_InitSmartFilter();
}

/*
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

/*
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
    byte   *playpal;
    byte    paldata[256 * 3];
    int     i, c, gammalevel = /*gammaSupport? 0 : */ usegamma;

    pallump = W_GetNumForName(PALLUMPNAME);
    playpal = GL_GetPalette();

    // Prepare the color table.
    for(i = 0; i < 256; ++i)
    {
        // Adjust the values for the appropriate gamma level.
        for(c = 0; c < 3; ++c)
            paldata[i * 3 + c] = gammatable[gammalevel][playpal[i * 3 + c]];
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

/*
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

/*
 * Lightmaps should be monochrome images.
 */
void GL_LoadLightMap(ded_lightmap_t *map)
{
    image_t image;
    filename_t resource;

    if(map->tex)
        return;                 // Already loaded.

    // Default texture name.
    map->tex = lightingTexNames[LST_DYNAMIC];

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

            map->tex = gl.NewTexture();

            // Upload the texture.
            // No mipmapping or resizing is needed, upload directly.
            gl.Disable(DGL_TEXTURE_COMPRESSION);
            gl.TexImage(image.pixelSize ==
                        2 ? DGL_LUMINANCE_PLUS_A8 : image.pixelSize ==
                        3 ? DGL_RGB : DGL_RGBA, image.width, image.height, 0,
                        image.pixels);
            gl.Enable(DGL_TEXTURE_COMPRESSION);
            GL_DestroyImage(&image);

            gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR);
            gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
            gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
            gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

            // Copy this to all defs with the same lightmap.
            Def_LightMapLoaded(map->id, map->tex);
        }
    }
}

void GL_DeleteLightMap(ded_lightmap_t *map)
{
    if(map->tex != lightingTexNames[LST_DYNAMIC])
    {
        gl.DeleteTextures(1, &map->tex);
    }
    map->tex = 0;
}

/*
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

            map->tex = gl.NewTexture();

            // Upload the texture.
            // No mipmapping or resizing is needed, upload directly.
            gl.Disable(DGL_TEXTURE_COMPRESSION);
            gl.TexImage(image.pixelSize ==
                        2 ? DGL_LUMINANCE_PLUS_A8 : image.pixelSize ==
                        3 ? DGL_RGB : DGL_RGBA, image.width, image.height, 0,
                        image.pixels);
            gl.Enable(DGL_TEXTURE_COMPRESSION);
            GL_DestroyImage(&image);

            gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
            gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
            gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
            gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

            // Copy this to all defs with the same flaremap.
            Def_FlareMapLoaded(map->id, map->tex, map->disabled, map->custom);
            loaded = true;
        }
    }

    if(!loaded)
    {   // External resource not found.
        // Perhaps a "built-in" flare texture id?
        char   *end;
        int     id, pass;
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

void GL_DeleteFlareMap(ded_flaremap_t * map)
{
    if(map->tex != flaretexnames[FXT_FLARE])
    {
        gl.DeleteTextures(1, &map->tex);
    }
    map->tex = 0;
}

/*
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
                                          LGM_NORMAL, DGL_FALSE, true);
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
                                             LGM_NORMAL, DGL_TRUE, true);
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

/*
 * Called from GL_LoadSystemTextures.
 */
void GL_LoadDDTextures(void)
{
    GL_PrepareDDTexture(DDT_UNKNOWN);
    GL_PrepareDDTexture(DDT_MISSING);
    GL_PrepareDDTexture(DDT_BBOX);
    GL_PrepareDDTexture(DDT_GRAY);
}

void GL_ClearDDTextures(void)
{
    gl.DeleteTextures(NUM_DD_TEXTURES, ddTextures);
    memset(ddTextures, 0, sizeof(ddTextures));
}

/*
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

/*
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

    gl.DeleteTextures(NUM_LIGHTING_TEXTURES, lightingTexNames);
    memset(lightingTexNames, 0, sizeof(lightingTexNames));

    gl.DeleteTextures(NUM_FLARE_TEXTURES, flaretexnames);
    memset(flaretexnames, 0, sizeof(flaretexnames));

    GL_ClearDDTextures();
    UI_ClearTextures();

    // Delete the particle textures.
    PG_ShutdownTextures();
}

/*
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

    // Textures and sprite lumps.
    for(i = 0; i < numtextures; ++i)
        GL_DeleteTexture(i);
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

    GL_DeleteRawImages();

    // Delete any remaining lump textures (e.g. flats).
    for(i = 0; i < numlumptexinfo; ++i)
    {
        gl.DeleteTextures(2, lumptexinfo[i].tex);
        memset(lumptexinfo[i].tex, 0, sizeof(lumptexinfo[i].tex));
    }
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

void GL_UpdateGamma(void)
{
    /*if(gammaSupport)
       {
       // The driver knows how to update the gamma directly.
       gl.Gamma(DGL_TRUE, gammatable[usegamma]);
       }
       else
       { */
    LoadPalette();
    GL_ClearRuntimeTextures();
    //}
}

/*
 * Binds the texture if necessary.
 */
void GL_BindTexture(DGLuint texname)
{
    /*if(curtex != texname)
       { */
    gl.Bind(texname);
    curtex = texname;
    //}
}

/*
 * Can be rather time-consuming.
 * Returns the name of the texture.
 * The texture parameters will NOT be set here.
 * 'data' contains indices to the playpal. If 'alphachannel' is true,
 * 'data' also contains the alpha values (after the indices).
 */
DGLuint GL_UploadTexture(byte *data, int width, int height,
                         boolean alphaChannel, boolean generateMipmaps,
                         boolean RGBData, boolean noStretch)
{
    byte   *palette = GL_GetPalette();
    int     i, levelWidth, levelHeight; // width and height at the current level
    int     comps;
    byte   *buffer, *rgbaOriginal, *idxBuffer;
    DGLuint texName;
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
    if(useSmartFilter /* && comps == 3 */ )
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

    // Generate a new texture name and bind it.
    texName = gl.NewTexture();

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
            if(gl.
               TexImage(alphaChannel ? DGL_COLOR_INDEX_8_PLUS_A8 :
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
        if(gl.
           TexImage(alphaChannel ? DGL_RGBA : DGL_RGB, levelWidth, levelHeight,
                    generateMipmaps ? DGL_TRUE : DGL_FALSE, buffer) != DGL_OK)
        {
            Con_Error
                ("GL_UploadTexture: TexImage failed (%i x %i), alpha:%i\n",
                 levelWidth, levelHeight, alphaChannel);
        }
    }

    if(freeBuffer)
        M_Free(buffer);

    return texName;
}

/*
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

/*
 * Detail textures are grayscale images.
 */
DGLuint GL_LoadDetailTexture(int num, float contrast, const char *external)
{
    byte   *lumpData, *image;
    int     w = 256, h = 256;
    dtexinst_t *inst;

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
    gl.SetInteger(DGL_GRAY_MIPMAP, contrast * 255);

    // Try external first.
    if(external != NULL)
    {
        inst->tex = GL_LoadGraphics2(RC_TEXTURE, external, LGM_NORMAL,
            DGL_GRAY_MIPMAP, true);

        if(inst->tex == 0)
        {
            VERBOSE(Con_Message("GL_LoadDetailTexture: "
                                "Failed to load: %s\n", external));
        }
    }
    else
    {
        lumpData = W_CacheLumpNum(num, PU_STATIC);

        // First try loading it as a PCX image.
        if(PCX_MemoryGetSize(lumpData, &w, &h))
        {
            // Nice...
            image = M_Malloc(w * h * 3);
            PCX_MemoryLoad(lumpData, W_LumpLength(num), w, h, image);
            inst->tex = gl.NewTexture();
            // Make faded mipmaps.
            if(!gl.TexImage(DGL_RGB, w, h, DGL_GRAY_MIPMAP, image))
            {
                Con_Error
                    ("GL_LoadDetailTexture: %-8s (%ix%i): "
                     "not powers of two.\n", lumpinfo[num].name, w, h);
            }
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
                    w = h = 64;
                }
                else
                {
                    w = h = 128;
                }
            }
            image = M_Malloc(w * h);
            memcpy(image, W_CacheLumpNum(num, PU_CACHE), w * h);
            inst->tex = gl.NewTexture();
            // Make faded mipmaps.
            gl.TexImage(DGL_LUMINANCE, w, h, DGL_GRAY_MIPMAP, image);
        }

        // Free allocated memory.
        M_Free(image);
        W_ChangeCacheTag(num, PU_CACHE);
    }

    // Set texture parameters.
    gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR_MIPMAP_LINEAR);
    gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
    gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
    gl.TexParameter(DGL_WRAP_T, DGL_REPEAT);

    return inst->tex;
}

/*
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
           (!is_wall_texture && index == dt->flat_lump))
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

/*
 * No translation is done.
 */
DGLuint GL_BindTexFlat(flat_t * fl)
{
    DGLuint name;
    byte   *flatptr;
    int     lump = fl->lump, width, height, pixSize = 3;
    boolean RGBData = false, freeptr = false;
    ded_detailtexture_t *def;
    image_t image;
    boolean hasExternal = false;

    if(lump < 0 || lump >= numlumps)
    {
        GL_BindTexture(0);
        return 0;
    }

    // Is there a high resolution version?
    if((loadExtAlways || highResWithPWAD || W_IsFromIWAD(lump)) &&
       (flatptr = GL_LoadHighResFlat(&image, lumpinfo[lump].name)) != NULL)
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
        if(lumpinfo[lump].size < 4096)
            return 0;           // Too small.
        // Get a pointer to the texture data.
        flatptr = W_CacheLumpNum(lump, PU_CACHE);
        width = height = 64;
    }
    // Is there a detail texture for this?
    if((fl->detail.tex = GL_PrepareDetailTexture(fl->lump, false, &def)))
    {
        // The width and height could be used for something meaningful.
        fl->detail.width = 128;
        fl->detail.height = 128;
        fl->detail.scale = def->scale;
        fl->detail.strength = def->strength;
        fl->detail.maxdist = def->maxdist;
    }

    // Load the texture.
    name =
        GL_UploadTexture(flatptr, width, height, pixSize == 4, true, RGBData,
                         false);

    // Average color for glow planes.
    if(RGBData)
    {
        averageColorRGB(&fl->color, flatptr, width, height);
    }
    else
    {
        averageColorIdx(&fl->color, flatptr, width, height,
                        GL_GetPalette(), false);
    }

    // Set the parameters.
    gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
    gl.TexParameter(DGL_MAG_FILTER, glmode[texMagMode]);

    if(freeptr)
        M_Free(flatptr);

    // Is there a surface decoration for this flat?
    fl->decoration = Def_GetDecoration(lump, false, hasExternal);

    // Get the surface reflection for this flat.
    fl->reflection = Def_GetReflection(lump, false);

    // The name of the texture is returned.
    return name;
}

unsigned int GL_PrepareFlat(int idx)
{
    return GL_PrepareFlat2(idx, true);
}

/*
 * Returns the OpenGL name of the texture.
 * (idx is really a lumpnum)
 */
unsigned int GL_PrepareFlat2(int idx, boolean translate)
{
    flat_t *flat = R_GetFlat(idx);

    // Get the translated one?
    if(translate && flat->translation.current != idx)
    {
        flat = R_GetFlat(flat->translation.current);
    }

    if(!lumptexinfo[flat->lump].tex[0])
    {
        // The flat isn't yet bound with OpenGL.
        lumptexinfo[flat->lump].tex[0] = GL_BindTexFlat(flat);
    }
    texw = texh = 64;
    texmask = false;
    texdetail = (r_detail && flat->detail.tex ? &flat->detail : 0);
    return lumptexinfo[flat->lump].tex[0];
}

/*
 * Prepares one of the "Doomsday Textures" 'which' must be one
 * of the DDT_* constants.
 */
DGLuint GL_PrepareDDTexture(ddtexture_t which)
{
    static const char *ddTexNames[NUM_DD_TEXTURES] = {
        "unknown",
        "missing",
        "bbox",
        "gray"
    };

    if(which < NUM_DD_TEXTURES)
    {
        if(!ddTextures[which])
        {
            ddTextures[which] =
                GL_LoadGraphics2(RC_GRAPHICS, ddTexNames[which], LGM_NORMAL,
                                 DGL_TRUE, false);

            if(!ddTextures[which])
                Con_Error("GL_PrepareDDTexture: \"%s\" not found!\n",
                          ddTexNames[which]);
        }
    }
    else
        Con_Error("GL_PrepareDDTexture: Invalid ddtexture %i\n", which);

    texw = texh = 64;
    texmask = false;
    texdetail = 0;
    return ddTextures[which];
}

/*
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

/*
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

/*
 * Frees all memory associated with the image.
 */
void GL_DestroyImage(image_t * img)
{
    M_Free(img->pixels);
    img->pixels = NULL;
}

/*
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

/*
 * Use this when loading custom textures from the Data\*\Textures dir.
 * The returned buffer must be freed with M_Free.
 */
byte *GL_LoadTexture(image_t * img, char *name)
{
    return GL_LoadHighRes(img, name, "", true, RC_TEXTURE);
}

/*
 * Use this when loading high-res wall textures.
 * The returned buffer must be freed with M_Free.
 */
byte *GL_LoadHighResTexture(image_t * img, char *name)
{
    if(noHighResTex)
        return NULL;
    return GL_LoadTexture(img, name);
}

/*
 * The returned buffer must be freed with M_Free.
 */
byte *GL_LoadHighResFlat(image_t * img, char *name)
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

/*
 * Extended version that uses a custom resource class.
 * Set mode to 2 to include an alpha channel. Set to 3 to make the
 * actual pixel colors all white.
 */
DGLuint GL_LoadGraphics2(resourceclass_t resClass, const char *name,
                         gfxmode_t mode, int useMipmap, boolean clamped)
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
        }

        GL_DestroyImage(&image);
    }
    return texture;
}

DGLuint GL_LoadGraphics(const char *name, gfxmode_t mode)
{
    return GL_LoadGraphics2(RC_GRAPHICS, name, mode, DGL_FALSE, true);
}

/*
 * Renders the given texture into the buffer.
 */
boolean GL_BufferTexture(texture_t *tex, byte *buffer, int width, int height,
                         int *has_big_patch)
{
    int     i, len;
    boolean alphaChannel = false;
    patch_t *patch;

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
        patch = W_CacheLumpNum(tex->patches[i].patch, PU_CACHE);
        // Check for big patches?
        if(SHORT(patch->height) > tex->height && has_big_patch &&
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

/*
 * Returns the DGL texture name.
 */
unsigned int GL_PrepareTexture(int idx)
{
    return GL_PrepareTexture2(idx, true);
}

/*
 * Returns the DGL texture name.
 */
unsigned int GL_PrepareTexture2(int idx, boolean translate)
{
    ded_detailtexture_t *def;
    int     originalIndex = idx;
    texture_t *tex;
    boolean alphaChannel = false, RGBData = false;
    int     i;
    image_t image;
    boolean hasExternal = false;

    if(idx == 0)
    {
        // No texture?
        texw = 1;
        texh = 1;
        texmask = 0;
        texdetail = 0;
        return 0;
    }
    if(translate)
    {
        idx = texturetranslation[idx].current;
    }
    tex = textures[idx];
    if(!textures[idx]->tex)
    {
        // Try to load a high resolution version of this texture.
        if((loadExtAlways || highResWithPWAD || !R_IsCustomTexture(idx)) &&
           GL_LoadHighResTexture(&image, tex->name) != NULL)
        {
            // High resolution texture loaded.
            RGBData = true;
            alphaChannel = (image.pixelSize == 4);
            hasExternal = true;
        }
        else
        {
            image.width = tex->width;
            image.height = tex->height;
            image.pixels = M_Malloc(2 * image.width * image.height);
            image.isMasked =
                GL_BufferTexture(tex, image.pixels, image.width, image.height,
                                 &i);

            // The -bigmtex option allows the engine to resize masked
            // textures whose patches are too big to fit the texture.
            if(allowMaskedTexEnlarge && image.isMasked && i)
            {
                // Adjust height to fit the largest patch.
                tex->height = image.height = i;
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
        if((textures[idx]->detail.tex =
            GL_PrepareDetailTexture(idx, true, &def)))
        {
            // The width and height could be used for something meaningful.
            textures[idx]->detail.width = 128;
            textures[idx]->detail.height = 128;
            textures[idx]->detail.scale = def->scale;
            textures[idx]->detail.strength = def->strength;
            textures[idx]->detail.maxdist = def->maxdist;
        }

        textures[idx]->tex =
            GL_UploadTexture(image.pixels, image.width, image.height,
                             alphaChannel, true, RGBData, false);

        // Average color for glow planes.
        if(RGBData)
        {
            averageColorRGB(&textures[idx]->color, image.pixels, image.width,
                            image.height);
        }
        else
        {
            averageColorIdx(&textures[idx]->color, image.pixels, image.width,
                            image.height, GL_GetPalette(), false);
        }

        // Set texture parameters.
        gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
        gl.TexParameter(DGL_MAG_FILTER, glmode[texMagMode]);

        textures[idx]->masked = (image.isMasked != 0);

        /*if(alphaChannel)
           {
           // Don't tile masked textures vertically.
           gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
           } */

        GL_DestroyImage(&image);

        // Is there a decoration for this surface?
        textures[idx]->decoration = Def_GetDecoration(idx, true, hasExternal);

        // Get the reflection for this surface.
        textures[idx]->reflection = Def_GetReflection(idx, true);
    }
    return GL_GetTextureInfo2(originalIndex, translate);
}

/*
 * Draws the given sky texture in a buffer. The returned buffer must be
 * freed by the caller. Idx must be a valid texture number.
 */
void GL_BufferSkyTexture(int idx, byte **outbuffer, int *width, int *height,
                         boolean zeroMask)
{
    texture_t *tex = textures[idx];
    byte   *imgdata;
    int     i, numpels;

    *width = tex->width;
    *height = tex->height;

    if(tex->patchcount > 1)
    {
        numpels = tex->width * tex->height;
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
            DrawRealPatch(imgdata, /*palette,*/ tex->width, tex->height,
                          W_CacheLumpNum(tex->patches[i].patch, PU_CACHE),
                          tex->patches[i].originx, tex->patches[i].originy,
                          zeroMask, 0, false);
        }
    }
    else
    {
        patch_t *patch = W_CacheLumpNum(tex->patches[0].patch, PU_CACHE);
        int     bufHeight =
            SHORT(patch->height) > tex->height ? SHORT(patch->height)
            : tex->height;
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
        numpels = tex->width * bufHeight;
        imgdata = M_Calloc(2 * numpels);
        DrawRealPatch(imgdata, /*palette,*/ tex->width, bufHeight, patch, 0, 0,
                      zeroMask, 0, false);
    }
    if(zeroMask && filloutlines)
        ColorOutlines(imgdata, *width, *height);
    *outbuffer = imgdata;
}

/*
 * Sky textures are usually 256 pixels wide.
 */
unsigned int GL_PrepareSky(int idx, boolean zeroMask)
{
    return GL_PrepareSky2(idx, zeroMask, true);
}

/*
 * Sky textures are usually 256 pixels wide.
 */
unsigned int GL_PrepareSky2(int idx, boolean zeroMask, boolean translate)
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
        if((loadExtAlways || highResWithPWAD || !R_IsCustomTexture(idx)) &&
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
        gl.Disable(DGL_TEXTURE_COMPRESSION);

        // Upload it.
        textures[idx]->tex =
            GL_UploadTexture(image.pixels, image.width, image.height,
                             alphaChannel, true, RGBData, false);
        gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
        gl.TexParameter(DGL_MAG_FILTER, glmode[texMagMode]);
        gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
        gl.TexParameter(DGL_WRAP_T, DGL_REPEAT);

        // Enable compression again.
        gl.Enable(DGL_TEXTURE_COMPRESSION);

        // Do we have a masked texture?
        textures[idx]->masked = (image.isMasked != 0);

        // Free the buffer.
        GL_DestroyImage(&image);

    }
    texw = textures[idx]->width;
    texh = textures[idx]->height;
    texmask = textures[idx]->masked;
    texdetail = 0;
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

/*
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
        patch_t *patch = W_CacheLumpNum(slump->lump, PU_CACHE);

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
                         true);

    gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
    gl.TexParameter(DGL_MAG_FILTER, filterSprites ? DGL_LINEAR : DGL_NEAREST);
    gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
    gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

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
        patch_t *patch = W_CacheLumpNum(spritelumps[pnum]->lump, PU_CACHE);

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

/*
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
        patch_t *patch = W_CacheLumpNum(lumpNum, PU_CACHE);

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
    int i;

    if(pnum > numSpriteLumps - 1)
        return;

    for(i=0; i < 3; ++i)
        rgb[i] = spritelumps[pnum]->color.rgb[i] * reciprocal255;
}

/*
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

void GL_NewRawLump(int lump)
{
    rawlumps = M_Realloc(rawlumps, sizeof(int) * ++numrawlumps);
    rawlumps[numrawlumps - 1] = lump;
}

DGLuint GL_GetOtherPart(int lump)
{
    return lumptexinfo[lump].tex[1];
}

/*
 * Sets texture parameters for raw image textures (parts).
 */
void GL_SetRawImageParams(void)
{
    gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
    gl.TexParameter(DGL_MAG_FILTER, linearRaw ? DGL_LINEAR : DGL_NEAREST);
    gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
    gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
}

/*
 * Prepares and uploads a raw image texture from the given lump.
 * Called only by GL_SetRawImage(), so params are valid.
 */
void GL_SetRawImageLump(int lump, int part)
{
    int     i, k, c, idx;
    byte   *dat1, *dat2, *palette, *lumpdata, *image;
    int     height, assumedWidth = 320;
    boolean need_free_image = true;
    boolean rgbdata;
    int     comps;
    lumptexinfo_t *info = lumptexinfo + lump;

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

    // Two pieces:
    dat1 = M_Malloc(comps * 256 * 256);
    dat2 = M_Malloc(comps * 64 * 256);

    if(height < 200 && part == 2)
        goto dont_load;         // What is this?!
    if(height < 200)
        assumedWidth = 256;

    memset(dat1, 0, comps * 256 * 256);
    memset(dat2, 0, comps * 64 * 256);
    palette = GL_GetPalette();

    // Image data loaded, divide it into two parts.
    for(k = 0; k < height; ++k)
        for(i = 0; i < 256; ++i)
        {
            idx = k * assumedWidth + i;
            // Part one.
            for(c = 0; c < comps; ++c)
                dat1[(k * 256 + i) * comps + c] = image[idx * comps + c];
            // We can setup part two at the same time.
            if(i < 64 && part)
                for(c = 0; c < comps; ++c)
                {
                    dat2[(k * 64 + i) * comps + c] =
                        image[(idx + 256) * comps + c];
                }
        }

    // Upload part one.
    info->tex[0] =
        GL_UploadTexture(dat1, 256, assumedWidth < 320 ? height : 256, false,
                         false, rgbdata, false);
    GL_SetRawImageParams();

    if(part)
    {
        // And the other part.
        info->tex[1] =
            GL_UploadTexture(dat2, 64, 256, false, false, rgbdata, false);
        GL_SetRawImageParams();

        // Add it to the list.
        GL_NewRawLump(lump);
    }

    info->width[0] = 256;
    info->width[1] = 64;
    info->height = height;

  dont_load:
    if(need_free_image)
        M_Free(image);
    M_Free(dat1);
    M_Free(dat2);
    W_ChangeCacheTag(lump, PU_CACHE);
}

/*
 * Raw images are always 320x200.
 * Part is either 1 or 2. Part 0 means only the left side is loaded.
 * No splittex is created in that case. Once a raw image is loaded
 * as part 0 it must be deleted before the other part is loaded at the
 * next loading. Part can also contain the width and height of the
 * texture.
 *
 * 2003-05-30 (skyjake): External resources can be larger than 320x200,
 * but they're never split into two parts.
 */
unsigned int GL_SetRawImage(int lump, int part)
{
    DGLuint texId = 0;
    image_t image;
    lumptexinfo_t *info = lumptexinfo + lump;

    // Check the part.
    if(part < 0 || part > 2 || lump >= numlumps)
        return texId;

    if(!info->tex[0])
    {
        // First try to find an external resource.
        filename_t fileName;

        if(R_FindResource(RC_PATCH, lumpinfo[lump].name, NULL, fileName) &&
           GL_LoadImage(&image, fileName, false) != NULL)
        {
            // We have the image in the buffer. We'll upload it as one
            // big texture.
            info->tex[0] =
                GL_UploadTexture(image.pixels, image.width, image.height,
                                 image.pixelSize == 4, false, true, false);
            GL_SetRawImageParams();

            info->width[0] = 320;
            info->width[1] = 0;
            info->tex[1] = 0;
            info->height = 200;

            GL_DestroyImage(&image);
        }
        else
        {
            // Must load the old-fashioned data lump.
            GL_SetRawImageLump(lump, part);
        }
    }

    // Bind the part that was asked for.
    if(!info->tex[1])
    {
        // There's only one part, so we'll bind it.
        texId = info->tex[0];
    }
    else
    {
        texId = info->tex[part <= 1 ? 0 : 1];
    }
    gl.Bind(texId);

    // We don't track the current texture with raw images.
    curtex = 0;

    return texId;
}

/*
 * Loads and sets up a patch using data from the specified lump.
 */
void GL_PrepareLumpPatch(int lump)
{
    patch_t *patch = W_CacheLumpNum(lump, PU_CACHE);
    int     numpels = SHORT(patch->width) * SHORT(patch->height), alphaChannel;
    byte   *buffer;

    if(!numpels)
        return;                 // This won't do!

    // Allocate memory for the patch.
    buffer = M_Calloc(2 * numpels);

    alphaChannel =
        DrawRealPatch(buffer, SHORT(patch->width), SHORT(patch->height),
                      patch, 0, 0, false, 0, true);
    if(filloutlines)
        ColorOutlines(buffer, SHORT(patch->width), SHORT(patch->height));

    if(monochrome)
        DeSaturate(buffer, GL_GetPalette(), SHORT(patch->width),
                   SHORT(patch->height));

    // See if we have to split the patch into two parts.
    // This is done to conserve the quality of wide textures
    // (like the status bar) on video cards that have a pitifully
    // small maximum texture size. ;-)
    if(SHORT(patch->width) > glMaxTexSize)
    {
        // The width of the first part is glMaxTexSize.
        int     part2width = SHORT(patch->width) - glMaxTexSize;
        byte   *tempbuff =
            M_Malloc(2 * MAX_OF(glMaxTexSize, part2width) *
                     SHORT(patch->height));

        // We'll use a temporary buffer for doing to splitting.
        // First, part one.
        pixBlt(buffer, SHORT(patch->width), SHORT(patch->height), tempbuff,
               glMaxTexSize, SHORT(patch->height), alphaChannel,
               0, 0, 0, 0, glMaxTexSize, SHORT(patch->height));
        lumptexinfo[lump].tex[0] =
            GL_UploadTexture(tempbuff, glMaxTexSize, SHORT(patch->height),
                             alphaChannel, false, false, false);

        gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
        gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
        gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
        gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

        // Then part two.
        pixBlt(buffer, SHORT(patch->width), SHORT(patch->height), tempbuff,
               part2width, SHORT(patch->height), alphaChannel, glMaxTexSize,
               0, 0, 0, part2width, SHORT(patch->height));
        lumptexinfo[lump].tex[1] =
            GL_UploadTexture(tempbuff, part2width, SHORT(patch->height),
                             alphaChannel, false, false, false);

        gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
        gl.TexParameter(DGL_MAG_FILTER, glmode[texMagMode]);
        gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
        gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

        GL_BindTexture(lumptexinfo[lump].tex[0]);

        lumptexinfo[lump].width[0] = glMaxTexSize;
        lumptexinfo[lump].width[1] = SHORT(patch->width) - glMaxTexSize;

        M_Free(tempbuff);
    }
    else                        // We can use the normal one-part method.
    {
        // Generate a texture.
        lumptexinfo[lump].tex[0] =
            GL_UploadTexture(buffer, SHORT(patch->width), SHORT(patch->height),
                             alphaChannel, false, false, false);
        gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
        gl.TexParameter(DGL_MAG_FILTER, glmode[texMagMode]);
        gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
        gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

        lumptexinfo[lump].width[0] = SHORT(patch->width);
        lumptexinfo[lump].width[1] = 0;
    }
    M_Free(buffer);
}

/*
 * No mipmaps are generated for regular patches.
 */
void GL_SetPatch(int lump)
{
    if(lump >= numlumps)
        return;

    if(!lumptexinfo[lump].tex[0])
    {
        patch_t *patch = W_CacheLumpNum(lump, PU_CACHE);
        filename_t fileName;
        image_t image;

        // Let's first try the resource locator and see if there is a
        // 'high-resolution' version available.
        if(!noHighResPatches &&
           (loadExtAlways || highResWithPWAD || W_IsFromIWAD(lump)) &&
           R_FindResource(RC_PATCH, lumpinfo[lump].name, "-ck", fileName) &&
           GL_LoadImage(&image, fileName, false))
        {
            // This is our texture! No mipmaps are generated.
            lumptexinfo[lump].tex[0] =
                GL_UploadTexture(image.pixels, image.width, image.height,
                                 image.pixelSize == 4, false, true, false);

            // The original image is no longer needed.
            GL_DestroyImage(&image);

            // Set the texture parameters.
            gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
            gl.TexParameter(DGL_MAG_FILTER, glmode[texMagMode]);
            gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
            gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

            lumptexinfo[lump].width[0] = SHORT(patch->width);
            lumptexinfo[lump].width[1] = 0;
            lumptexinfo[lump].tex[1] = 0;
        }
        else
        {
            // Use data from the normal lump.
            GL_PrepareLumpPatch(lump);
        }

        // The rest of the size information.
        lumptexinfo[lump].height = SHORT(patch->height);
        lumptexinfo[lump].offx = -SHORT(patch->leftoffset);
        lumptexinfo[lump].offy = -SHORT(patch->topoffset);
    }
    else
    {
        GL_BindTexture(lumptexinfo[lump].tex[0]);
    }
    curtex = lumptexinfo[lump].tex[0];
}

/*
 * You should use Disable(DGL_TEXTURING) instead of this.
 */
void GL_SetNoTexture(void)
{
    gl.Bind(0);
    curtex = 0;
}

/*
 * Prepare a texture used in the lighting system. 'which' must be one
 * of the LST_* constants.
 */
DGLuint GL_PrepareLSTexture(lightingtex_t which)
{
    switch (which)
    {
    case LST_DYNAMIC:
        // The dynamic light map is a 64x64 grayscale 8-bit image.
        if(!lightingTexNames[LST_DYNAMIC])
        {
            // We don't want to compress the flares (banding would be
            // noticeable).
            gl.Disable(DGL_TEXTURE_COMPRESSION);

            lightingTexNames[LST_DYNAMIC] =
                GL_LoadGraphics("dLight", LGM_WHITE_ALPHA);

            gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR);
            gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
            gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
            gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

            // Enable texture compression as usual.
            gl.Enable(DGL_TEXTURE_COMPRESSION);
        }
        // Global tex variables not set! (scalable texture)
        return lightingTexNames[LST_DYNAMIC];

    case LST_GRADIENT:
        if(!lightingTexNames[LST_GRADIENT])
        {
            lightingTexNames[LST_GRADIENT] =
                GL_LoadGraphics("wallglow", LGM_WHITE_ALPHA);

            gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR);
            gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
            gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
            gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
        }
        // Global tex variables not set! (scalable texture)
        return lightingTexNames[LST_GRADIENT];

    case LST_RADIO_CO:          // closed/open
    case LST_RADIO_CC:          // closed/closed
    case LST_RADIO_OO:          // open/open
    case LST_RADIO_OE:          // open/edge
        // FakeRadio corner shadows.
        if(!lightingTexNames[which])
        {
            // Disable texture compression
            gl.Disable(DGL_TEXTURE_COMPRESSION);

            switch(which)
            {
            case LST_RADIO_CO:
                lightingTexNames[which] =
                    GL_LoadGraphics("radioCO", LGM_WHITE_ALPHA);
                break;

            case LST_RADIO_CC:
                lightingTexNames[which] =
                    GL_LoadGraphics("radioCC", LGM_WHITE_ALPHA);
                break;

            case LST_RADIO_OO:
                lightingTexNames[which] =
                    GL_LoadGraphics("radioOO", LGM_WHITE_ALPHA);
                break;

            case LST_RADIO_OE:
                lightingTexNames[which] =
                    GL_LoadGraphics("radioOE", LGM_WHITE_ALPHA);
                break;

            default:
                break;
            }
            // Enable texture compression as usual.
            gl.Enable(DGL_TEXTURE_COMPRESSION);

            gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR);
            gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
            gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
            gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
        }
        // Global tex variables not set! (scalable texture)
        return lightingTexNames[which];

    default:
        // Failed to prepare anything.
        return 0;
    }
}

DGLuint GL_PrepareFlareTexture(flaretex_t flare)
{
    // There are three flare textures.
    if(flare >= NUM_FLARE_TEXTURES)
        return 0;

    if(!flaretexnames[flare])
    {
        // We don't want to compress the flares (banding would be noticeable).
        gl.Disable(DGL_TEXTURE_COMPRESSION);

        flaretexnames[flare] =
            GL_LoadGraphics(flare == 0 ? "flare" : flare == 1 ? "brflare" :
                           "bigflare", LGM_WHITE_ALPHA);

        if(flaretexnames[flare] == 0)
        {
            Con_Error("GL_PrepareFlareTexture: flare texture %i not found!\n",
                      flare);
        }

        gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
        gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
        gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
        gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

        // Allow texture compression as usual.
        gl.Enable(DGL_TEXTURE_COMPRESSION);
    }

    return flaretexnames[flare];
}

int GL_GetLumpTexWidth(int lump)
{
    return lumptexinfo[lump].width[0];
}

int GL_GetLumpTexHeight(int lump)
{
    return lumptexinfo[lump].height;
}

/*
 * Updates the textures, flats and sprites (gameTex) or the user
 * interface textures (patches and raw screens).
 */
void GL_SetTextureParams(int minMode, int magMode, int gameTex, int uiTex)
{
    int     i, k;
    flat_t **flats, **ptr;

    if(gameTex)
    {
        // Textures.
        for(i = 0; i < numtextures; ++i)
            if(textures[i]->tex)    // Is the texture loaded?
            {
                gl.Bind(textures[i]->tex);
                gl.TexParameter(DGL_MIN_FILTER, minMode);
                gl.TexParameter(DGL_MAG_FILTER, magMode);
            }
        // Flats.
        flats = R_CollectFlats(NULL);
        for(ptr = flats; *ptr; ptr++)
            if(lumptexinfo[(*ptr)->lump].tex[0])    // Is the texture loaded?
            {
                gl.Bind(lumptexinfo[(*ptr)->lump].tex[0]);
                gl.TexParameter(DGL_MIN_FILTER, minMode);
                gl.TexParameter(DGL_MAG_FILTER, magMode);
            }
        Z_Free(flats);
        // Sprites.
        for(i = 0; i < numSpriteLumps; ++i)
            if(spritelumps[i]->tex)
            {
                gl.Bind(spritelumps[i]->tex);
                gl.TexParameter(DGL_MIN_FILTER, minMode);
                gl.TexParameter(DGL_MAG_FILTER, magMode);
            }
        // Translated sprites.
        for(i = 0; i < numtranssprites; ++i)
        {
            gl.Bind(transsprites[i]->tex);
            gl.TexParameter(DGL_MIN_FILTER, minMode);
            gl.TexParameter(DGL_MAG_FILTER, magMode);
        }
    }
    if(uiTex)
    {
        for(i = 0; i < numlumps; ++i)
            for(k = 0; k < 2; ++k)
                if(lumptexinfo[i].tex[k])
                {
                    gl.Bind(lumptexinfo[i].tex[k]);
                    gl.TexParameter(DGL_MIN_FILTER, minMode);
                    gl.TexParameter(DGL_MAG_FILTER, magMode);
                }
    }
}

void GL_UpdateTexParams(int mipmode)
{
    mipmapping = mipmode;
    GL_SetTextureParams(glmode[mipmode], glmode[texMagMode], true, false);
}

void GL_TexReset(void)
{
    GL_ClearTextureMemory();
    GL_LoadSystemTextures(true, true);
    Con_Printf("All DGL textures deleted.\n");
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
    GL_SetTextureParams(DGL_NEAREST, DGL_NEAREST, true, true);

    // And do a texreset so everything is updated.
    GL_TexReset();
}

/*
 * To save texture memory, delete all raw image textures. Raw images are
 * used as interlude backgrounds, title screens, etc. Called from
 * DD_SetupLevel.
 */
void GL_DeleteRawImages(void)
{
    int     i;

    for(i = 0; i < numrawlumps; ++i)
    {
        gl.DeleteTextures(2, lumptexinfo[rawlumps[i]].tex);
        lumptexinfo[rawlumps[i]].tex[0] = lumptexinfo[rawlumps[i]].tex[1] = 0;
    }

    M_Free(rawlumps);
    rawlumps = 0;
    numrawlumps = 0;
}

/*
 * Updates the raw screen smoothing (linear magnification).
 */
void GL_UpdateRawScreenParams(int smoothing)
{
    int     i;
    int     glmode = smoothing ? DGL_LINEAR : DGL_NEAREST;

    linearRaw = smoothing;
    for(i = 0; i < numrawlumps; ++i)
    {
        // First part 1.
        gl.Bind(lumptexinfo[rawlumps[i]].tex[0]);
        gl.TexParameter(DGL_MAG_FILTER, glmode);
        // Then part 2.
        gl.Bind(lumptexinfo[rawlumps[i]].tex[1]);
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

/*
 * Deletes a texture. Only for textures (not for sprites, flats, etc.).
 */
void GL_DeleteTexture(int texidx)
{
    if(texidx < 0 || texidx >= numtextures)
        return;

    if(textures[texidx]->tex)
    {
        gl.DeleteTextures(1, &textures[texidx]->tex);
        textures[texidx]->tex = 0;
    }
}

unsigned int GL_GetTextureName(int texidx)
{
    return textures[texidx]->tex;
}

skintex_t *GL_GetSkinTex(const char *skin)
{
    int     i;
    skintex_t *st;
    char    realpath[256];

    if(!skin[0])
        return NULL;

    // Convert the given skin file to a full pathname.
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
        Con_Message("SkinTex: %s => %i\n", M_Pretty(skin), st - skinnames);
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

        if(!mdl->allowTexComp)
        {
            // This will prevent texture compression.
            gl.Disable(DGL_TEXTURE_COMPRESSION);
        }

        st->tex =
            GL_UploadTexture(image, width, height, size == 4, true, true,
                             false);

        gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
        gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
        gl.TexParameter(DGL_WRAP_S, DGL_WRAP_S);
        gl.TexParameter(DGL_WRAP_T, DGL_WRAP_T);

        // Compression can be enabled again.
        gl.Enable(DGL_TEXTURE_COMPRESSION);

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
                             image.pixelSize == 4, true, true, false);

        gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
        gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
        gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
        gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

        // We don't need the image data any more.
        GL_DestroyImage(&image);
    }
    return stp->tex;
}

/*
 * Returns the texture name, if it has been prepared.
 */
DGLuint GL_GetTextureInfo(int index)
{
    return GL_GetTextureInfo2(index, true);
}

/*
 * Returns the texture name, if it has been prepared.
 */
DGLuint GL_GetTextureInfo2(int index, boolean translate)
{
    texture_t *tex;

    if(!index)
        return 0;

    // Translate the texture.
    if(translate)
    {
        index = texturetranslation[index].current;
    }
    tex = textures[index];

    // Set the global texture info variables.
    texw = tex->width;
    texh = tex->height;
    texmask = tex->masked;
    texdetail = (r_detail && tex->detail.tex ? &tex->detail : 0);
    return tex->tex;
}

/**
 * Copy the averaged texture color into the dest buffer <code>rgb</code>.
 *
 * @param   texid       The id of the texture.
 * @param   rgb         The dest buffer.
 */
void GL_GetTextureColor(int texid, unsigned char *rgb)
{
    texture_t *tex = textures[texid];

    memcpy(rgb, tex->color.rgb, 3);
}

void GL_SetTexture(int idx)
{
    gl.Bind(GL_PrepareTexture(idx));
}

/*
 * Calculates texture coordinates based on the given dimensions. The
 * coordinates are calculated as width/CeilPow2(width), or 1 if the
 * CeilPow2 would go over glMaxTexSize.
 */
static void GL_SetTexCoords(float *tc, int wid, int hgt)
{
    int     pw = CeilPow2(wid), ph = CeilPow2(hgt);

    if(pw > glMaxTexSize || ph > glMaxTexSize)
        tc[VX] = tc[VY] = 1;
    else
    {
        tc[VX] = wid / (float) pw;
        tc[VY] = hgt / (float) ph;
    }
}

/*
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

/*
 * Return a skycol_t for texidx.
 */
skycol_t *GL_GetSkyColor(int texidx)
{
    int     i, width, height;
    skycol_t *skycol;
    byte   *imgdata, *pald;

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
    LineAverageRGB(imgdata, width, height, 0, skycol->rgb, pald, false);
    M_Free(imgdata);            // Free the temp buffer created by GL_BufferSkyTexture.
    W_ChangeCacheTag(pallump, PU_CACHE);
    return skycol;
}

/*
 * Returns the sky fadeout color of the given texture.
 */
void GL_GetSkyTopColor(int texidx, byte *rgb)
{
    skycol_t *skycol = GL_GetSkyColor(texidx);

    if(!skycol)
    {
        // Must be an invalid texture.
        memset(rgb, 0, 3);      // Default to black.
    }
    else
        memcpy(rgb, skycol->rgb, 3);
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
    int     i, lump, size;
    patch_t *patch;
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
    GL_TexReset();
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
