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
 * r_data.h: Data Structures For Refresh
 */

#ifndef __DOOMSDAY_REFRESH_DATA_H__
#define __DOOMSDAY_REFRESH_DATA_H__

#include "gl_main.h"
#include "dd_def.h"
#include "p_think.h"
#include "m_nodepile.h"
#include "def_data.h"
#include "r_extres.h"
#include "s_environ.h"
#include "r_materials.h"

// Flags for decorations.
#define DCRF_NO_IWAD    0x1         // Don't use if from IWAD.
#define DCRF_PWAD       0x2         // Can use if from PWAD.
#define DCRF_EXTERNAL   0x4         // Can use if from external resource.

// Texture flags.
#define TXF_NO_DRAW     0x1         // Texture should never be drawn.
#define TXF_MASKED      0x2

// Texture definition
typedef struct {
    int16_t         originX;
    int16_t         originY;
    int16_t         patch;
    int16_t         stepDir;
    int16_t         colorMap;
} mappatch_t;

typedef struct {
    char            name[8];
    int32_t         masked;
    int16_t         width;
    int16_t         height;
    //void          **columnDirectory;  // OBSOLETE
    int32_t         columnDirectoryPadding;
    int16_t         patchCount;
    mappatch_t      patches[1];
} maptexture_t;

// strifeformat texture definition variants
typedef struct {
    int16_t         originX;
    int16_t         originY;
    int16_t         patch;
} strifemappatch_t;

typedef struct {
    char            name[8];
    int32_t         unused;
    int16_t         width;
    int16_t         height;
    int16_t         patchCount;
    strifemappatch_t patches[1];
} strifemaptexture_t;

// Detail texture information.
typedef struct detailinfo_s {
    DGLuint         tex;
    int             width, height;
    float           strength;
    float           scale;
    float           maxDist;
} detailinfo_t;

typedef struct gltexture_s {
    DGLuint         id;
    float           width, height;
    boolean         masked;
    detailinfo_t*   detail;
} gltexture_t;

typedef struct glcommand_vertex_s {
    float           s, t;
    int             index;
} glcommand_vertex_t;

#define RL_MAX_DIVS         64

// Rendpoly flags.
#define RPF_MASKED      0x0001     // Use the special list for masked textures.
#define RPF_SKY_MASK    0x0004     // A sky mask polygon.
#define RPF_LIGHT       0x0008     // A dynamic light.
#define RPF_DYNLIT      0x0010     // Normal list: poly is dynamically lit.
#define RPF_GLOW        0x0020     // Multiply original vtx colors.
#define RPF_DETAIL      0x0040     // Render with detail (incl. vtx distances)
#define RPF_SHADOW      0x0100
#define RPF_HORIZONTAL  0x0200
#define RPF_SHINY       0x0400     // Shiny surface.
#define RPF_DONE        0x8000     // This poly has already been drawn.

typedef enum {
    RP_NONE,
    RP_QUAD,                       // Wall segment.
    RP_DIVQUAD,                    // Divided wall segment.
    RP_FLAT                        // Floor or ceiling.
} rendpolytype_t;

typedef struct rvertex_s {
    float           pos[3];
} rvertex_t;

typedef struct rcolor_s {
    float           rgba[4];
} rcolor_t;

typedef struct walldiv_s {
    unsigned int    num;
    float           pos[RL_MAX_DIVS];
} walldiv_t;

typedef struct rendpoly_wall_s {
    float           length;
    walldiv_t       divs[2];       // For wall segments (two vertices).
} rendpoly_wall_t;

// rendpoly_params_t is only for convenience; the data written in the rendering
// list data buffer is taken from this struct.
typedef struct rendpoly_params_s {
    boolean         isWall;
    rendpolytype_t  type;
    short           flags;          // RPF_*.
    float           texOffset[2];   // Texture coordinates for left/top
                                    // (in real texcoords).
    gltexture_t     tex;
    gltexture_t     interTex;
    float           interPos;       // Blending strength (0..1).
    uint            lightListIdx;   // List of lights that affect this poly.
    blendmode_t     blendMode;      // Primitive-specific blending mode.
    rendpoly_wall_t* wall;          // Wall specific data if any.
} rendpoly_params_t;

// This is the dummy mobj_t used for blockring roots.
// It has some excess information since it has to be compatible with
// regular mobjs (otherwise the rings don't really work).
// Note: the thinker and pos data could be used for something else...
typedef struct linkmobj_s {
    thinker_t       thinker;
    float           pos[3];
    struct mobj_s*  bNext, *bPrev;
} linkmobj_t;

typedef struct shadowlink_s {
    struct shadowlink_s* next;
    linedef_t*      lineDef;
    byte            side;
} shadowlink_t;

typedef struct subplaneinfo_s {
    vertexillum_t*  illum;
    biastracker_t   tracker;
    uint            updated;
    biasaffection_t affected[MAX_BIAS_AFFECTED];
} subplaneinfo_t;

typedef float       colorcomp_t;
typedef colorcomp_t rgbcol_t[3];
typedef colorcomp_t rgbacol_t[4];

typedef struct {
    int             originX;       // block origin (allways UL), which has allready
    int             originY;       // accounted  for the patch's internal origin
    int             patch;
} texpatch_t;

// Flags for texinfo.
#define TXIF_MONOCHROME             0x1
#define TXIF_UPSCALE_AND_SHARPEN    0x2

typedef struct {
    short           width;
    short           height;
    short           offsetX;
    short           offsetY;
    int             modFlags;      // Possible modifier filters to apply (monochrome, scale+sharp)
    byte            masked;        // Is the (DGL) texture masked?
    detailinfo_t    detail;        // Detail texture information.
} texinfo_t;

// a maptexturedef_t describes a rectangular texture, which is composed of one
// or more mappatch_t structures that arrange graphic patches
typedef struct {
    DGLuint         tex;           // Name of the associated DGL texture.
    char            name[9];       // for switch changing, etc; ends in \0
    texinfo_t       info;
    int             flags;         // TXF_* flags.
    rgbcol_t        color;
    materialclass_t materialClass;  // Used for environmental sound properties.
    short           patchCount;
    texpatch_t      patches[1];    // [patchcount] drawn back to front
} texture_t;                       //   into the cached texture

typedef struct {
    DGLuint         tex;
    texinfo_t       info;
} ddtexture_t;

typedef struct flat_s {
    DGLuint         tex;            // Name of the associated DGL texture.
    char            name[9];        // for switch changing, etc; ends in \0
    texinfo_t       info;
    int             flags;
    rgbcol_t        color;
    materialclass_t materialClass;  // Used for environmental sound properties.
    lumpnum_t       lump;
} flat_t;

typedef struct {
    lumpnum_t       lump;          // Real lump number.
    texinfo_t       info;
    int             flags;
    float           flareX;        // Offset to flare.
    float           flareY;
    float           lumSize;
    float           texCoord[2][2]; // Prepared texture coordinates.
    DGLuint         tex;           // Name of the associated DGL texture.
    DGLuint         hudTex;        // Name of the HUD sprite texture.
    rgbcol_t        color;         // Average color, for lighting.
} spritetex_t;

// Model skin.
typedef struct {
    char            path[256];
    DGLuint         tex;
} skintex_t;

// a patch is a lumppatch that has been prepared for render.
typedef struct patch_s {
    lumpnum_t       lump;
    short           offX, offY;

    // Part 1
    DGLuint         tex;          // Name of the associated DGL texture.
    texinfo_t       info;

    // Part 2 (only used with textures larger than the max texture size).
    DGLuint         tex2;
    texinfo_t       info2;

    struct patch_s* next;
} patch_t;

// A rawtex is a lump raw graphic that has been prepared for render.
typedef struct rawtex_s {
    lumpnum_t       lump;
    // Part 1
    DGLuint         tex;          // Name of the associated DGL texture.
    texinfo_t       info;

    // Part 2 (only used with textures larger than the max texture size).
    DGLuint         tex2;
    texinfo_t       info2;
} rawtex_t;

typedef struct animframe_s {
    material_t*     mat;
    ushort          tics;
    ushort          random;
} animframe_t;

typedef struct animgroup_s {
    int             id;
    int             flags;
    int             index;
    int             maxTimer;
    int             timer;
    int             count;
    animframe_t*    frames;
} animgroup_t;

typedef struct {
    boolean         used;
    float           approxDist;         // Only an approximation.
    struct lumobj_s* lum;
    float           worldVector[3];     // Light direction vector (world space).
    float           vector[3];          // Light direction vector (object space).
    float           color[3];           // How intense the light is (0..1, RGB).
    float           offset;
    float           lightSide, darkSide;    // Factors for world light.
} vlight_t;

/**
 * Textures used in the lighting system.
 */
typedef enum lightingtexid_e {
    LST_DYNAMIC,                   // Round dynamic light
    LST_GRADIENT,                  // Top-down gradient
    LST_RADIO_CO,                  // FakeRadio closed/open corner shadow
    LST_RADIO_CC,                  // FakeRadio closed/closed corner shadow
    LST_RADIO_OO,                  // FakeRadio open/open shadow
    LST_RADIO_OE,                  // FakeRadio open/edge shadow
    NUM_LIGHTING_TEXTURES
} lightingtexid_t;

typedef enum flaretexid_e {
    FXT_FLARE,                     // (flare)
    FXT_BRFLARE,                   // (brFlare)
    FXT_BIGFLARE,                  // (bigFlare)
    NUM_FLARE_TEXTURES
} flaretexid_t;

/**
 * Textures used in world rendering.
 * eg a surface with a missing tex/flat is drawn using the "missing" graphic
 */
typedef enum ddtextureid_e {
    DDT_UNKNOWN,          // Drawn if a texture/flat is unknown
    DDT_MISSING,          // Drawn in place of HOMs in dev mode.
    DDT_BBOX,             // Drawn when rendering bounding boxes
    DDT_GRAY,             // For lighting debug.
    NUM_DD_TEXTURES
} ddtextureid_t;

extern nodeindex_t* linelinks;
extern blockmap_t* BlockMap;
extern blockmap_t* SSecBlockMap;
extern linkmobj_t* blockrings;
extern byte* rejectMatrix;      // for fast sight rejection
extern nodepile_t* mobjNodes, *lineNodes;

extern int viewwidth, viewheight;
extern int numTextures;
extern texture_t** textures;
extern int numFlats;
extern flat_t** flats;

extern spritetex_t** spriteTextures;
extern int numSpriteTextures;
extern int numDDTextures;
extern ddtexture_t ddTextures[NUM_DD_TEXTURES];
extern rawtex_t* rawtextures;
extern uint numrawtextures;
extern int numgroups;
extern animgroup_t* groups;
extern int levelFullBright;
extern int glowingTextures;
extern byte precacheSprites, precacheSkins;

void            R_InitRendVerticesPool(void);
rvertex_t*      R_AllocRendVertices(uint num);
rcolor_t*       R_AllocRendColors(uint num);
void            R_FreeRendVertices(rvertex_t* rvertices);
void            R_FreeRendColors(rcolor_t* rcolors);
void            R_InfoRendVerticesPool(void);

void            R_InitData(void);
void            R_UpdateData(void);
void            R_ShutdownData(void);

void            R_UpdateSector(struct sector_s* sec, boolean forceUpdate);

void            R_PrecacheLevel(void);
void            R_PrecachePatch(lumpnum_t lump);

void            R_DestroyAnimGroups(void);
void            R_InitAnimGroup(ded_group_t* def);
void            R_ResetAnimGroups(void);
boolean         R_IsInAnimGroup(int groupNum, materialtype_t type, int number);
void            R_AnimateAnimGroups(void);

void            R_InitSpriteTextures(void);
int             R_NewSpriteTexture(lumpnum_t lump, material_t** mat);

int             R_GetSkinTexIndex(const char* skin);
skintex_t*      R_GetSkinTexByIndex(int id);
int             R_RegisterSkin(char* skin, const char* modelfn, char* fullpath);
void            R_DeleteSkinTextures(void);
void            R_DestroySkins(void); // Called at shutdown.

patch_t*        R_FindPatch(lumpnum_t lump); // May return NULL.
patch_t*        R_GetPatch(lumpnum_t lump); // Creates new entries.
patch_t**       R_CollectPatches(int* count);
rawtex_t*       R_FindRawTex(lumpnum_t lump); // May return NULL.
rawtex_t*       R_GetRawTex(lumpnum_t lump); // Creates new entries.

boolean         R_IsAllowedDecoration(ded_decor_t* def, int index,
                                      boolean hasExternal);
boolean         R_IsValidLightDecoration(const ded_decorlight_t* lightDef);
void            R_GenerateDecorMap(ded_decor_t* def);

#endif
