# $Id$
# Runtime map data defitions. Processed by the makedmt.py script.

public
#define DMT_VERTEX_POS  DDVT_FLOAT
end

internal
#define LO_prev     link[0]
#define LO_next     link[1]

typedef struct lineowner_s {
    struct linedef_s *lineDef;
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
end

struct vertex
    -       uint        numLineOwners // Number of line owners.
    -       boolean     anchored    // One or more of our line owners are one-sided.
    -       lineowner_t* lineOwners // Lineowner base ptr [numlineowners] size. A doubly, circularly linked list. The base is the line with the lowest angle and the next-most with the largest angle.
    -       fvertex_t   v
    -       mvertex_t   buildData
end

internal
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

#define SEG_SIDEDEF(s)          ((s)->lineDef->sideDefs[(s)->side])

// Seg flags
#define SEGF_POLYOBJ            0x1 // Seg is part of a poly object.

// Seg frame flags
#define SEGINF_FACINGFRONT      0x0001
#define SEGINF_BACKSECSKYFIX    0x0002
end

public
#define DMT_SEG_SIDEDEF         DDVT_PTR
end

struct seg
    PTR     vertex_s*[2] v          // [Start, End] of the segment.
    PTR     linedef_s*  lineDef
    PTR     sector_s*[2] sec
    PTR     subsector_s* subsector
    PTR     seg_s*      backSeg
    ANGLE   angle_t     angle
    BYTE    byte        side        // 0=front, 1=back
    BYTE    byte        flags
    FLOAT   float       length      // Accurate length of the segment (v1 -> v2).
    FLOAT   float       offset
    -       uint        updated
    -       biasaffection_s[MAX_BIAS_AFFECTED] affected
    -       biastracker_s[3] tracker // 0=middle, 1=top, 2=bottom
    -       vertexillum_s[3][4] illum
    -       short       frameFlags
end

internal
#define SUBF_MIDPOINT         0x80    // Midpoint is tri-fan centre.
end

struct subsector
    UINT    uint        segCount
    PTR     seg_s**     segs        // [segcount] size.
    PTR     polyobj_s*  polyObj     // NULL, if there is no polyobj.
    PTR     sector_s*   sector
    -       int         flags
    -       int         validCount
    -       uint        group
    -       uint[NUM_REVERB_DATA] reverb
    -       fvertex_t   bBox[2]     // Min and max points.
    -       fvertex_t   midPoint    // Center of vertices.
    -       subplaneinfo_s** planes
    -       ushort      numVertices
    -       fvertex_s** vertices    // [numvertices] size
    -       shadowlink_s* shadows
end

public
#define DMT_MATERIAL DDVT_SHORT
end

internal
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
end

struct surface
    INT     int         flags       // SUF_ flags
    -       int         oldFlags
    -       material_s *material
    -       material_s *oldMaterial
    BLENDMODE blendmode_t blendMode
    -       float[3]    normal      // Surface normal
    -       float[3]    oldNormal
    FLOAT   float[2]    offset      // [X, Y] Planar offset to surface material origin.
    -       float[2]    oldOffset
    FLOAT   float[4]    rgba        // Surface color tint
    -       float[4]    oldRGBA
    -       short       frameFlags
    -       uint        numDecorations
    -       surfacedecor_t *decorations
end

internal
typedef enum {
    PLN_FLOOR,
    PLN_CEILING,
    NUM_PLANE_TYPES
} planetype_t;

typedef struct skyfix_s {
    float offset;
} skyfix_t;
end

internal
#define PS_normal               surface.normal
#define PS_material             surface.material
#define PS_offset               surface.offset
#define PS_rgba                 surface.rgba
end

struct plane
    PTR     degenmobj_t soundOrg    // Sound origin for plane
    PTR     sector_s*   sector      // Owner of the plane (temp)
    -       surface_t   surface
    FLOAT   float       height      // Current height
    -       float[2]    oldHeight
    FLOAT   float       glow        // Glow amount
    FLOAT   float[3]    glowRGB     // Glow color
    FLOAT   float       target      // Target height
    FLOAT   float       speed       // Move speed
    -       float       visHeight   // Visible plane height (smoothed)
    -       float       visOffset
end

internal
// Helper macros for accessing sector floor/ceiling plane data elements.
#define SP_plane(n)             planes[(n)]

#define SP_planesurface(n)      SP_plane(n)->surface
#define SP_planeheight(n)       SP_plane(n)->height
#define SP_planenormal(n)       SP_plane(n)->surface.normal
#define SP_planematerial(n)     SP_plane(n)->surface.material
#define SP_planeoffset(n)       SP_plane(n)->surface.offset
#define SP_planergb(n)          SP_plane(n)->surface.rgba
#define SP_planeglow(n)         SP_plane(n)->glow
#define SP_planeglowrgb(n)      SP_plane(n)->glowRGB
#define SP_planetarget(n)       SP_plane(n)->target
#define SP_planespeed(n)        SP_plane(n)->speed
#define SP_planesoundorg(n)     SP_plane(n)->soundOrg
#define SP_planevisheight(n)    SP_plane(n)->visHeight

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

#define S_skyfix(n)             skyFix[(n)]
#define S_floorskyfix           S_skyfix(PLN_FLOOR)
#define S_ceilskyfix            S_skyfix(PLN_CEILING)
end

internal
// Sector frame flags
#define SIF_VISIBLE         0x1     // Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1     // Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

// Sector flags.
#define SECF_PERMANENTLINK  0x1
#define SECF_UNCLOSED       0x2     // An unclosed sector (some sort of fancy hack).
#define SECF_SELFREFHACK    0x4     // A self-referencing hack sector which ISNT enclosed by the sector referenced. Bounding box for the sector.

typedef struct ssecgroup_s {
    struct sector_s**   linked;     // [sector->planeCount+1] size.
                                    // Plane attached to another sector.
} ssecgroup_t;

typedef struct msector_s {
    // Sector index. Always valid after loading & pruning.
    int         index;

    // Suppress superfluous mini warnings.
    int         warnedFacing;
} msector_t;
end

struct sector
    -       int         frameFlags
    -       int         addSpriteCount // frame number of last R_AddSprites
    INT     int         validCount  // if == validCount, already checked.
    -       int         flags
    -       skyfix_t[2] skyFix      // floor, ceiling.
    -       float[4]    bBox        // Bounding box for the sector
    FLOAT   float       lightLevel
    -       float       oldLightLevel
    FLOAT   float[3]    rgb
    -       float[3]    oldRGB
    PTR     mobj_s*     mobjList    // List of mobjs in the sector.
    UINT    uint        lineDefCount
    PTR     linedef_s** lineDefs       // [lineDefCount+1] size.
    UINT    uint        ssectorCount
    PTR     subsector_s** ssectors // [ssectorCount+1] size.
    -       uint        numReverbSSecAttributors
    -       subsector_s** reverbSSecs // [numReverbSSecAttributors] size.
    -       uint        subsGroupCount
    -       ssecgroup_t* subsGroups // [subsGroupCount+1] size.
    PTR     degenmobj_t soundOrg
    UINT    uint        planeCount
    -       plane_s**   planes      // [planeCount+1] size.
    -       sector_s*   containSector // Sector that contains this (if any).
    -       sector_s*   lightSource // Main sky light source
    -       uint        blockCount  // Number of gridblocks in the sector.
    -       uint        changedBlockCount // Number of blocks to mark changed.
    -       ushort*     blocks      // Light grid block indices.
    FLOAT   float[NUM_REVERB_DATA] reverb
    -       msector_t   buildData
end

internal
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
#define SW_surfaceblendmode(n)  SW_surface(n).blendMode

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
end

internal
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
end

struct sidedef
    -       surface_t[3] sections
    UINT    uint        segCount
    PTR     seg_s**     segs        // [segcount] size, segs arranged left>right
    PTR     sector_s*   sector
    SHORT   short       flags
    -       msidedef_t  buildData

# The following is used with FakeRadio.
    -       int         fakeRadioUpdateCount // frame number of last update
    -       shadowcorner_t[2] topCorners
    -       shadowcorner_t[2] bottomCorners
    -       shadowcorner_t[2] sideCorners
    -       edgespan_t[2] spans // [left, right]
end

internal
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

#define L_side(n)               sideDefs[(n)]
#define L_frontside             L_side(FRONT)
#define L_backside              L_side(BACK)
#define L_sector(n)             sideDefs[(n)]->sector
#define L_frontsector           L_sector(FRONT)
#define L_backsector            L_sector(BACK)

// Is this line self-referencing (front sec == back sec)? 
#define LINE_SELFREF(l)			((l)->L_frontside && (l)->L_backside && \
								 (l)->L_frontsector == (l)->L_backsector)

// Internal flags:
#define LF_POLYOBJ				0x1 // Line is part of a polyobject.
end

internal
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
    struct linedef_s *overlap;
} mlinedef_t;
end

public
#define DMT_LINEDEF_SEC    DDVT_PTR
end

struct linedef
    PTR     vertex_s*[2] v
    -       lineowner_s*[2] vo      // Links to vertex line owner nodes [left, right]
    PTR     sidedef_s*[2] sideDefs
    INT     int         flags		// Public DDLF_* flags.
    -		byte		inFlags		// Internal LF_* flags
    INT     slopetype_t slopeType
    INT     int         validCount
    -       binangle_t  angle       // Calculated from front side's normal
    FLOAT   float       dX
    FLOAT   float       dY
    -       float       length      // Accurate length
    FLOAT   float[4]    bBox
    -       boolean[DDMAXPLAYERS] mapped // Whether the line has been mapped by each player yet.
    -       mlinedef_t  buildData
end

internal
typedef struct mpolyobj_s {
    int         index;
    uint        lineCount;
    struct linedef_s **lineDefs;
} mpolyobj_t;
end

struct polyobj
    UINT    uint        idx         // Idx of polyobject
    INT     int         tag         // Reference tag assigned in HereticEd
    INT     int         validCount
    FLOAT   vec2_t[2]   box
    PTR     degenmobj_t startSpot
    -       fvertex_t   dest        // Destination XY
    ANGLE   angle_t     angle
    ANGLE   angle_t     destAngle   // Destination angle.
    ANGLE   angle_t     angleSpeed  // Rotation speed.
    UINT    uint        numSegs
    PTR     seg_s**     segs
    -       fvertex_t*  originalPts // Used as the base for the rotations
    -       fvertex_t*  prevPts     // Use to restore the old point values
    FLOAT   float       speed       // Movement speed.
    BOOL    boolean     crush       // Should the polyobj attempt to crush mobjs?
    INT     int         seqType
    PTR     void*       specialData // pointer a thinker, if the poly is moving
    -       mpolyobj_t  buildData
end

internal
#define RIGHT                   0
#define LEFT                    1

/**
 * An infinite line of the form point + direction vectors. 
 */
typedef struct partition_s {
	float				x, y;
	float				dX, dY;
} partition_t;
end

struct node
    -       partition_t partition
    FLOAT   float[2][4] bBox // Bounding box for each child.
    UINT    uint[2]     children // If NF_SUBSECTOR it's a subsector.
end
