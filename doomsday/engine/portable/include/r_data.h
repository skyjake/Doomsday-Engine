/**\file r_data.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include "dd_types.h"
#include "gl_main.h"
#include "dd_def.h"
#include "thinker.h"
#include "m_nodepile.h"
#include "def_data.h"
#include "textures.h"

#ifdef __cplusplus
extern "C" {
#endif

struct texture_s;
struct font_s;

// Maximum number of palette translation tables.
#define NUM_TRANSLATION_CLASSES         3
#define NUM_TRANSLATION_MAPS_PER_CLASS  7
#define NUM_TRANSLATION_TABLES            (NUM_TRANSLATION_CLASSES * NUM_TRANSLATION_MAPS_PER_CLASS)

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

/**
 * @defgroup textureUnitFlags  Texture Unit Flags
 */
///@{
#define TUF_TEXTURE_IS_MANAGED    0x1 ///< A managed texture is bound to this unit.
///@}

typedef struct rtexmapunit_texture_s {
    union {
        struct {
            DGLuint name; ///< Texture used on this layer (if any).
            int magMode; ///< GL texture magnification filter.
        } gl;
        struct texturevariant_s* variant;
    };
    /// @ref textureUnitFlags
    int flags;
} rtexmapunit_texture_t;

/**
 * Texture unit state. POD.
 *
 * A simple Record data structure for storing properties used for
 * configuring a GL texture unit during render.
 */
typedef struct rtexmapuint_s {
    /// Info about the bound texture for this unit.
    rtexmapunit_texture_t texture;

    /// Currently used only with reflection.
    blendmode_t blendMode;

    /// Opacity of this layer [0..1].
    float opacity;

    /// Texture-space scale multiplier.
    vec2f_t scale;

    /// Texture-space origin translation (unscaled).
    vec2f_t offset;
} rtexmapunit_t;

/// Manipulators, for convenience.
void Rtu_Init(rtexmapunit_t* rtu);

boolean Rtu_HasTexture(const rtexmapunit_t* rtu);

/// Change the scale property.
void Rtu_SetScale(rtexmapunit_t* rtu, float s, float t);
void Rtu_SetScalev(rtexmapunit_t* rtu, float const st[2]);

/**
 * Multiply the offset and scale properties by @a scalar.
 * \note @a scalar is applied to both scale and offset properties
 * however the offset remains independent from scale (i.e., it is
 * still considered "unscaled").
 */
void Rtu_Scale(rtexmapunit_t* rtu, float scalar);
void Rtu_ScaleST(rtexmapunit_t* rtu, float const scalarST[2]);

/// Change the offset property.
void Rtu_SetOffset(rtexmapunit_t* rtu, float s, float t);
void Rtu_SetOffsetv(rtexmapunit_t* rtu, float const st[2]);

/// Translate the offset property.
void Rtu_TranslateOffset(rtexmapunit_t* rtu, float s, float t);
void Rtu_TranslateOffsetv(rtexmapunit_t* rtu, float const st[2]);

/**
 * Logical texture unit indices.
 */
typedef enum {
    RTU_PRIMARY = 0,
    RTU_PRIMARY_DETAIL,
    RTU_INTER,
    RTU_INTER_DETAIL,
    RTU_REFLECTION,
    RTU_REFLECTION_MASK,
    NUM_TEXMAP_UNITS
} rtexmapunitid_t;

typedef struct glcommand_vertex_s {
    float           s, t;
    int             index;
} glcommand_vertex_t;

typedef struct rvertex_s {
    float           pos[3];
} rvertex_t;

/**
 * ColorRawf. Color Raw (f)loating point. Is intended as a handy POD
 * structure for easy manipulation of four component, floating point
 * color plus alpha value sets.
 */
typedef struct ColorRawf_s {
    union {
        // Straight RGBA vector representation.
        float rgba[4];
        // Hybrid RGB plus alpha component representation.
        struct {
            float rgb[3];
            float alpha;
        };
        // Component-wise representation.
        struct {
            float red;
            float green;
            float blue;
            float _alpha;
        };
    };
} ColorRawf;

float ColorRawf_AverageColor(ColorRawf* color);
float ColorRawf_AverageColorMulAlpha(ColorRawf* color);

typedef struct rtexcoord_s {
    float           st[2];
} rtexcoord_t;

typedef struct shadowlink_s {
    struct shadowlink_s* next;
    LineDef*        lineDef;
    byte            side;
} shadowlink_t;

typedef struct {
    lumpnum_t lumpNum;
    short offX; /// block origin (always UL), which has allready
    short offY; /// Accounted for the patch's internal origin
} texpatch_t;

#define TXDF_NODRAW         0x0001 /// Not to be drawn.
#define TXDF_CUSTOM         0x0002 /// Definition does not define a texture that originates from the current game.

// Describes a rectangular texture, which is composed of one
// or more texpatch_t structures that arrange graphic patches.
typedef struct {
    ddstring_t name; ///< Percent-encoded.
    /// Size of the texture in logical pixels.
    Size2Raw size;
    short flags;
    /// Index of this resource according to the logic of the original game's indexing algorithm.
    int origIndex;
    short patchCount;
    texpatch_t* patches; // [patchcount] drawn back to front into the cached texture.
} patchcompositetex_t;

// Patch flags.
#define PF_MONOCHROME         0x1
#define PF_UPSCALE_AND_SHARPEN 0x2

typedef struct patchtex_s {
    short flags;
    /// Offset to texture origin in logical pixels.
    short offX, offY;
} patchtex_t;

#pragma pack(1)
typedef struct doompatch_header_s {
    int16_t width; /// Bounding box size.
    int16_t height;
    int16_t leftOffset; /// Pixels to the left of origin.
    int16_t topOffset; /// Pixels below the origin.
} doompatch_header_t;
#pragma pack()

/**
 * A rawtex is a lump raw graphic that has been prepared for render.
 */
typedef struct rawtex_s {
    ddstring_t name; ///< Percent-encoded.
    lumpnum_t lumpNum;
    DGLuint tex; /// Name of the associated DGL texture.
    short width, height;
    byte masked;
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
    LST_CAMERA_VIGNETTE,
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

extern colorpaletteid_t defaultColorPalette;

extern int levelFullBright;

extern byte rendInfoRPolys;
extern byte precacheMapMaterials, precacheSprites, precacheSkins;

extern byte* translationTables;

void            R_UpdateData(void);
void            R_InitRendVerticesPool(void);
rvertex_t*      R_AllocRendVertices(uint num);
ColorRawf*       R_AllocRendColors(uint num);
rtexcoord_t*    R_AllocRendTexCoords(uint num);
void            R_FreeRendVertices(rvertex_t* rvertices);
void            R_FreeRendColors(ColorRawf* rcolors);
void            R_FreeRendTexCoords(rtexcoord_t* rtexcoords);
void            R_InfoRendVerticesPool(void);

void R_DivVerts(rvertex_t* dst, const rvertex_t* src,
    walldivnode_t* leftDivFirst, uint leftDivCount, walldivnode_t* rightDivFirst, uint rightDivCount);

void R_DivTexCoords(rtexcoord_t* dst, const rtexcoord_t* src,
    walldivnode_t* leftDivFirst, uint leftDivCount, walldivnode_t* rightDivFirst, uint rightDivCount,
    float bL, float tL, float bR, float tR);

void R_DivVertColors(ColorRawf* dst, const ColorRawf* src,
    walldivnode_t* leftDivFirst, uint leftDivCount, walldivnode_t* rightDivFirst, uint rightDivCount,
    float bL, float tL, float bR, float tR);

void R_InitTranslationTables(void);
void R_UpdateTranslationTables(void);

void R_InitSystemTextures(void);
void R_InitPatchComposites(void);
void R_InitFlatTextures(void);
void R_InitSpriteTextures(void);

patchid_t R_DeclarePatch(const char* name);

/**
 * Retrieve extended info for the patch associated with @a id.
 * @param id  Unique identifier of the patch to lookup.
 * @param info  Extend info will be written here if found.
 * @return  @c true= Extended info for this patch was found.
 */
boolean R_GetPatchInfo(patchid_t id, patchinfo_t* info);

/// @return  Uri for the patch associated with @a id. Should be released with Uri_Delete()
Uri* R_ComposePatchUri(patchid_t id);

struct texture_s* R_CreateSkinTex(const Uri* filePath, boolean isShinySkin);

struct texture_s* R_RegisterModelSkin(ddstring_t* foundPath, const char* skin, const char* modelfn, boolean isReflection);
struct texture_s* R_FindModelSkinForResourcePath(const Uri* resourcePath);
struct texture_s* R_FindModelReflectionSkinForResourcePath(const Uri* resourcePath);

/**
 * Construct a DetailTexture according to the paramaters of the definition.
 * \note May return an existing DetailTexture if it is concluded that the
 * definition does not infer a unique DetailTexture.
 *
 * @param def  Definition describing the desired DetailTexture.
 * @return  DetailTexture inferred from the definition or @c NULL if invalid.
 */
struct texture_s* R_CreateDetailTextureFromDef(const ded_detailtexture_t* def);
struct texture_s* R_FindDetailTextureForResourcePath(const Uri* resourcePath);

struct texture_s* R_CreateLightMap(const Uri* resourcePath);
struct texture_s* R_FindLightMapForResourcePath(const Uri* resourcePath);

struct texture_s* R_CreateFlareTexture(const Uri* resourcePath);
struct texture_s* R_FindFlareTextureForResourcePath(const Uri* resourcePath);

struct texture_s* R_CreateReflectionTexture(const Uri* resourcePath);
struct texture_s* R_FindReflectionTextureForResourcePath(const Uri* resourcePath);

struct texture_s* R_CreateMaskTexture(const Uri* resourcePath, const Size2Raw* size);
struct texture_s* R_FindMaskTextureForResourcePath(const Uri* resourcePath);

void R_InitRawTexs(void);
void R_UpdateRawTexs(void);

/**
 * Returns a rawtex_t* for the given lump if one already exists else @c NULL.
 */
rawtex_t* R_FindRawTex(lumpnum_t lumpNum);

/**
 * Get a rawtex_t data structure for a raw texture specified with a WAD lump
 * number. Allocates a new rawtex_t if it hasn't been loaded yet.
 */
rawtex_t* R_GetRawTex(lumpnum_t lumpNum);

/**
 * Returns a NULL-terminated array of pointers to all the rawtexs.
 * The array must be freed with Z_Free.
 */
rawtex_t** R_CollectRawTexs(int* count);

/// Prepare for color palette creation.
void R_InitColorPalettes(void);

/// To be called when any existing color palettes are no longer required.
void R_DestroyColorPalettes(void);

/// @return  Number of available color palettes.
int R_ColorPaletteCount(void);

/// @return  ColorPalette associated with unique @a id, else @c NULL.
struct colorpalette_s* R_ToColorPalette(colorpaletteid_t id);

/**
 * Given a color palette list index return the ColorPalette.
 * @return  ColorPalette if found else @c NULL
 */
struct colorpalette_s* R_GetColorPaletteByIndex(int paletteIdx);

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
 * Given a color palette index, calculate the equivalent RGB color.
 * \note Part of the Doomsday public API.
 *
 * @param id  Id of the ColorPalette to use.
 * @param colorIdx  ColorPalette color index.
 * @param rgb  Final color will be written back here.
 * @param correctGamma  @c true if the texture gamma ramp should be applied.
 */
void R_GetColorPaletteRGBubv(colorpaletteid_t id, int colorIdx, uint8_t rgb[3], boolean applyTexGamma);
void R_GetColorPaletteRGBf(colorpaletteid_t id, int colorIdx, float rgb[3], boolean applyTexGamma);

/**
 * Change the default color palette.
 *
 * @param id  Id of the color palette to make default.
 *
 * @return  @c true iff successful, else @c NULL.
 */
boolean R_SetDefaultColorPalette(colorpaletteid_t id);

//boolean         R_UpdateBspLeaf(struct BspLeaf* bspLeaf, boolean forceUpdate);
boolean         R_UpdateSector(struct sector_s* sec, boolean forceUpdate);
boolean         R_UpdateLinedef(struct linedef_s* line, boolean forceUpdate);
boolean         R_UpdateSidedef(struct sidedef_s* side, boolean forceUpdate);
boolean         R_UpdatePlane(struct plane_s* pln, boolean forceUpdate);
boolean         R_UpdateSurface(struct surface_s* suf, boolean forceUpdate);

/**
 * Prepare resources for the current Map.
 */
void R_PrecacheForMap(void);

/**
 * Prepare all texture resources for the specified mobjtype.
 *
 * \note Part of the Doomsday public API.
 */
void R_PrecacheMobjNum(int mobjtypeNum);
boolean R_DrawVLightVector(const vlight_t* light, void* context);

/**
 * @return  @c true if the given decoration works under the specified circumstances.
 */
boolean R_IsAllowedDecoration(ded_decor_t* def, boolean hasExternal, boolean isCustom);

/**
 * @return  @c true if the given reflection works under the specified circumstances.
 */
boolean R_IsAllowedReflection(ded_reflection_t* def, boolean hasExternal, boolean isCustom);

/**
 * @return  @c true if the given decoration works under the specified circumstances.
 */
boolean R_IsAllowedDetailTex(ded_detailtexture_t* def, boolean hasExternal, boolean isCustom);

boolean R_IsValidLightDecoration(const ded_decorlight_t* lightDef);

void R_InitSvgs(void);

/**
 * Unload any resources needed for vector graphics.
 * Called during shutdown and before a renderer restart.
 */
void R_UnloadSvgs(void);

void R_ShutdownSvgs(void);

int R_TextureUniqueId2(const Uri* uri, boolean quiet);
int R_TextureUniqueId(const Uri* uri); /* quiet=false */

typedef struct animframe_s {
    textureid_t texture;
    ushort tics;
    ushort randomTics;
} animframe_t;

typedef struct animgroup_s {
    int id;
    int flags;
    int count;
    animframe_t* frames;
} animgroup_t;

/// @return  Number of animation/precache groups.
int R_AnimGroupCount(void);

/// To be called to destroy all animation groups when they are no longer needed.
void R_ClearAnimGroups(void);

/// @return  AnimGroup associated with @a animGroupNum else @c NULL
const animgroup_t* R_ToAnimGroup(int animGroupNum);

/**
 * Create a new animation group.
 * @return  Logical (unique) identifier reference associated with the new group.
 */
int R_CreateAnimGroup(int flags);

/**
 * Append a new @a texture frame to the identified @a animGroupNum.
 *
 * @param animGroupNum  Logical identifier reference to the group being modified.
 * @param texture  Texture frame to be inserted into the group.
 * @param tics  Base duration of the new frame in tics.
 * @param randomTics  Extra frame duration in tics (randomized on each cycle).
 */
void R_AddAnimGroupFrame(int animGroupNum, const Uri* texture, int tics, int randomTics);

/// @return  @c true iff @a texture is linked to the identified @a animGroupNum.
boolean R_IsTextureInAnimGroup(const Uri* texture, int animGroupNum);

struct font_s* R_CreateFontFromFile(const Uri* uri, const char* resourcePath);
struct font_s* R_CreateFontFromDef(ded_compositefont_t* def);

#ifdef __cplusplus
}
#endif

#endif /// LIBDENG_REFRESH_DATA_H
