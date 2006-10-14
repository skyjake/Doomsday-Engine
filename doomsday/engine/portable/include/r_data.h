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
 * r_data.h: Data Structures For Refresh
 */

#ifndef __DOOMSDAY_REFRESH_DATA_H__
#define __DOOMSDAY_REFRESH_DATA_H__

#include "gl_main.h"
#include "dd_def.h"
#include "p_mapdata.h"
#include "p_think.h"
#include "m_nodepile.h"
#include "def_data.h"
#include "rend_bias.h"
#include "r_extres.h"

#define SIF_VISIBLE         0x1    // Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1    // Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

// Sector flags.
#define SECF_INVIS_FLOOR    0x1
#define SECF_INVIS_CEILING  0x2

#define LINE_INFO(x)    (x->info)
#define SIDE_INFO(x)    (x->info)
#define SEG_INFO(x)     (x->info)
#define SUBSECT_INFO(x) (x->info)
#define SECT_INFO(x)    (x->info)
#define SECT_FLOOR(x)   (x->planes[PLN_FLOOR]->info->visheight)
#define SECT_CEIL(x)    (x->planes[PLN_CEILING]->info->visheight)
#define SECT_PLANE_HEIGHT(x, n) (x->planes[n]->info->visheight)

// Flags for decorations.
#define DCRF_NO_IWAD    0x1         // Don't use if from IWAD.
#define DCRF_PWAD       0x2         // Can use if from PWAD.
#define DCRF_EXTERNAL   0x4         // Can use if from external resource.

// Surface flags.
#define SUF_TEXFIX      0x1         // Current texture is a fix replacement
                                    // (not sent to clients, returned via DMU etc).
#define SUF_GLOW        0x2         // Surface glows (full bright).
#define SUF_BLEND       0x4         // Surface possibly has a blended texture.
#define SUF_NO_RADIO    0x8         // No fakeradio for this surface.

// Texture flags.
#define TXF_MASKED      0x1
#define TXF_GLOW        0x2         // For lava etc, textures that glow.
#define TXF_PTCGEN      0x8         // Ptcgen def has been determined.

// Animation group flags.
#define AGF_SMOOTH      0x1
#define AGF_FIRST_ONLY  0x2
#define AGF_TEXTURE     0x1000
#define AGF_FLAT        0x2000
#define AGF_PRECACHE    0x4000      // Group is just for precaching.

// Texture definition
typedef struct {
    short           originx;
    short           originy;
    short           patch;
    short           stepdir;
    short           colormap;
} mappatch_t;

typedef struct {
    char            name[8];
    boolean         masked;
    short           width;
    short           height;
    //void          **columndirectory;  // OBSOLETE
    int             columndirectorypadding;
    short           patchcount;
    mappatch_t      patches[1];
} maptexture_t;

// strifeformat texture definition variants
typedef struct
{
    short   originx;
    short   originy;
    short   patch;
} strifemappatch_t;

typedef struct
{
    char        name[8];
    boolean         unused;
    short       width;
    short       height;
    short       patchcount;
    strifemappatch_t    patches[1];
} strifemaptexture_t;

// Detail texture information.
typedef struct detailinfo_s {
    DGLuint         tex;
    int             width, height;
    float           strength;
    float           scale;
    float           maxdist;
} detailinfo_t;

typedef struct gltexture_s {
    DGLuint         id;
    float           width, height;
    detailinfo_t   *detail;
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

typedef struct {
    float           pos[3];        // X, Y and Z coordinates.
    gl_rgba_t       color;         // Color of the vertex.
    float           dist;          // Distance to the vertex.
} rendpoly_vertex_t;

typedef struct rendpoly_wall_s {
    float           length;
    struct div_t {
        byte        num;
        float       pos[RL_MAX_DIVS];
    } divs[2];                     // For wall segments (two vertices).
} rendpoly_wall_t;

// rendpoly_t is only for convenience; the data written in the rendering
// list data buffer is taken from this struct.
typedef struct rendpoly_s {
    boolean         isWall;
    rendpolytype_t  type;
    short           flags;         // RPF_*.
    float           texoffx, texoffy;   /* Texture coordinates for left/top
                                           (in real texcoords). */
    gltexture_t     tex;
    gltexture_t     intertex;
    float           interpos;      // Blending strength (0..1).
    struct dynlight_s *lights;     // List of lights that affect this poly.
    DGLuint         decorlightmap; // Pregen RGB lightmap for decor lights.
    sector_t       *sector;        // The sector this poly belongs to (if any).
    blendmode_t     blendmode;     // Primitive-specific blending mode.

    // The geometry:
    byte            numvertices;   // Number of vertices for the poly.
    rendpoly_vertex_t *vertices;

    rendpoly_wall_t *wall;         // Wall specific data if any.
} rendpoly_t;

// This is the dummy mobj_t used for blockring roots.
// It has some excess information since it has to be compatible with
// regular mobjs (otherwise the rings don't really work).
// Note: the thinker and pos data could be used for something else...
typedef struct linkmobj_s {
    thinker_t       thinker;
    fixed_t         pos[3];
    struct mobj_s  *next, *prev;
} linkmobj_t;

typedef struct surfaceinfo_s {
    int             flags;         // SUF_ flags
    short           texture;
    boolean         isflat;        // true if current texture is a flat
    float           normal[3];     // Surface normal.
    fixed_t         texmove[2];    // Texture movement X and Y.
    float           offx;          // Texture x offset.
    float           offy;          // Texture y offset.
    byte            rgba[4];       // Surface color tint
} surfaceinfo_t;

typedef struct planeinfo_s {
    fixed_t         oldheight[2];
    float           visheight;          // Visible plane height, float,
    float           visoffset;
    sector_t       *linked;             // Plane attached to another sector.
} planeinfo_t;

typedef struct sectorinfo_s {
    sector_t       *containsector;      // Sector that contains this (if any).
    boolean         permanentlink;
    boolean         unclosed;           // An unclosed sector (some sort of fancy hack).
    boolean         selfRefHack;        // A self-referencing hack sector which ISNT
                                        // enclosed by the sector referenced.
    float           bounds[4];          // Bounding box for the sector.
    int             flags;
    int             oldlightlevel;
    byte            oldrgb[3];
    int             addspritecount;     // frame number of last R_AddSprites
    sector_t       *lightsource;        // Main sky light source
    int             blockcount;         // Number of gridblocks in the sector.
    int             changedblockcount;  // Number of blocks to mark changed.
    unsigned short *blocks;             // Light grid block indices.
} sectorinfo_t;

typedef struct vilight_s {
    short           source;
    byte            rgb[4];       // Light from an affecting source.
} vilight_t;

// Vertex illumination flags.
#define VIF_LERP         0x1      // Interpolation is in progress.
#define VIF_STILL_UNSEEN 0x2      // The color of the vertex is still unknown.

typedef struct vertexillum_s {
    gl_rgba_t       color;        // Current color of the vertex.
    gl_rgba_t       dest;         // Destination color of the vertex.
    unsigned int    updatetime;   // When the value was calculated.
    short           flags;
    vilight_t       casted[MAX_BIAS_AFFECTED];
} vertexillum_t;

typedef struct biasaffection_s {
    short           source;       // Index of light source.
} biasaffection_t;

// Shadowpoly flags.
#define SHPF_FRONTSIDE  0x1

typedef struct shadowpoly_s {
    struct line_s  *line;
    short           flags;
    ushort          visframe;      // Last visible frame (for rendering).
    vertex_t       *outer[2];      // Left and right.
    float           inoffset[2][2]; // Offset from 'outer.'
    float           extoffset[2][2];    // Extended: offset from 'outer.'
    float           bextoffset[2][2];   // Back-extended: offset frmo 'outer.'
} shadowpoly_t;

typedef struct shadowlink_s {
    struct shadowlink_s *next;
    shadowpoly_t   *poly;
} shadowlink_t;

#define SEGINF_FACINGFRONT 0x0001

typedef struct seginfo_s {
    short           flags;
    biastracker_t   tracker[3]; // 0=top, 1=middle, 2=bottom
    vertexillum_t   illum[3][4];
    uint            updated;
    biasaffection_t affected[MAX_BIAS_AFFECTED];
} seginfo_t;

typedef struct subplaneinfo_s {
    int             type;           // Plane type (ie PLN_FLOOR or PLN_CEILING)
    vertexillum_t  *illumination;
    biastracker_t   tracker;
    uint            updated;
    biasaffection_t affected[MAX_BIAS_AFFECTED];
} subplaneinfo_t;

typedef struct subsectorinfo_s {
    subplaneinfo_t **planes;
    ushort          numvertices;
    fvertex_t      *vertices;
    int             validcount;
    shadowlink_t   *shadows;
} subsectorinfo_t;

#define SIDEINF_TOPPVIS     0x0001
#define SIDEINF_MIDDLEPVIS  0x0002
#define SIDEINF_BOTTOMPVIS  0x0004

typedef struct sideinfo_s {
    short           flags;
    struct line_s  *neighbor[2];      // Left and right neighbour.
    boolean         pretendneighbor[2]; // Neighbor is not a "real" neighbor
                                      // (it does not share a line with this
                                      // side's sector).
    struct sector_s *proxsector[2];   // Sectors behind the neighbors.
    struct line_s  *backneighbor[2];  // Neighbour in the backsector (if any).
    struct line_s  *alignneighbor[2]; // Aligned left and right neighbours.
} sideinfo_t;

typedef struct lineinfo_s {
    float           length;        // Accurate length.
    binangle_t      angle;         // Calculated from front side's normal.
    boolean         selfrefhackroot; // This line is the root of a self-referencing
                                     // hack sector.
} lineinfo_t;

typedef struct vertexinfo_s {
    int             num;           // Number of sector owners.
    int            *list;          // Sector indices.
    int             numlines;      // Number of line owners.
    int            *linelist;      // Line indices.
    boolean         anchored;      // One or more of our line owners are one-sided.
} vertexinfo_t;

typedef struct polyblock_s {
    polyobj_t      *polyobj;
    struct polyblock_s *prev;
    struct polyblock_s *next;
} polyblock_t;

typedef struct {
    byte            rgb[3];
} rgbcol_t;

typedef struct {
    int             originx;       // block origin (allways UL), which has allready
    int             originy;       // accounted  for the patch's internal origin
    int             patch;
} texpatch_t;

// a maptexturedef_t describes a rectangular texture, which is composed of one
// or more mappatch_t structures that arrange graphic patches
typedef struct {
    char            name[9];       // for switch changing, etc; ends in \0
    short           width;
    short           height;
    int             flags;         // TXF_* flags.
    rgbcol_t        color;
    short           patchcount;
    DGLuint         tex;           // Name of the associated DGL texture.
    byte            masked;        // Is the (DGL) texture masked?
    detailinfo_t    detail;        // Detail texture information.
    byte            ingroup;       // True if texture belongs to some animgroup.
    struct ded_decor_s *decoration; /* Pointer to the surface
                                     * decoration, if any. */
    struct ded_reflection_s *reflection; // Surface reflection definition.

    texpatch_t      patches[1];    // [patchcount] drawn back to front
} texture_t;                       //   into the cached texture

typedef struct translation_s {
    int             current;
    int             next;
    float           inter;
} translation_t;

typedef struct flat_s {
    struct flat_s  *next;
    int             lump;
    translation_t   translation;
    short           flags;
    rgbcol_t        color;
    detailinfo_t    detail;        // Detail texture information.
    byte            ingroup;       // True if belongs to some animgroup.
    struct ded_decor_s *decoration;// Pointer to the surface decoration,
                                   // if any.
    struct ded_reflection_s *reflection; // Surface reflection definition.
    struct ded_ptcgen_s *ptcgen;   // Particle generator for the flat.
} flat_t;

typedef struct lumptexinfo_s {
    DGLuint         tex[2];        // Names of the textures (two parts for big ones)
    unsigned short  width[2], height;
    short           offx, offy;
} lumptexinfo_t;

typedef struct animframe_s {
    int             number;
    ushort          tics;
    ushort          random;
} animframe_t;

typedef struct animgroup_s {
    int             id;
    int             flags;
    int             index;
    int             maxtimer;
    int             timer;
    int             count;
    animframe_t    *frames;
} animgroup_t;

extern nodeindex_t *linelinks;
extern long    *blockmaplump;      // offsets in blockmap are from here
extern long    *blockmap;
extern int      bmapwidth, bmapheight;  // in mapblocks
extern fixed_t  bmaporgx, bmaporgy; // origin of block map
extern linkmobj_t *blockrings;
extern polyblock_t **polyblockmap;
extern byte    *rejectmatrix;      // for fast sight rejection
extern nodepile_t thingnodes, linenodes;

extern int      bspBuild;
extern int      createBMap;
extern int      createReject;

extern lumptexinfo_t *lumptexinfo;
extern int      numlumptexinfo;
extern int      viewwidth, viewheight;
extern int      numtextures;
extern texture_t **textures;
extern translation_t *texturetranslation;   // for global animation
extern int      numgroups;
extern animgroup_t *groups;
extern int      levelFullBright;
extern int      glowingTextures;
extern byte     precacheSprites, precacheSkins;

void            R_InitRendPolyPool(void);
rendpoly_t     *R_AllocRendPoly(rendpolytype_t type, boolean isWall,
                                unsigned int numverts);
void            R_FreeRendPoly(rendpoly_t *poly);
void            R_MemcpyRendPoly(rendpoly_t *dest, rendpoly_t *src);
void            R_InfoRendPolys(void);

void            R_InitData(void);
void            R_UpdateData(void);
void            R_ShutdownData(void);
void            R_UpdateSector(sector_t* sec, boolean forceUpdate);
void            R_UpdateSurface(surface_t *current, boolean forceUpdate);
void            R_UpdateAllSurfaces(boolean forceUpdate);
void            R_PrecacheLevel(void);
void            R_InitAnimGroup(ded_group_t * def);
void            R_ResetAnimGroups(void);
boolean         R_IsInAnimGroup(int groupNum, int type, int number);
void            R_AnimateAnimGroups(void);
int             R_GraphicResourceFlags(resourceclass_t rclass, int id);
flat_t         *R_FindFlat(int lumpnum);    // May return NULL.
flat_t         *R_GetFlat(int lumpnum); // Creates new entries.
flat_t        **R_CollectFlats(int *count);
int             R_FlatNumForName(char *name);
int             R_CheckTextureNumForName(char *name);
int             R_TextureNumForName(char *name);
char           *R_TextureNameForNum(int num);
int             R_SetFlatTranslation(int flat, int translateTo);
int             R_SetTextureTranslation(int tex, int translateTo);
boolean         R_IsCustomTexture(int texture);
boolean         R_IsAllowedDecoration(ded_decor_t * def, int index,
                                      boolean hasExternal);
boolean         R_IsValidLightDecoration(ded_decorlight_t * lightDef);
void            R_GenerateDecorMap(ded_decor_t * def);

#endif
