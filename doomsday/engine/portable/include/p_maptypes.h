/* Generated by ../../engine/scripts/makedmt.py */

#ifndef __DOOMSDAY_PLAY_MAP_DATA_TYPES_H__
#define __DOOMSDAY_PLAY_MAP_DATA_TYPES_H__

#include "p_mapdata.h"

#define LO_prev     link[0]
#define LO_next     link[1]

typedef struct lineowner_s {
    struct line_s *line;
    struct lineowner_s *link[2];    // {prev, next} (i.e. {anticlk, clk}).
    binangle_t      angle;          // between this and next clockwise.
} lineowner_t;

#define V_pos                   v.pos

typedef struct mvertex_s {
    // Vertex index. Always valid after loading and pruning of unused
    // vertices has occurred.
    int         index;

    // Reference count. When building normal node info, unused vertices
    // will be pruned.
    int         refCount;

    // Usually NULL, unless this vertex occupies the same location as a
    // previous vertex. Only used during the pruning phase.
    struct vertex_s *equiv;

    struct edgetip_s *tipSet; // Set of wall_tips.

// Final data.
    double      pos[2];
} mvertex_t;

typedef struct vertex_s {
    runtime_mapdata_header_t header;
    unsigned int        numlineowners; // Number of line owners.
    boolean             anchored;      // One or more of our line owners are one-sided.
    lineowner_t*        lineowners;    // Lineowner base ptr [numlineowners] size. A doubly, circularly linked list. The base is the line with the lowest angle and the next-most with the largest angle.
    fvertex_t           v;
    mvertex_t           buildData;
} vertex_t;

// Helper macros for accessing seg data elements.
#define FRONT 0
#define BACK  1

#define SG_v(n)                 v[(n)]
#define SG_vpos(n)              SG_v(n)->V_pos

#define SG_v1                   SG_v(0)
#define SG_v1pos                SG_v(0)->V_pos

#define SG_v2                   SG_v(1)
#define SG_v2pos                SG_v(1)->V_pos

#define SG_sector(n)            sec[(n)]
#define SG_frontsector          SG_sector(FRONT)
#define SG_backsector           SG_sector(BACK)

// Seg flags
#define SEGF_POLYOBJ            0x1 // Seg is part of a poly object.

// Seg frame flags
#define SEGINF_FACINGFRONT      0x0001
#define SEGINF_BACKSECSKYFIX    0x0002

typedef struct seg_s {
    runtime_mapdata_header_t header;
    struct vertex_s*    v[2];          // [Start, End] of the segment.
    struct side_s*      sidedef;
    struct line_s*      linedef;
    struct sector_s*    sec[2];
    struct subsector_s* subsector;
    struct seg_s*       backseg;
    angle_t             angle;
    byte                side;          // 0=front, 1=back
    byte                flags;
    float               length;        // Accurate length of the segment (v1 -> v2).
    float               offset;
    unsigned int        updated;
    struct biasaffection_s affected[MAX_BIAS_AFFECTED];
    struct biastracker_s tracker[3];   // 0=middle, 1=top, 2=bottom
    struct vertexillum_s illum[3][4];
    short               frameflags;
} seg_t;

#define SUBF_MIDPOINT         0x80    // Midpoint is tri-fan centre.

typedef struct msubsec_s {
    // Approximate middle point.
    double      midPoint[2];

    // Subsector index. Always valid, set when the subsector is
    // initially created.
    int         index;

    int         hEdgeCount;
    struct hedge_s *hEdges; // Head ptr to a list of half-edges in this subsector.
} msubsec_t;

typedef struct subsector_s {
    runtime_mapdata_header_t header;
    unsigned int        segcount;
    struct seg_s**      segs;          // [segcount] size.
    struct polyobj_s*   poly;          // NULL, if there is no polyobj.
    struct sector_s*    sector;
    int                 flags;
    int                 validCount;
    unsigned int        group;
    unsigned int        reverb[NUM_REVERB_DATA];
    fvertex_t           bbox[2];       // Min and max points.
    fvertex_t           midpoint;      // Center of vertices.
    struct subplaneinfo_s** planes;
    unsigned short      numvertices;
    struct fvertex_s**  vertices;      // [numvertices] size
    struct shadowlink_s* shadows;
    msubsec_t           buildData;
} subsector_t;

// Surface flags.
#define SUF_TEXFIX      0x1         // Current texture is a fix replacement
                                    // (not sent to clients, returned via DMU etc).
#define SUF_GLOW        0x2         // Surface glows (full bright).
#define SUF_BLEND       0x4         // Surface possibly has a blended texture.
#define SUF_NO_RADIO    0x8         // No fakeradio for this surface.

#define SUF_UPDATE_FLAG_MASK    0xff000000
#define SUF_UPDATE_DECORATIONS  0x80000000

// Surface frame flags
#define SUFINF_PVIS     0x0001

typedef struct surfacedecor_s {
    float           pos[3];         // World coordinates of the decoration.
    struct ded_decorlight_s *def;
} surfacedecor_t;

typedef struct surface_s {
    runtime_mapdata_header_t header;
    int                 flags;         // SUF_ flags
    int                 oldflags;
    struct material_s   *material;
    struct material_s   *oldmaterial;
    blendmode_t         blendmode;
    float               normal[3];     // Surface normal
    float               oldnormal[3];
    float               offset[2];     // [X, Y] Planar offset to surface material origin.
    float               oldoffset[2];
    float               rgba[4];       // Surface color tint
    float               oldrgba[4];
    short               frameflags;
    surfacedecor_t      decorations[MAX_SURFACE_DECORATIONS];
} surface_t;

typedef enum {
    PLN_FLOOR,
    PLN_CEILING,
    NUM_PLANE_TYPES
} planetype_t;

typedef struct skyfix_s {
    float offset;
} skyfix_t;

#define PS_normal               surface.normal
#define PS_material             surface.material
#define PS_offset               surface.offset
#define PS_rgba                 surface.rgba

typedef struct plane_s {
    runtime_mapdata_header_t header;
    degenmobj_t         soundorg;      // Sound origin for plane
    struct sector_s*    sector;        // Owner of the plane (temp)
    surface_t           surface;
    float               height;        // Current height
    float               oldheight[2];
    float               glow;          // Glow amount
    float               glowrgb[3];    // Glow color
    float               target;        // Target height
    float               speed;         // Move speed
    float               visheight;     // Visible plane height (smoothed)
    float               visoffset;
} plane_t;

// Helper macros for accessing sector floor/ceiling plane data elements.
#define SP_plane(n)             planes[(n)]

#define SP_planesurface(n)      SP_plane(n)->surface
#define SP_planeheight(n)       SP_plane(n)->height
#define SP_planenormal(n)       SP_plane(n)->surface.normal
#define SP_planematerial(n)     SP_plane(n)->surface.material
#define SP_planeoffset(n)       SP_plane(n)->surface.offset
#define SP_planergb(n)          SP_plane(n)->surface.rgba
#define SP_planeglow(n)         SP_plane(n)->glow
#define SP_planeglowrgb(n)      SP_plane(n)->glowrgb
#define SP_planetarget(n)       SP_plane(n)->target
#define SP_planespeed(n)        SP_plane(n)->speed
#define SP_planesoundorg(n)     SP_plane(n)->soundorg
#define SP_planevisheight(n)    SP_plane(n)->visheight

#define SP_ceilsurface          SP_planesurface(PLN_CEILING)
#define SP_ceilheight           SP_planeheight(PLN_CEILING)
#define SP_ceilnormal           SP_planenormal(PLN_CEILING)
#define SP_ceilmaterial         SP_planematerial(PLN_CEILING)
#define SP_ceiloffset           SP_planeoffset(PLN_CEILING)
#define SP_ceilrgb              SP_planergb(PLN_CEILING)
#define SP_ceilglow             SP_planeglow(PLN_CEILING)
#define SP_ceilglowrgb          SP_planeglowrgb(PLN_CEILING)
#define SP_ceiltarget           SP_planetarget(PLN_CEILING)
#define SP_ceilspeed            SP_planespeed(PLN_CEILING)
#define SP_ceilsoundorg         SP_planesoundorg(PLN_CEILING)
#define SP_ceilvisheight        SP_planevisheight(PLN_CEILING)

#define SP_floorsurface         SP_planesurface(PLN_FLOOR)
#define SP_floorheight          SP_planeheight(PLN_FLOOR)
#define SP_floornormal          SP_planenormal(PLN_FLOOR)
#define SP_floormaterial        SP_planematerial(PLN_FLOOR)
#define SP_flooroffset          SP_planeoffset(PLN_FLOOR)
#define SP_floorrgb             SP_planergb(PLN_FLOOR)
#define SP_floorglow            SP_planeglow(PLN_FLOOR)
#define SP_floorglowrgb         SP_planeglowrgb(PLN_FLOOR)
#define SP_floortarget          SP_planetarget(PLN_FLOOR)
#define SP_floorspeed           SP_planespeed(PLN_FLOOR)
#define SP_floorsoundorg        SP_planesoundorg(PLN_FLOOR)
#define SP_floorvisheight       SP_planevisheight(PLN_FLOOR)

#define S_skyfix(n)             skyfix[(n)]
#define S_floorskyfix           S_skyfix(PLN_FLOOR)
#define S_ceilskyfix            S_skyfix(PLN_CEILING)

// Sector frame flags
#define SIF_VISIBLE         0x1     // Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1     // Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

// Sector flags.
#define SECF_PERMANENTLINK  0x1
#define SECF_UNCLOSED       0x2     // An unclosed sector (some sort of fancy hack).
#define SECF_SELFREFHACK    0x4     // A self-referencing hack sector which ISNT enclosed by the sector referenced. Bounding box for the sector.

typedef struct ssecgroup_s {
    struct sector_s**   linked;     // [sector->planecount+1] size.
                                    // Plane attached to another sector.
} ssecgroup_t;

typedef struct msector_s {
    // Sector index. Always valid after loading & pruning.
    int         index;

    // Suppress superfluous mini warnings.
    int         warnedFacing;
    boolean     warnedUnclosed;
} msector_t;

typedef struct sector_s {
    runtime_mapdata_header_t header;
    int                 frameflags;
    int                 addspritecount; // frame number of last R_AddSprites
    int                 validCount;    // if == validCount, already checked.
    int                 flags;
    skyfix_t            skyfix[2];     // floor, ceiling.
    float               bbox[4];       // Bounding box for the sector
    float               lightlevel;
    float               oldlightlevel;
    float               rgb[3];
    float               oldrgb[3];
    struct mobj_s*      mobjList;      // List of mobjs in the sector.
    unsigned int        linecount;
    struct line_s**     Lines;         // [linecount+1] size.
    unsigned int        subscount;
    struct subsector_s** subsectors;   // [subscount+1] size.
    unsigned int        numReverbSSecAttributors;
    struct subsector_s** reverbSSecs;  // [numReverbSSecAttributors] size.
    unsigned int        subsgroupcount;
    ssecgroup_t*        subsgroups;    // [subsgroupcount+1] size.
    degenmobj_t         soundorg;
    unsigned int        planecount;
    struct plane_s**    planes;        // [planecount+1] size.
    struct sector_s*    containsector; // Sector that contains this (if any).
    struct sector_s*    lightsource;   // Main sky light source
    unsigned int        blockcount;    // Number of gridblocks in the sector.
    unsigned int        changedblockcount; // Number of blocks to mark changed.
    unsigned short*     blocks;        // Light grid block indices.
    float               reverb[NUM_REVERB_DATA];
    msector_t           buildData;
} sector_t;

// Parts of a wall segment.
typedef enum segsection_e {
    SEG_MIDDLE,
    SEG_TOP,
    SEG_BOTTOM
} segsection_t;

// Helper macros for accessing sidedef top/middle/bottom section data elements.
#define SW_surface(n)           sections[(n)]
#define SW_surfaceflags(n)      SW_surface(n).flags
#define SW_surfacematerial(n)   SW_surface(n).material
#define SW_surfacenormal(n)     SW_surface(n).normal
#define SW_surfaceoffset(n)     SW_surface(n).offset
#define SW_surfacergba(n)       SW_surface(n).rgba
#define SW_surfaceblendmode(n)  SW_surface(n).blendmode

#define SW_middlesurface        SW_surface(SEG_MIDDLE)
#define SW_middleflags          SW_surfaceflags(SEG_MIDDLE)
#define SW_middlematerial       SW_surfacematerial(SEG_MIDDLE)
#define SW_middlenormal         SW_surfacenormal(SEG_MIDDLE)
#define SW_middletexmove        SW_surfacetexmove(SEG_MIDDLE)
#define SW_middleoffset         SW_surfaceoffset(SEG_MIDDLE)
#define SW_middlergba           SW_surfacergba(SEG_MIDDLE)
#define SW_middleblendmode      SW_surfaceblendmode(SEG_MIDDLE)

#define SW_topsurface           SW_surface(SEG_TOP)
#define SW_topflags             SW_surfaceflags(SEG_TOP)
#define SW_topmaterial          SW_surfacematerial(SEG_TOP)
#define SW_topnormal            SW_surfacenormal(SEG_TOP)
#define SW_toptexmove           SW_surfacetexmove(SEG_TOP)
#define SW_topoffset            SW_surfaceoffset(SEG_TOP)
#define SW_toprgba              SW_surfacergba(SEG_TOP)

#define SW_bottomsurface        SW_surface(SEG_BOTTOM)
#define SW_bottomflags          SW_surfaceflags(SEG_BOTTOM)
#define SW_bottommaterial       SW_surfacematerial(SEG_BOTTOM)
#define SW_bottomnormal         SW_surfacenormal(SEG_BOTTOM)
#define SW_bottomtexmove        SW_surfacetexmove(SEG_BOTTOM)
#define SW_bottomoffset         SW_surfaceoffset(SEG_BOTTOM)
#define SW_bottomrgba           SW_surfacergba(SEG_BOTTOM)

// Sidedef flags
#define SDF_BLENDTOPTOMID       0x01
#define SDF_BLENDMIDTOTOP       0x02
#define SDF_BLENDMIDTOBOTTOM    0x04
#define SDF_BLENDBOTTOMTOMID    0x08

#define FRONT                   0
#define BACK                    1

typedef struct msidedef_s {
    // Sidedef index. Always valid after loading & pruning.
    int         index;
} msidedef_t;

typedef struct side_s {
    runtime_mapdata_header_t header;
    surface_t           sections[3];
    unsigned int        segcount;
    struct seg_s**      segs;          // [segcount] size, segs arranged left>right
    struct sector_s*    sector;
    short               flags;
    msidedef_t          buildData;
    int                 fakeRadioUpdateCount; // frame number of last update
    shadowcorner_t      topCorners[2];
    shadowcorner_t      bottomCorners[2];
    shadowcorner_t      sideCorners[2];
    edgespan_t          spans[2];      // [left, right]
} side_t;

// Helper macros for accessing linedef data elements.
#define L_v(n)                  v[(n)]
#define L_vpos(n)               v[(n)]->V_pos

#define L_v1                    L_v(0)
#define L_v1pos                 L_v(0)->V_pos

#define L_v2                    L_v(1)
#define L_v2pos                 L_v(1)->V_pos

#define L_vo(n)                 vo[(n)]
#define L_vo1                   L_vo(0)
#define L_vo2                   L_vo(1)

#define L_side(n)               sides[(n)]
#define L_frontside             L_side(FRONT)
#define L_backside              L_side(BACK)
#define L_sector(n)             sides[(n)]->sector
#define L_frontsector           L_sector(FRONT)
#define L_backsector            L_sector(BACK)

// Line flags
#define LINEF_SELFREF           0x1 // Front and back sectors of this line are the same.
#define LINEF_POLYOBJ           0x2 // Line is part of a polyobject.

#define MLF_TWOSIDED            0x1 // Line is marked two-sided.
#define MLF_ZEROLENGTH          0x2 // Zero length (line should be totally ignored).
#define MLF_SELFREF             0x4 // Sector is the same on both sides.
#define MLF_POLYOBJ             0x8 // Line is part of a polyobj.

typedef struct mlinedef_s {
    // Linedef index. Always valid after loading & pruning of zero
    // length lines has occurred.
    int         index;
    int         mlFlags; // MLF_* flags.

    // One-sided linedef used for a special effect (windows).
    // The value refers to the opposite sector on the back side.
    struct sector_s *windowEffect;

    // Normally NULL, except when this linedef directly overlaps an earlier
    // one (a rarely-used trick to create higher mid-masked textures).
    // No segs should be created for these overlapping linedefs.
    struct line_s *overlap;
} mlinedef_t;

typedef struct line_s {
    runtime_mapdata_header_t header;
    struct vertex_s*    v[2];
    struct lineowner_s* vo[2];         // Links to vertex line owner nodes [left, right]
    struct side_s*      sides[2];
    int                 flags;
    slopetype_t         slopetype;
    int                 validCount;
    short               mapflags;      // MF_* flags, read from the LINEDEFS, map data lump.
    binangle_t          angle;         // Calculated from front side's normal
    float               dx;
    float               dy;
    float               length;        // Accurate length
    float               bbox[4];
    boolean             mapped[DDMAXPLAYERS]; // Whether the line has been mapped by each player yet.
    mlinedef_t          buildData;
} line_t;

typedef struct mpolyobj_s {
    int         index;
    uint        lineCount;
    struct line_s **lines;
} mpolyobj_t;

typedef struct polyobj_s {
    runtime_mapdata_header_t header;
    vec2_t              box[2];
    int                 validCount;
    int                 tag;           // Reference tag assigned in HereticEd
    angle_t             angle;
    unsigned int        idx;           // Idx of polyobject
    unsigned int        numsegs;
    struct seg_s**      segs;
    fvertex_t*          originalPts;   // Used as the base for the rotations
    fvertex_t*          prevPts;       // Use to restore the old point values
    fvertex_t           dest;          // Destination XY
    float               speed;         // Movement speed.
    angle_t             destAngle;     // Destination angle.
    angle_t             angleSpeed;    // Rotation speed.
    boolean             crush;         // Should the polyobj attempt to crush mobjs?
    int                 seqType;
    void*               specialdata;   // pointer a thinker, if the poly is moving
    degenmobj_t         startSpot;
    mpolyobj_t          buildData;
} polyobj_t;

typedef struct child_s {
    // Child node or subsector (one must be NULL).
    struct node_s *node;
    subsector_t  *subSec;
} child_t;

#define RIGHT                   0
#define LEFT                    1

typedef struct mnode_s {
    // Node index. Only valid once the NODES or GL_NODES lump has been
    // created.
    int         index;

    // The node is too long, and the (dx,dy) values should be halved
    // when writing into the NODES lump.
    boolean     tooLong;

// Final data.
    child_t     children[2]; // Children {RIGHT, LEFT}
} mnode_t;

typedef struct node_s {
    runtime_mapdata_header_t header;
    float               x;             // Partition line.
    float               y;             // Partition line.
    float               dx;            // Partition line.
    float               dy;            // Partition line.
    float               bbox[2][4];    // Bounding box for each child.
    unsigned int        children[2];   // If NF_SUBSECTOR it's a subsector.
    mnode_t             buildData;
} node_t;

#endif
