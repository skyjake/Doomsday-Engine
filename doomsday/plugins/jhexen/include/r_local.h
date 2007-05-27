/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

#ifndef __R_LOCAL__
#define __R_LOCAL__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "jhexen.h"

//#pragma pack(1)

#define ANGLETOSKYSHIFT         22 // sky map is 256*128*4 maps

#define BASEYCENTER                     100

#define MAXWIDTH                        1120
#define MAXHEIGHT                       832

#define PI                                      3.141592657

#define CENTERY                         (SCREENHEIGHT/2)

#define MINZ                    (FRACUNIT*4)

#define FIELDOFVIEW             2048    // fineangles in the SCREENWIDTH wide window

//
// lighting constants
//
#define LIGHTLEVELS                     16
#define LIGHTSEGSHIFT           4
#define MAXLIGHTSCALE           48
#define LIGHTSCALESHIFT         12
#define MAXLIGHTZ                       128
#define LIGHTZSHIFT                     20
#define NUMCOLORMAPS            32 // number of diminishing
#define INVERSECOLORMAP         32

// Player colors.
#define AM_PLR1_COLOR 157       // Blue
#define AM_PLR2_COLOR 177       // Red
#define AM_PLR3_COLOR 137       // Yellow
#define AM_PLR4_COLOR 198       // Green
#define AM_PLR5_COLOR 215       // Jade
#define AM_PLR6_COLOR 32        // White
#define AM_PLR7_COLOR 106       // Hazel
#define AM_PLR8_COLOR 234       // Purple

/*
   ==============================================================================

   INTERNAL MAP TYPES

   ==============================================================================
 */

//================ used by play and refresh

typedef struct xsector_s {
#if 0
    fixed_t         floorheight, ceilingheight;
    short           floorpic, ceilingpic;
    short           lightlevel;
    byte            rgb[3];
    int             validcount;    // if == validcount, already checked
    mobj_t         *thinglist;     // list of mobjs in sector
    int             linecount;
    struct line_s **Lines;         // [linecount] size
    float           flatoffx, flatoffy; // Scrolling flats.
    float           ceiloffx, ceiloffy; // Scrolling ceilings.
    int             skyfix;        // Offset to ceiling height rendering w/sky.
    float           reverb[NUM_REVERB_DATA];
    int             blockbox[4];   // mapblock bounding box for height changes
    plane_t         planes[2];     // PLN_*
    degenmobj_t     soundorg;      // for any sounds played by the sector
#endif
    // --- You can freely make changes after this.

    short           special, tag;
    int             soundtraversed; // 0 = untraversed, 1,2 = sndlines -1
    mobj_t         *soundtarget;   // thing that made a sound (or null)
    seqtype_t       seqType;       // stone, metal, heavy, etc...
    void           *specialdata;   // thinker_t for reversable actions
} xsector_t;

typedef struct xline_s {
    byte            special;
    byte            arg1;
    byte            arg2;
    byte            arg3;
    byte            arg4;
    byte            arg5;
    // Has been rendered at least once and needs to appear in the map,
    // for each player.
    boolean         mapped[MAXPLAYERS];
    int             validcount;
} xline_t;

/*
// ===== Polyobj data =====
typedef struct xpolyobj_s {
#if 0
    int             numSegs;
    seg_t         **Segs;
    int             validcount;
    degenmobj_t     startSpot;
    angle_t         angle;
    vertex_t       *originalPts;   // used as the base for the rotations
    vertex_t       *prevPts;       // use to restore the old point values
    int             tag;           // reference tag assigned in HereticEd
    int             bbox[4];
    vertex_t        dest;
    int             speed;         // Destination XY and speed.
    angle_t         destAngle, angleSpeed;  // Destination angle and rotation speed.

    // --- You can freely make changes after this.
#endif

    boolean         crush;         // should the polyobj attempt to crush mobjs?
    int             seqType;
    fixed_t         size;          // polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
    void           *specialdata;   // pointer a thinker, if the poly is moving
} xpolyobj_t;
*/
/*
#if 0
typedef struct polyblock_s {
    polyobj_t      *polyobj;
    struct polyblock_s *prev;
    struct polyblock_s *next;
    // Don't change this; engine uses a similar struct.
} polyblock_t;
#endif
*/

/*
   ==============================================================================

   OTHER TYPES

   ==============================================================================
 */

typedef byte    lighttable_t;      // this could be wider for >8 bit display


//=============================================================================

// Map data is in the main engine, so these are helpers...
extern angle_t  clipangle;

extern int      viewangletox[FINEANGLES / 2];
extern angle_t  xtoviewangle[SCREENWIDTH + 1];
extern fixed_t  finetangent[FINEANGLES / 2];

extern fixed_t  rw_distance;
extern angle_t  rw_normalangle;

//
// R_main.c
//
extern int      centerx, centery;
extern int      flyheight;

#define validCount      (*gi.validcount)

extern int      sscount, linecount, loopcount;
extern lighttable_t *scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
extern lighttable_t *scalelightfixed[MAXLIGHTSCALE];
extern lighttable_t *zlight[LIGHTLEVELS][MAXLIGHTZ];

extern int      extralight;

extern fixed_t  viewcos, viewsin;

extern int      detailshift;       // 0 = high, 1 = low

extern void     (*colfunc) (void);
extern void     (*basecolfunc) (void);
extern void     (*fuzzcolfunc) (void);
extern void     (*spanfunc) (void);

//
// R_bsp.c
//
extern seg_t   *curline;
extern side_t  *sidedef;
extern line_t  *linedef;
extern sector_t *frontsector, *backsector;

extern int      rw_x;
extern int      rw_stopx;

extern boolean  segtextured;
extern boolean  markfloor;         // false if the back side is the same plane
extern boolean  markceiling;
extern boolean  skymap;

//extern  drawseg_t       drawsegs[MAXDRAWSEGS], *ds_p;

extern lighttable_t **hscalelight, **vscalelight, **dscalelight;

typedef void    (*drawfunc_t) (int start, int stop);
void            R_ClearClipSegs(void);

void            R_ClearDrawSegs(void);

//void R_InitSkyMap (void);
void            R_RenderBSPNode(int bspnum);

//
// R_segs.c
//
extern int      rw_angle1;         // angle to line origin
extern int      TransTextureStart;
extern int      TransTextureEnd;

//void R_RenderMaskedSegRange (drawseg_t *ds, int x1, int x2);

//extern  short                   openings[MAXOPENINGS], *lastopening;

extern short    floorclip[SCREENWIDTH];
extern short    ceilingclip[SCREENWIDTH];

extern fixed_t  yslope[SCREENHEIGHT];
extern fixed_t  distscale[SCREENWIDTH];

void            R_InitPlanes(void);
void            R_ClearPlanes(void);
void            R_MapPlane(int y, int x1, int x2);
void            R_MakeSpans(int x, int t1, int b1, int t2, int b2);
void            R_DrawPlanes(void);

/*visplane_t *R_FindPlane (fixed_t height, int picnum, int lightlevel,
   int special);
   visplane_t *R_CheckPlane (visplane_t *pl, int start, int stop); */

//
// R_debug.m
//
extern int      drawbsp;

void            RD_OpenMapWindow(void);
void            RD_ClearMapWindow(void);
void            RD_DisplayLine(int x1, int y1, int x2, int y2, float gray);
void            RD_DrawNodeLine(node_t * node);
void            RD_DrawLineCheck(seg_t *line);
void            RD_DrawLine(seg_t *line);
void            RD_DrawBBox(fixed_t *bbox);

//
// R_data.c
//
typedef struct {
    int             originx;       // block origin (allways UL), which has allready
    int             originy;       // accounted  for the patch's internal origin
    int             patch;
} texpatch_t;

// a maptexturedef_t describes a rectangular texture, which is composed of one
// or more mappatch_t structures that arrange graphic patches
typedef struct {
    char            name[8];       // for switch changing, etc
    short           width;
    short           height;
    short           patchcount;
    texpatch_t      patches[1];    // [patchcount] drawn back to front
    //  into the cached texture
    // Extra stuff. -jk
    boolean         masked;        // from maptexture_t
} texture_t;

extern fixed_t *textureheight;     // needed for texture pegging
extern fixed_t *spritewidth;       // needed for pre rendering (fracs)
extern fixed_t *spriteoffset;
extern fixed_t *spritetopoffset;
extern int      viewwidth, scaledviewwidth, viewheight;

#define firstflat       gi.Get(DD_FIRSTFLAT)
#define numflats        gi.Get(DD_NUMFLATS)

extern int      firstspritelump, lastspritelump, numspritelumps;

//extern boolean LevelUseFullBright;

byte           *R_GetColumn(int tex, int col);
void            R_InitData(void);
void            R_UpdateData(void);

//void R_PrecacheLevel (void);

extern fixed_t  pspritescale, pspriteiscale;

//=============================================================================
//
// R_draw.c
//
//=============================================================================

extern byte    *translationtables;
extern byte    *dc_translation;

void            R_InitBuffer(int width, int height);
void            R_InitTranslationTables(void);
void            R_UpdateTranslationTables(void);

//#pragma pack()

#endif                          // __R_LOCAL__
