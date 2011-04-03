/**\file r_data.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * Data structures for refresh.
 */

#ifndef LIBDENG_REFRESH_DATA_H
#define LIBDENG_REFRESH_DATA_H

#include "gl_main.h"
#include "dd_def.h"
#include "p_think.h"
#include "m_nodepile.h"
#include "def_data.h"

// Flags for material decorations.
#define DCRF_NO_IWAD        0x1 // Don't use if from IWAD.
#define DCRF_PWAD           0x2 // Can use if from PWAD.
#define DCRF_EXTERNAL       0x4 // Can use if from external resource.

// Flags for material reflections.
#define REFF_NO_IWAD        0x1 // Don't use if from IWAD.
#define REFF_PWAD           0x2 // Can use if from PWAD.
#define REFF_EXTERNAL       0x4 // Can use if from external resource.

// Flags for material detailtexs.
#define DTLF_NO_IWAD        0x1 // Don't use if from IWAD.
#define DTLF_PWAD           0x2 // Can use if from PWAD.
#define DTLF_EXTERNAL       0x4 // Can use if from external resource.

typedef struct systex_s {
    textureid_t id;
    dduri_t* external;
} systex_t;

typedef struct detailtex_s {
    textureid_t id;
    boolean isExternal;
    const dduri_t* filePath;
} detailtex_t;

typedef struct lightmap_s {
    textureid_t id;
    const dduri_t* external;
} lightmap_t;

typedef struct flaretex_s {
    textureid_t id;
    const dduri_t* external;
} flaretex_t;

typedef struct shinytex_s {
    textureid_t id;
    const dduri_t* external;
} shinytex_t;

typedef struct masktex_s {
    textureid_t id;
    const dduri_t* external;
    short width, height;
} masktex_t;

typedef struct skinname_s {
    textureid_t id;
    dduri_t* path;
} skinname_t;

typedef enum {
    TU_PRIMARY = 0,
    TU_PRIMARY_DETAIL,
    TU_INTER,
    TU_INTER_DETAIL,
    NUM_TEXMAP_UNITS
} gltexunit_t;

typedef struct glcommand_vertex_s {
    float           s, t;
    int             index;
} glcommand_vertex_t;

#define RL_MAX_DIVS         64
typedef struct walldiv_s {
    unsigned int    num;
    float           pos[RL_MAX_DIVS];
} walldiv_t;

typedef struct rvertex_s {
    float           pos[3];
} rvertex_t;

typedef struct rcolor_s {
    float           rgba[4];
} rcolor_t;

typedef struct rtexcoord_s {
    float           st[2];
} rtexcoord_t;

typedef struct shadowlink_s {
    struct shadowlink_s* next;
    linedef_t*      lineDef;
    byte            side;
} shadowlink_t;

typedef struct {
    lumpnum_t       lump;
    short           offX; // block origin (allways UL), which has allready
    short           offY; // accounted for the patch's internal origin
} texpatch_t;

#define TXDF_NODRAW         0x0001 // Not to be drawn.
#define TXDF_IWAD           0x0002 // Defines an IWAD texture. Note the definition may NOT be from the IWAD.

// Describes a rectangular texture, which is composed of one
// or more texpatch_t structures that arrange graphic patches.
typedef struct {
    char            name[9];
    short           width, height;
    short           flags;
    short           patchCount;
    texpatch_t      patches[1]; // [patchcount] drawn back to front into the cached texture.
} patchcompositetex_t;

typedef struct flat_s {
    lumpname_t name;
    boolean isCustom;
} flat_t;

typedef struct {
    lumpname_t name;
    boolean isCustom;
    short width, height, offX, offY;
} spritetex_t;

// Patch flags.
#define PF_MONOCHROME         0x1
#define PF_UPSCALE_AND_SHARPEN 0x2

// A patchtex is a lumppatch that has been prepared for render.
typedef struct patchtex_s {
    lumpnum_t       lump;
    textureid_t     texId; // Name of the associated GL texture.
    boolean         isCustom;
    short           flags;
    short           width, height;
    short           offX, offY;
} patchtex_t;

#pragma pack(1)
typedef struct doompatch_header_s {
    int16_t width; // Bounding box size.
    int16_t height;
    int16_t leftOffset; // Pixels to the left of origin.
    int16_t topOffset; // Pixels below the origin.
} doompatch_header_t;
#pragma pack()

// A rawtex is a lump raw graphic that has been prepared for render.
typedef struct rawtex_s {
    lumpnum_t       lump;
    DGLuint         tex; // Name of the associated DGL texture.
    short           width, height;
    byte            masked;

    struct rawtex_s* next;
} rawtex_t;

typedef struct {
    float           approxDist; // Only an approximation.
    float           vector[3]; // Light direction vector.
    float           color[3]; // How intense the light is (0..1, RGB).
    float           offset;
    float           lightSide, darkSide; // Factors for world light.
    boolean         affectedByAmbient;
} vlight_t;

/**
 * Textures used in the lighting system.
 */
typedef enum lightingtexid_e {
    LST_DYNAMIC, // Round dynamic light
    LST_GRADIENT, // Top-down gradient
    LST_RADIO_CO, // FakeRadio closed/open corner shadow
    LST_RADIO_CC, // FakeRadio closed/closed corner shadow
    LST_RADIO_OO, // FakeRadio open/open shadow
    LST_RADIO_OE, // FakeRadio open/edge shadow
    NUM_LIGHTING_TEXTURES
} lightingtexid_t;

typedef enum flaretexid_e {
    FXT_ROUND,
    FXT_FLARE, // (flare)
    FXT_BRFLARE, // (brFlare)
    FXT_BIGFLARE, // (bigFlare)
    NUM_SYSFLARE_TEXTURES
} flaretexid_t;

typedef struct {
    DGLuint         tex;
} ddtexture_t;

extern nodeindex_t* linelinks;
extern blockmap_t* BlockMap;
extern blockmap_t* SSecBlockMap;
extern nodepile_t* mobjNodes, *lineNodes;

extern int viewwidth, viewheight;
extern int levelFullBright;
extern byte rendInfoRPolys;
extern byte precacheSprites, precacheSkins;

extern byte* translationTables;

extern systex_t** sysTextures;
extern int sysTexturesCount;

extern detailtex_t** detailTextures;
extern int detailTexturesCount;

extern lightmap_t** lightmapTextures;
extern int lightmapTexturesCount;

extern flaretex_t** flareTextures;
extern int flareTexturesCount;

extern shinytex_t** shinyTextures;
extern int shinyTexturesCount;

extern masktex_t** maskTextures;
extern int maskTexturesCount;

extern uint numSkinNames;
extern skinname_t* skinNames;

void            R_UpdateData(void);
void            R_InitRendVerticesPool(void);
rvertex_t*      R_AllocRendVertices(uint num);
rcolor_t*       R_AllocRendColors(uint num);
rtexcoord_t*    R_AllocRendTexCoords(uint num);
void            R_FreeRendVertices(rvertex_t* rvertices);
void            R_FreeRendColors(rcolor_t* rcolors);
void            R_FreeRendTexCoords(rtexcoord_t* rtexcoords);
void            R_InfoRendVerticesPool(void);

void            R_DivVerts(rvertex_t* dst, const rvertex_t* src,
                           const walldiv_t* divs);
void            R_DivVertColors(rcolor_t* dst, const rcolor_t* src,
                                const walldiv_t* divs, float bL, float tL,
                                float bR, float tR);
void            R_DivTexCoords(rtexcoord_t* dst, const rtexcoord_t* src,
                               const walldiv_t* divs, float bL, float tL,
                               float bR, float tR);

void            R_UpdatePatchCompositesAndFlats(void);

void R_InitTranslationTables(void);
void R_UpdateTranslationTables(void);

/**
 * Registers the "system" textures that are part of the Doomsday data resource package.
 */
void R_InitSystemTextures(void);
void R_DestroySystemTextures(void);

void R_InitPatchComposites(void);

/// @return  Number of PatchCompositeTextures.
int R_PatchCompositeCount(void);

/// @return  PatchCompositeTex associated to index# @a idx
patchcompositetex_t* R_PatchCompositeTextureByIndex(int idx);

void R_InitFlatTextures(void);

/// @return  Number of Flats.
int R_FlatTextureCount(void);

/// @return  Flat associated to index# @a idx
flat_t* R_FlatTextureByIndex(int idx);

void R_InitSpriteTextures(void);

/// @return  Number of SpriteTextures.
int R_SpriteTextureCount(void);

/// @return  SpriteTexture associated to index# @a idx
spritetex_t* R_SpriteTextureByIndex(int idx);

/// Prepare for color palette creation.
void R_InitColorPalettes(void);

/// To be called when any existing color palettes are no longer required.
void R_DestroyColorPalettes(void);

/// @return  Number of available color palettes.
int R_ColorPaletteCount(void);

/**
 * Given a color palette index return the associated ColorPalette.
 * @return  ColorPalette if found else @c NULL
 */
struct colorpalette_s* R_ToColorPalette(int paletteIdx);

/**
 * Add a new (named) color palette.
 * \note Part of the Doomsday public API.
 *
 * \design The idea with the two-teered implementation is to allow maximum
 * flexibility. Within the engine we can create new palettes and manipulate
 * them directly via the DGL interface. The underlying implementation is
 * wrapped in a similar way to the materials so that publically, there is a
 * set of (eternal) names and unique identifiers that survive game and GL
 * resets.
 *
 * @param fmt  Format string describes the format of @p data.
 *      Expected form: "C#C#C"
 *      C = color component, one of R, G, B.
 *      # = bits per component.
 * @param name  Unique name by which the palette will be known.
 * @param colorData  Color component triplets (at least @a colorCount * 3 values).
 * @param colorCount  Number of colors.
 *
 * @return  Color palette id.
 */
colorpaletteid_t R_CreateColorPalette(const char* fmt, const char* name,
    const uint8_t* colorData, int colorCount);

/**
 * Given a color palette id, look up the specified unique name.
 * \note Part of the Doomsday public API.
 *
 * @param id  Id of the color palette to locate.
 * @return  Pointer to the unique name associated with the specified id else @c NULL
 */
const char* R_GetColorPaletteNameForNum(colorpaletteid_t id);

/**
 * Given a color palette name, look up the associated identifier.
 * \note Part of the Doomsday public API.
 *
 * @param name Unique name of the palette to locate.
 * @return  Identifier of the palette associated with this name, else @c 0
 */
colorpaletteid_t R_GetColorPaletteNumForName(const char* name);

/**
 * Given a colorpalette id return the associated color palette index.
 * If the specified id cannot be found, the default color palette will be
 * returned instead (if defined).
 *
 * @param id  Id of the color palette to be prepared.
 *
 * @return  Index of the palette iff found else @c 0
 */
int R_FindColorPaletteIndexForId(colorpaletteid_t id);

/**
 * Given a color palette index, calculate the equivalent RGB color.
 * \note Part of the Doomsday public API.
 *
 * @param id  Id of the ColorPalette to use.
 * @param colorIdx  ColorPalette color index.
 * @param rgb  Final color will be written back here.
 * @param correctGamma  @c true if the texture gamma ramp should be applied.
 */
void R_GetColorPaletteRGBubv(colorpaletteid_t id, int colorIdx, uint8_t rgb[3],
    boolean applyTexGamma);
void R_GetColorPaletteRGBf(colorpaletteid_t id, int colorIdx, float rgb[3],
    boolean applyTexGamma);

/**
 * Change the default color palette.
 *
 * @param id  Id of the color palette to make default.
 *
 * @return  @c true iff successful, else @c NULL.
 */
boolean R_SetDefaultColorPalette(colorpaletteid_t id);

//boolean         R_UpdateSubSector(struct subsector_t* ssec, boolean forceUpdate);
boolean         R_UpdateSector(struct sector_s* sec, boolean forceUpdate);
boolean         R_UpdateLinedef(struct linedef_s* line, boolean forceUpdate);
boolean         R_UpdateSidedef(struct sidedef_s* side, boolean forceUpdate);
boolean         R_UpdatePlane(struct plane_s* pln, boolean forceUpdate);
boolean         R_UpdateSurface(struct surface_s* suf, boolean forceUpdate);

/**
 * Prepare all texture resources for the current Map.
 */
void R_PrecacheMap(void);

/**
 * Prepare all texture resources for the specified mobjtype.
 *
 * \note Part of the Doomsday public API.
 */
void R_PrecacheMobjNum(int mobjtypeNum);

uint            R_GetSkinNumForName(const dduri_t* path);
const skinname_t* R_GetSkinNameByIndex(uint id);
uint R_RegisterSkin(ddstring_t* foundPath, const char* skin, const char* modelfn, boolean isShinySkin);
void            R_DestroySkins(void);

boolean         R_DrawVLightVector(const vlight_t* light, void* context);

void            R_InitAnimGroup(ded_group_t* def);

detailtex_t*    R_CreateDetailTexture(const ded_detailtexture_t* def);
detailtex_t*    R_GetDetailTexture(const dduri_t* filePath, boolean isExternal);
void            R_DestroyDetailTextures(void);

lightmap_t*     R_CreateLightMap(const dduri_t* uri);
lightmap_t*     R_GetLightMap(const dduri_t* uri);
void            R_DestroyLightMaps(void);

flaretex_t*     R_CreateFlareTexture(const dduri_t* uri);
flaretex_t*     R_GetFlareTexture(const dduri_t* uri);
void            R_DestroyFlareTextures(void);

shinytex_t*     R_CreateShinyTexture(const dduri_t* uri);
shinytex_t*     R_GetShinyTexture(const dduri_t* uri);
void            R_DestroyShinyTextures(void);

masktex_t*      R_CreateMaskTexture(const dduri_t* uri, short width, short height);
masktex_t*      R_GetMaskTexture(const dduri_t* uri);
void            R_DestroyMaskTextures(void);

patchid_t       R_PrecachePatch(const char* name, patchinfo_t* info);
patchid_t       R_RegisterPatch(const char* name);

patchtex_t*     R_PatchTextureByIndex(patchid_t id);
void            R_ClearPatchTexs(void);
boolean         R_GetPatchInfo(patchid_t id, patchinfo_t* info);

void R_InitRawTexs(void);
void R_UpdateRawTexs(void);
rawtex_t*       R_FindRawTex(lumpnum_t lump); // May return NULL.
rawtex_t*       R_GetRawTex(lumpnum_t lump); // Creates new entries.
rawtex_t**      R_CollectRawTexs(int* count);

boolean         R_IsAllowedDecoration(ded_decor_t* def, struct material_s* mat,
                                      boolean hasExternal);
boolean         R_IsAllowedReflection(ded_reflection_t* def, struct material_s* mat,
                                      boolean hasExternal);
boolean         R_IsAllowedDetailTex(ded_detailtexture_t* def, struct material_s* mat,
                                     boolean hasExternal);
boolean         R_IsValidLightDecoration(const ded_decorlight_t* lightDef);

void            R_InitVectorGraphics(void);
void            R_UnloadVectorGraphics(void);
void            R_ShutdownVectorGraphics(void);

#endif /* LIBDENG_REFRESH_DATA_H */
