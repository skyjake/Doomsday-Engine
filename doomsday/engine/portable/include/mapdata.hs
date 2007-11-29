# $Id$
# Runtime map data defitions. Processed by the makedmt.py script.

public
#define DMT_VERTEX_POS  DDVT_FLOAT
end

internal
#define LO_prev     link[0]
#define LO_next     link[1]

typedef struct lineowner_s {
    struct line_s *line;
    struct lineowner_s *link[2];    // {prev, next} (i.e. {anticlk, clk}).
    binangle_t      angle;          // between this and next clockwise.
} lineowner_t;

#define V_pos                   v.pos
end

struct vertex
    -       fvertex_t   v
    -       uint        numlineowners // Number of line owners.
    -       lineowner_t* lineowners // Lineowner base ptr [numlineowners] size. A doubly, circularly linked list. The base is the line with the lowest angle and the next-most with the largest angle.
    -       boolean     anchored    // One or more of our line owners are one-sided.
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

// Seg flags
#define SEGF_POLYOBJ            0x1 // Seg is part of a poly object.

// Seg frame flags
#define SEGINF_FACINGFRONT      0x0001
#define SEGINF_BACKSECSKYFIX    0x0002
end

struct seg
    PTR     vertex_s*[2] v          // [Start, End] of the segment.
    FLOAT   float       length      // Accurate length of the segment (v1 -> v2).
    FLOAT   float       offset
    PTR     side_s*     sidedef
    PTR     line_s*     linedef
    PTR     sector_s*[2] sec
    PTR     subsector_s* subsector
    PTR     seg_s*      backseg
    ANGLE   angle_t     angle
    BYTE    byte        side        // 0=front, 1=back
    BYTE    byte        flags
    -       short       frameflags
    -       biastracker_s[3] tracker // 0=middle, 1=top, 2=bottom
    -       vertexillum_s[3][4] illum
    -       uint        updated
    -       biasaffection_s[MAX_BIAS_AFFECTED] affected
end

internal
#define SUBF_MIDPOINT         0x80    // Midpoint is tri-fan centre.
end

struct subsector
    PTR     sector_s*   sector
    UINT    uint        segcount
    PTR     seg_s**     segs        // [segcount] size.
    PTR     polyobj_s*  poly        // NULL, if there is no polyobj.
    -       int         flags
    -       fvertex_t   bbox[2]     // Min and max points.
    -       fvertex_t   midpoint    // Center of vertices.
    -       subplaneinfo_s** planes
    -       ushort      numvertices
    -       fvertex_s** vertices    // [numvertices] size
    -       int         validCount
    -       shadowlink_s* shadows
    -       uint        group
    -       uint[NUM_REVERB_DATA] reverb
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

// Surface frame flags
#define SUFINF_PVIS     0x0001
end

struct surface
    INT     int         flags       // SUF_ flags
    -       int         oldflags
    -       material_s *material
    -       material_s *oldmaterial
    BLENDMODE blendmode_t blendmode
    -       float[3]    normal      // Surface normal
    -       float[3]    oldnormal
    FLOAT   float[2]    offset      // [X, Y] Planar offset to surface material origin.
    -       float[2]    oldoffset
    FLOAT   float[4]    rgba        // Surface color tint
    -       float[4]    oldrgba
    -       short       frameflags
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
end

struct plane
    FLOAT   float       height      // Current height
    -       float[2]    oldheight
    -       surface_t   surface
    FLOAT   float       glow        // Glow amount
    FLOAT   float[3]    glowrgb     // Glow color
    FLOAT   float       target      // Target height
    FLOAT   float       speed       // Move speed
    PTR     degenmobj_t soundorg    // Sound origin for plane
    PTR     sector_s*   sector      // Owner of the plane (temp)
    -       float       visheight   // Visible plane height (smoothed)
    -       float       visoffset
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
end

internal
// Sector frame flags
#define SIF_VISIBLE         0x1     // Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1     // Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

// Sector flags.
#define SECF_INVIS_FLOOR    0x1
#define SECF_INVIS_CEILING  0x2

typedef struct ssecgroup_s {
    struct sector_s**   linked;     // [sector->planecount+1] size.
                                    // Plane attached to another sector.
} ssecgroup_t;
end

struct sector
    FLOAT   float       lightlevel
    -       float       oldlightlevel
    FLOAT   float[3]    rgb
    -       float[3]    oldrgb
    INT     int         validCount  // if == validCount, already checked.
    PTR     mobj_s*     mobjList    // List of mobjs in the sector.
    UINT    uint        linecount
    PTR     line_s**    Lines       // [linecount+1] size.
    PTR     subsector_s** subsectors // [subscount+1] size.
    -       uint        numReverbSSecAttributors
    -       subsector_s** reverbSSecs // [numReverbSSecAttributors] size.
    -       uint        subsgroupcount
    -       ssecgroup_t* subsgroups // [subsgroupcount+1] size.
    -       skyfix_t[2] skyfix      // floor, ceiling.
    PTR     degenmobj_t soundorg
    FLOAT   float[NUM_REVERB_DATA] reverb
    -       uint[4]     blockbox    // Mapblock bounding box.
    UINT    uint        planecount
    -       plane_s**   planes      // [planecount+1] size.
    -       sector_s*   containsector // Sector that contains this (if any).
    -       boolean     permanentlink
    -       boolean     unclosed    // An unclosed sector (some sort of fancy hack).
    -       boolean     selfRefHack // A self-referencing hack sector which ISNT enclosed by the sector referenced. Bounding box for the sector.
    -       float[4]    bbox        // Bounding box for the sector
    -       int         frameflags
    -       int         addspritecount // frame number of last R_AddSprites
    -       sector_s*   lightsource // Main sky light source
    -       uint        blockcount  // Number of gridblocks in the sector.
    -       uint        changedblockcount // Number of blocks to mark changed.
    -       ushort*     blocks      // Light grid block indices.
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
end

internal
// Sidedef flags
#define SDF_BLENDTOPTOMID       0x01
#define SDF_BLENDMIDTOTOP       0x02
#define SDF_BLENDMIDTOBOTTOM    0x04
#define SDF_BLENDBOTTOMTOMID    0x08
end

struct side
    -       surface_t[3] sections
    UINT    uint        segcount
    PTR     seg_s**     segs        // [segcount] size, segs arranged left>right
    PTR     sector_s*   sector
    SHORT   short       flags

# The following is used with FakeRadio.
    -       int         fakeRadioUpdateCount // frame number of last update
    -       shadowcorner_t[2] topCorners
    -       shadowcorner_t[2] bottomCorners
    -       shadowcorner_t[2] sideCorners
    -       edgespan_t[2] spans // [left, right]
end

public
#define DMT_LINE_SEC    DDVT_PTR
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

#define L_side(n)               sides[(n)]
#define L_frontside             L_side(FRONT)
#define L_backside              L_side(BACK)
#define L_sector(n)             sides[(n)]->sector
#define L_frontsector           L_sector(FRONT)
#define L_backsector            L_sector(BACK)

// Line flags
#define LINEF_SELFREF           0x1 // Front and back sectors of this line are the same.
end

struct line
    PTR     vertex_s*[2] v
    -       int         flags
    SHORT   short       mapflags    // MF_* flags, read from the LINEDEFS, map data lump.
    FLOAT   float       dx
    FLOAT   float       dy
    INT     slopetype_t slopetype
    INT     int         validCount
    PTR     side_s*[2]  sides
    FLOAT   float[4]    bbox
    -       lineowner_s*[2] vo      // Links to vertex line owner nodes [left, right]
    -       float       length      // Accurate length
    -       binangle_t  angle       // Calculated from front side's normal
    -       boolean[DDMAXPLAYERS] mapped // Whether the line has been mapped by each player yet.
end

struct polyobj
    UINT    uint        numsegs
    PTR     seg_s**     segs
    INT     int         validCount
    PTR     degenmobj_t startSpot
    ANGLE   angle_t     angle
    INT     int         tag         // Reference tag assigned in HereticEd
    -       fvertex_t*  originalPts // Used as the base for the rotations
    -       fvertex_t*  prevPts     // Use to restore the old point values
    FLOAT   vec2_t[2]   box
    -       fvertex_t   dest        // Destination XY
    FLOAT   float       speed       // Movement speed.
    ANGLE   angle_t     destAngle   // Destination angle.
    ANGLE   angle_t     angleSpeed  // Rotation speed.
    BOOL    boolean     crush       // Should the polyobj attempt to crush mobjs?
    INT     int         seqType
    PTR     void*       specialdata // pointer a thinker, if the poly is moving
end

struct node
    FLOAT   float     x             // Partition line.
    FLOAT   float     y             // Partition line.
    FLOAT   float     dx            // Partition line.
    FLOAT   float     dy            // Partition line.
    FLOAT   float[2][4] bbox        // Bounding box for each child.
    UINT    uint[2]   children      // If NF_SUBSECTOR it's a subsector.
end
