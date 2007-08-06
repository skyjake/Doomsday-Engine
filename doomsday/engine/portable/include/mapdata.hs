# $Id$
# Runtime map data defitions. Processed by the makedmt.py script.

public
#define DMT_VERTEX_POS	DDVT_FLOAT
end

internal
#define V_pos					v.pos
end

struct vertex
    -       fvertex_t	v
    -       uint        numlineowners // Number of line owners.
    -       lineowner_s* lineowners // Lineowner base ptr [numlineowners] size. A doubly, circularly linked list. The base is the line with the lowest angle and the next-most with the largest angle.
    -       boolean     anchored	// One or more of our line owners are one-sided.
end

internal
// Helper macros for accessing seg data elements.
#define FRONT 0
#define BACK  1

#define SG_v(n)					v[(n)]
#define SG_vpos(n)				SG_v(n)->V_pos

#define SG_v1                   SG_v(0)
#define SG_v1pos				SG_v(0)->V_pos

#define SG_v2                   SG_v(1)
#define SG_v2pos				SG_v(1)->V_pos

#define SG_sector(n)			sec[(n)]
#define SG_frontsector          SG_sector(FRONT)
#define SG_backsector           SG_sector(BACK)

// Seg flags
#define SEGF_POLYOBJ			0x1 // Seg is part of a poly object.
#define SEGF_MIGRANT			0x2 // Seg's sectors are NOT the same as the subsector they are in.
									// Apparently, glBSP combines segs from unclosed sectors into
									// subsectors to guarantee they are convex.

// Seg frame flags
#define SEGINF_FACINGFRONT      0x0001
#define SEGINF_BACKSECSKYFIX    0x0002
end

struct seg
    PTR     vertex_s*[2] v			// [Start, End] of the segment.
    FLOAT   float       length		// Accurate length of the segment (v1 -> v2).
    FLOAT   float       offset
    PTR     side_s*     sidedef
    PTR     line_s*     linedef
    PTR     sector_s*[2] sec
    PTR		seg_s*		backseg
    ANGLE   angle_t     angle
    BYTE    byte		side		// 0=front, 1=back
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
    -		uint		firstWADSeg // First seg index, as specified in SSECTORS lump.
    UINT    uint        segcount
    PTR		seg_s**		segs		// [segcount] size.
    PTR     polyobj_s*  poly		// NULL, if there is no polyobj.
    -		int         flags
#    -       ushort      numverts
#    -       fvertex_t*  verts		// A sorted list of edge vertices.
    -       fvertex_t   bbox[2]		// Min and max points.
    -       fvertex_t   midpoint	// Center of vertices.
    -       subplaneinfo_s** planes
    -       ushort      numvertices
    -       fvertex_s** vertices	// [numvertices] size
    -       int         validcount
    -       shadowlink_s* shadows
    -       uint        group
    -       uint[NUM_REVERB_DATA] reverb
end

public
#define DMT_MATERIAL_TEXTURE DDVT_SHORT
end

internal
typedef struct material_s {
	short		texture;
	boolean		isflat;
	struct translation_s* xlat;
} material_t;

#define SM_texture		material.texture
#define SM_isflat		material.isflat
#define SM_xlat			material.xlat

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
    INT     int         flags		// SUF_ flags
    -       int         oldflags
    -		material_t  material
    -       material_t  oldmaterial
    BLENDMODE blendmode_t blendmode
    -       float[3]    normal		// Surface normal
    -       float[3]    oldnormal
    FLOAT   float[2]    texmove		// Texture movement X and Y
    -       float[2]    oldtexmove
    FLOAT   float       offx		// Texture x offset
    -       float       oldoffx
    FLOAT   float       offy		// Texture y offset
    -       float       oldoffy
    FLOAT   float[4]    rgba		// Surface color tint
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
#define PS_normal				surface.normal
#define PS_texture				surface.SM_texture
#define PS_isflat				surface.SM_isflat
#define PS_offx					surface.offx
#define PS_offy					surface.offy
#define PS_texmove				surface.texmove
end

struct plane
    FLOAT   float       height		// Current height
    -       float[2]    oldheight
    -       surface_t   surface
    FLOAT   float       glow		// Glow amount
    FLOAT   float[3]    glowrgb		// Glow color
    FLOAT   float       target		// Target height
    FLOAT   float       speed		// Move speed
    PTR     degenmobj_t soundorg	// Sound origin for plane
    PTR     sector_s*   sector		// Owner of the plane (temp)
    -       float       visheight	// Visible plane height (smoothed)
    -       float       visoffset
end

internal
// Helper macros for accessing sector floor/ceiling plane data elements.
#define SP_plane(n)				planes[(n)]

#define SP_planesurface(n)      SP_plane(n)->surface
#define SP_planeheight(n)       SP_plane(n)->height
#define SP_planenormal(n)       SP_plane(n)->surface.normal
#define SP_planetexture(n)      SP_plane(n)->surface.SM_texture
#define SP_planeisflat(n)       SP_plane(n)->surface.SM_isflat
#define SP_planeoffx(n)         SP_plane(n)->surface.offx
#define SP_planeoffy(n)         SP_plane(n)->surface.offy
#define SP_planergb(n)          SP_plane(n)->surface.rgba
#define SP_planeglow(n)         SP_plane(n)->glow
#define SP_planeglowrgb(n)      SP_plane(n)->glowrgb
#define SP_planetarget(n)       SP_plane(n)->target
#define SP_planespeed(n)        SP_plane(n)->speed
#define SP_planetexmove(n)      SP_plane(n)->surface.texmove
#define SP_planesoundorg(n)     SP_plane(n)->soundorg
#define SP_planevisheight(n)	SP_plane(n)->visheight

#define SP_ceilsurface          SP_planesurface(PLN_CEILING)
#define SP_ceilheight           SP_planeheight(PLN_CEILING)
#define SP_ceilnormal           SP_planenormal(PLN_CEILING)
#define SP_ceiltexture          SP_planetexture(PLN_CEILING)
#define SP_ceilisflat           SP_planeisflat(PLN_CEILING)
#define SP_ceiloffx             SP_planeoffx(PLN_CEILING)
#define SP_ceiloffy             SP_planeoffy(PLN_CEILING)
#define SP_ceilrgb              SP_planergb(PLN_CEILING)
#define SP_ceilglow             SP_planeglow(PLN_CEILING)
#define SP_ceilglowrgb          SP_planeglowrgb(PLN_CEILING)
#define SP_ceiltarget           SP_planetarget(PLN_CEILING)
#define SP_ceilspeed            SP_planespeed(PLN_CEILING)
#define SP_ceiltexmove          SP_planetexmove(PLN_CEILING)
#define SP_ceilsoundorg         SP_planesoundorg(PLN_CEILING)
#define SP_ceilvisheight        SP_planevisheight(PLN_CEILING)

#define SP_floorsurface         SP_planesurface(PLN_FLOOR)
#define SP_floorheight          SP_planeheight(PLN_FLOOR)
#define SP_floornormal          SP_planenormal(PLN_FLOOR)
#define SP_floortexture         SP_planetexture(PLN_FLOOR)
#define SP_floorisflat          SP_planeisflat(PLN_FLOOR)
#define SP_flooroffx            SP_planeoffx(PLN_FLOOR)
#define SP_flooroffy            SP_planeoffy(PLN_FLOOR)
#define SP_floorrgb             SP_planergb(PLN_FLOOR)
#define SP_floorglow            SP_planeglow(PLN_FLOOR)
#define SP_floorglowrgb         SP_planeglowrgb(PLN_FLOOR)
#define SP_floortarget          SP_planetarget(PLN_FLOOR)
#define SP_floorspeed           SP_planespeed(PLN_FLOOR)
#define SP_floortexmove         SP_planetexmove(PLN_FLOOR)
#define SP_floorsoundorg        SP_planesoundorg(PLN_FLOOR)
#define SP_floorvisheight       SP_planevisheight(PLN_FLOOR)

#define S_skyfix(n)				skyfix[(n)]
#define S_floorskyfix			S_skyfix(PLN_FLOOR)
#define S_ceilskyfix			S_skyfix(PLN_CEILING)
end

internal
// Sector frame flags
#define SIF_VISIBLE         0x1		// Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1		// Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

// Sector flags.
#define SECF_INVIS_FLOOR    0x1
#define SECF_INVIS_CEILING  0x2

typedef struct ssecgroup_s {
	struct sector_s**   linked;     // [sector->planecount] size.
	                                // Plane attached to another sector.
} ssecgroup_t;
end

struct sector
    FLOAT   float       lightlevel
    -       float       oldlightlevel
    FLOAT   float[3]    rgb
    -       float[3]    oldrgb
    INT     int         validcount  // if == validcount, already checked.
    PTR     mobj_s*     thinglist   // List of mobjs in the sector.
    UINT    uint        linecount   
    PTR     line_s**    Lines       // [linecount] size.
    UINT    uint        subscount
    PTR     subsector_s** subsectors // [subscount] size.
    -		uint		numReverbSSecAttributors
    -		subsector_s** reverbSSecs // [numReverbSSecAttributors] size.
    -       uint        subsgroupcount
    -       ssecgroup_t* subsgroups // [subsgroupcount] size.
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
#define SW_surface(n)			sections[(n)]
#define SW_surfaceflags(n)      SW_surface(n).flags
#define SW_surfacetexture(n)    SW_surface(n).SM_texture
#define SW_surfaceisflat(n)     SW_surface(n).SM_isflat
#define SW_surfacenormal(n)     SW_surface(n).normal
#define SW_surfacetexmove(n)    SW_surface(n).texmove
#define SW_surfaceoffx(n)       SW_surface(n).offx
#define SW_surfaceoffy(n)       SW_surface(n).offy
#define SW_surfacergba(n)       SW_surface(n).rgba
#define SW_surfacetexlat(n)     SW_surface(n).SM_xlat
#define SW_surfaceblendmode(n)  SW_surface(n).blendmode

#define SW_middlesurface        SW_surface(SEG_MIDDLE)
#define SW_middleflags          SW_surfaceflags(SEG_MIDDLE)
#define SW_middletexture        SW_surfacetexture(SEG_MIDDLE)
#define SW_middleisflat         SW_surfaceisflat(SEG_MIDDLE)
#define SW_middlenormal         SW_surfacenormal(SEG_MIDDLE)
#define SW_middletexmove        SW_surfacetexmove(SEG_MIDDLE)
#define SW_middleoffx           SW_surfaceoffx(SEG_MIDDLE)
#define SW_middleoffy           SW_surfaceoffy(SEG_MIDDLE)
#define SW_middlergba           SW_surfacergba(SEG_MIDDLE)
#define SW_middletexlat         SW_surfacetexlat(SEG_MIDDLE)
#define SW_middleblendmode      SW_surfaceblendmode(SEG_MIDDLE)

#define SW_topsurface           SW_surface(SEG_TOP)
#define SW_topflags             SW_surfaceflags(SEG_TOP)
#define SW_toptexture           SW_surfacetexture(SEG_TOP)
#define SW_topisflat            SW_surfaceisflat(SEG_TOP)
#define SW_topnormal            SW_surfacenormal(SEG_TOP)
#define SW_toptexmove           SW_surfacetexmove(SEG_TOP)
#define SW_topoffx              SW_surfaceoffx(SEG_TOP)
#define SW_topoffy              SW_surfaceoffy(SEG_TOP)
#define SW_toprgba              SW_surfacergba(SEG_TOP)
#define SW_toptexlat            SW_surfacetexlat(SEG_TOP)

#define SW_bottomsurface        SW_surface(SEG_BOTTOM)
#define SW_bottomflags          SW_surfaceflags(SEG_BOTTOM)
#define SW_bottomtexture        SW_surfacetexture(SEG_BOTTOM)
#define SW_bottomisflat         SW_surfaceisflat(SEG_BOTTOM)
#define SW_bottomnormal         SW_surfacenormal(SEG_BOTTOM)
#define SW_bottomtexmove        SW_surfacetexmove(SEG_BOTTOM)
#define SW_bottomoffx           SW_surfaceoffx(SEG_BOTTOM)
#define SW_bottomoffy           SW_surfaceoffy(SEG_BOTTOM)
#define SW_bottomrgba           SW_surfacergba(SEG_BOTTOM)
#define SW_bottomtexlat         SW_surfacetexlat(SEG_BOTTOM)
end

struct side
    -       surface_t[3] sections
    UINT	uint		segcount
    PTR		seg_s**		segs		// [segcount] size, segs arranged left>right
    PTR     sector_s*   sector
    SHORT   short       flags
end

public
#define DMT_LINE_SEC	DDVT_PTR
end

internal
// Helper macros for accessing linedef data elements.
#define L_v(n)					v[(n)]
#define L_vpos(n)				v[(n)]->V_pos

#define L_v1					L_v(0)
#define L_v1pos					L_v(0)->V_pos

#define L_v2					L_v(1)
#define L_v2pos					L_v(1)->V_pos

#define L_vo(n)					vo[(n)]
#define L_vo1                   L_vo(0)
#define L_vo2                   L_vo(1)

#define L_side(n)  			    sides[(n)]
#define L_frontside             L_side(FRONT)
#define L_backside              L_side(BACK)
#define L_sector(n)             sides[(n)]->sector
#define L_frontsector           L_sector(FRONT)
#define L_backsector            L_sector(BACK)

// Line flags 
#define LINEF_SELFREF           0x1 // Front and back sectors of this line are the same.
#define LINEF_SELFREFHACKROOT	0x2	// This line is the root of a self-referencing hack sector
#define LINEF_BENIGN			0x4 // Benign lines are those which have no front or back segs
									// (though they may have sides). These are induced in GL
									// node generation process. They are inoperable and will
									// NOT be added to a sector's line table.
end

struct line
    PTR     vertex_s*[2] v
    -		int			flags
    SHORT   short       mapflags	// MF_* flags, read from the LINEDEFS, map data lump.
    FLOAT   float       dx
    FLOAT   float       dy
    INT     slopetype_t slopetype
    INT     int         validcount
    PTR     side_s*[2]  sides
    FIXED   fixed_t[4]  bbox
    -       lineowner_s*[2] vo      // Links to vertex line owner nodes [left, right]
    -       float       length      // Accurate length
    -       binangle_t  angle       // Calculated from front side's normal
    -       boolean[DDMAXPLAYERS] mapped // Whether the line has been mapped by each player yet.
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
