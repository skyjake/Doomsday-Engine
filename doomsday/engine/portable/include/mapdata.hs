# $Id$
# Runtime map data defitions. Processed by the makedmt.py script.

struct vertex
    FLOAT   float[2]    pos
    -       uint        numsecowners // Number of sector owners.
    -       uint*       secowners // Sector indices [numsecowners] size.
    -       uint        numlineowners // Number of line owners.
    -       lineowner_s* lineowners // Lineowner base ptr [numlineowners] size. A doubly, circularly linked list. The base is the line with the lowest angle and the next-most with the largest angle.
    -       boolean     anchored // One or more of our line owners are one-sided.
end

internal
// Helper macros for accessing seg data elements.
#define FRONT 0
#define BACK  1

#define SG_v1                   v[0]
#define SG_v2                   v[1]
#define SG_frontsector          sec[FRONT]
#define SG_backsector           sec[BACK]

// Seg frame flags
#define SEGINF_FACINGFRONT      0x0001
#define SEGINF_BACKSECSKYFIX    0x0002
end

struct seg
    PTR     vertex_s*[2] v      // [Start, End] of the segment.
    FLOAT   float       length  // Accurate length of the segment (v1 -> v2).
    FLOAT   float       offset
    PTR     side_s*     sidedef
    PTR     line_s*     linedef
    PTR     sector_s*[2] sec
    ANGLE   angle_t     angle
    BYTE    byte        flags
    -       short       frameflags
    -       biastracker_s[3] tracker // 0=top, 1=middle, 2=bottom
    -       vertexillum_s[3][4] illum
    -       uint        updated
    -       biasaffection_s[MAX_BIAS_AFFECTED] affected
end

struct subsector
    PTR     sector_s*   sector
    UINT    uint        linecount
    UINT    uint        firstline
    PTR     polyobj_s*  poly    // NULL, if there is no polyobj.
    BYTE    byte        flags
    -       ushort      numverts
    -       fvertex_t*  verts   // A sorted list of edge vertices.
    -       fvertex_t   bbox[2] // Min and max points.
    -       fvertex_t   midpoint // Center of vertices.
    -       subplaneinfo_s** planes
    -       ushort      numvertices
    -       fvertex_s*  vertices
    -       int         validcount
    -       shadowlink_s* shadows
end

internal
// Surface flags.
#define SUF_TEXFIX      0x1         // Current texture is a fix replacement
                                    // (not sent to clients, returned via DMU etc).
#define SUF_GLOW        0x2         // Surface glows (full bright).
#define SUF_BLEND       0x4         // Surface possibly has a blended texture.
#define SUF_NO_RADIO    0x8         // No fakeradio for this surface.
end

struct surface
    INT     int         flags   // SUF_ flags
    -       int         oldflags
    SHORT   short       texture
    -       short       oldtexture
    -       boolean     isflat  // true if current texture is a flat
    -       boolean     oldisflat
    -       float[3]    normal  // Surface normal
    -       float[3]    oldnormal
    FLOAT   float[2]    texmove // Texture movement X and Y
    -       float[2]    oldtexmove
    FLOAT   float       offx	// Texture x offset
    -       float       oldoffx
    FLOAT   float       offy	// Texture y offset
    -       float       oldoffy
    BYTE    byte[4]     rgba    // Surface color tint
    -       byte[4]     oldrgba
    -       translation_s* xlat
end

struct plane
    FLOAT   float       height  // Current height
    -       float[2]    oldheight
    -       surface_t   surface
    FLOAT   float       glow    // Glow amount
    BYTE    byte[3]     glowrgb // Glow color
    FLOAT   float       target  // Target height
    FLOAT   float       speed   // Move speed
    PTR     degenmobj_t soundorg // Sound origin for plane
    PTR     sector_s*   sector  // Owner of the plane (temp)
    -       float       visheight // Visible plane height (smoothed)
    -       float       visoffset
    -       sector_s*   linked  // Plane attached to another sector.
end

internal
// Helper macros for accessing sector floor/ceiling plane data elements.
#define SP_ceilsurface          planes[PLN_CEILING]->surface
#define SP_ceilheight           planes[PLN_CEILING]->height
#define SP_ceilnormal           planes[PLN_CEILING]->normal
#define SP_ceilpic              planes[PLN_CEILING]->surface.texture
#define SP_ceilisflat           planes[PLN_CEILING]->surface.isflat
#define SP_ceiloffx             planes[PLN_CEILING]->surface.offx
#define SP_ceiloffy             planes[PLN_CEILING]->surface.offy
#define SP_ceilrgb              planes[PLN_CEILING]->surface.rgba
#define SP_ceilglow             planes[PLN_CEILING]->glow
#define SP_ceilglowrgb          planes[PLN_CEILING]->glowrgb
#define SP_ceiltarget           planes[PLN_CEILING]->target
#define SP_ceilspeed            planes[PLN_CEILING]->speed
#define SP_ceiltexmove          planes[PLN_CEILING]->surface.texmove
#define SP_ceilsoundorg         planes[PLN_CEILING]->soundorg
#define SP_ceilvisheight        planes[PLN_CEILING]->visheight
#define SP_ceillinked           planes[PLN_CEILING]->linked

#define SP_floorsurface         planes[PLN_FLOOR]->surface
#define SP_floorheight          planes[PLN_FLOOR]->height
#define SP_floornormal          planes[PLN_FLOOR]->normal
#define SP_floorpic             planes[PLN_FLOOR]->surface.texture
#define SP_floorisflat          planes[PLN_FLOOR]->surface.isflat
#define SP_flooroffx            planes[PLN_FLOOR]->surface.offx
#define SP_flooroffy            planes[PLN_FLOOR]->surface.offy
#define SP_floorrgb             planes[PLN_FLOOR]->surface.rgba
#define SP_floorglow            planes[PLN_FLOOR]->glow
#define SP_floorglowrgb         planes[PLN_FLOOR]->glowrgb
#define SP_floortarget          planes[PLN_FLOOR]->target
#define SP_floorspeed           planes[PLN_FLOOR]->speed
#define SP_floortexmove         planes[PLN_FLOOR]->surface.texmove
#define SP_floorsoundorg        planes[PLN_FLOOR]->soundorg
#define SP_floorvisheight       planes[PLN_FLOOR]->visheight
#define SP_floorlinked          planes[PLN_FLOOR]->linked

#define SECT_PLANE_HEIGHT(x, n) (x->planes[n]->visheight)
end

internal
// Sector frame flags
#define SIF_VISIBLE         0x1    // Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1    // Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

// Sector flags.
#define SECF_INVIS_FLOOR    0x1
#define SECF_INVIS_CEILING  0x2
end

struct sector
    SHORT   short       lightlevel
    -       int         oldlightlevel
    BYTE    byte[3]     rgb
    -       byte[3]     oldrgb
    INT     int         validcount  // if == validcount, already checked.
    PTR     mobj_s*     thinglist   // List of mobjs in the sector.
    UINT    uint        linecount   
    PTR     line_s**    Lines       // [linecount] size.
    UINT    uint        subscount
    PTR     subsector_s** subsectors // [subscount] size.
    -       skyfix_t[2] skyfix      // floor, ceiling.
    PTR     degenmobj_t soundorg

    FLOAT   float[NUM_REVERB_DATA] reverb
    -       int[4]      blockbox    // Mapblock bounding box.
    UINT    uint        planecount
    -       plane_s**   planes      // [planecount] size.
    -       sector_s*   containsector // Sector that contains this (if any).
    -       boolean     permanentlink
    -       boolean     unclosed    // An unclosed sector (some sort of fancy hack).
    -       boolean     selfRefHack // A self-referencing hack sector which ISNT enclosed by the sector referenced. Bounding box for the sector.
    -       float[4]    bounds      // Bounding box for the sector
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
#define SW_middlesurface        sections[SEG_MIDDLE]
#define SW_middleflags          sections[SEG_MIDDLE].flags
#define SW_middlepic            sections[SEG_MIDDLE].texture
#define SW_middleisflat         sections[SEG_MIDDLE].isflat
#define SW_middlenormal         sections[SEG_MIDDLE].normal
#define SW_middletexmove        sections[SEG_MIDDLE].texmove
#define SW_middleoffx           sections[SEG_MIDDLE].offx
#define SW_middleoffy           sections[SEG_MIDDLE].offy
#define SW_middlergba           sections[SEG_MIDDLE].rgba
#define SW_middletexlat         sections[SEG_MIDDLE].xlat

#define SW_topsurface           sections[SEG_TOP]
#define SW_topflags             sections[SEG_TOP].flags
#define SW_toppic               sections[SEG_TOP].texture
#define SW_topisflat            sections[SEG_TOP].isflat
#define SW_topnormal            sections[SEG_TOP].normal
#define SW_toptexmove           sections[SEG_TOP].texmove
#define SW_topoffx              sections[SEG_TOP].offx
#define SW_topoffy              sections[SEG_TOP].offy
#define SW_toprgba              sections[SEG_TOP].rgba
#define SW_toptexlat            sections[SEG_TOP].xlat

#define SW_bottomsurface        sections[SEG_BOTTOM]
#define SW_bottomflags          sections[SEG_BOTTOM].flags
#define SW_bottompic            sections[SEG_BOTTOM].texture
#define SW_bottomisflat         sections[SEG_BOTTOM].isflat
#define SW_bottomnormal         sections[SEG_BOTTOM].normal
#define SW_bottomtexmove        sections[SEG_BOTTOM].texmove
#define SW_bottomoffx           sections[SEG_BOTTOM].offx
#define SW_bottomoffy           sections[SEG_BOTTOM].offy
#define SW_bottomrgba           sections[SEG_BOTTOM].rgba
#define SW_bottomtexlat         sections[SEG_BOTTOM].xlat

// Side frame flags
#define SIDEINF_TOPPVIS     0x0001
#define SIDEINF_MIDDLEPVIS  0x0002
#define SIDEINF_BOTTOMPVIS  0x0004
end

struct side
    -       surface_t[3] sections
    BLENDMODE blendmode_t blendmode
    PTR     sector_s*   sector
    SHORT   short       flags
    -       short       frameflags
    -       line_s*[2]  neighbor    // Left and right neighbour.
    -       boolean[2]  pretendneighbor // If true, neighbor is not a "real" neighbor (it does not share a line with this side's sector).
    -       sector_s*[2] proxsector // Sectors behind the neighbors.
    -       line_s*[2]  backneighbor // Neighbour in the backsector (if any).
    -       line_s*[2]  alignneighbor // Aligned left and right neighbours.
end

internal
// Helper macros for accessing linedef data elements.
#define L_v1                    v[0]
#define L_v2                    v[1]
#define L_vo1                   vo[0]
#define L_vo2                   vo[1]
#define L_frontsector           sec[FRONT]
#define L_backsector            sec[BACK]
#define L_frontside             sides[FRONT]
#define L_backside              sides[BACK]
end

struct line
    PTR     vertex_s*[2] v
    SHORT   short       flags
    PTR     sector_s*[2] sec        // [front, back] sectors.
    FLOAT   float       dx
    FLOAT   float       dy
    INT     slopetype_t slopetype
    INT     int         validcount
    PTR     side_s*[2]  sides
    FIXED   fixed_t[4]  bbox
    -       lineowner_s*[2] vo      // Links to vertex line owner nodes [left, right]
    -       float       length      // Accurate length
    -       binangle_t  angle       // Calculated from front side's normal
    -       boolean     selfrefhackroot // This line is the root of a self-referencing hack sector
end

struct polyobj
    UINT    uint        numsegs
    PTR     seg_s**     segs
    INT     int         validcount
    PTR     degenmobj_t startSpot
    ANGLE   angle_t     angle
    INT     int         tag         // reference tag assigned in HereticEd
    -       ddvertex_t* originalPts // used as the base for the rotations
    -       ddvertex_t* prevPts     // use to restore the old point values
    FIXED   fixed_t[4]  bbox
    -       fvertex_t   dest
    INT     int         speed       // Destination XY and speed.
    ANGLE   angle_t     destAngle   // Destination angle.
    ANGLE   angle_t     angleSpeed  // Rotation speed.
    BOOL    boolean     crush       // should the polyobj attempt to crush mobjs?
    INT     int         seqType
    FIXED   fixed_t     size        // polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
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
