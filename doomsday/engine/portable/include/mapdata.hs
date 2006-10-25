# $Id$
# Runtime map data defitions. Processed by the makedmt.py script.

struct vertex
    -       vertexinfo_s* info
    -       fixed_t[2]    pos
end

public
#define DMT_VERTEX_X            DDVT_FIXED
#define DMT_VERTEX_Y            DDVT_FIXED
end

struct seg
    -       seginfo_s*  info
    PTR     vertex_s*   v1      // Start of the segment.
    PTR     vertex_s*   v2      // End of the segment.
    -       fvertex_s   fv1     // Start of the segment (float).
    -       fvertex_s   fv2     // End of the segment (float).
    FLOAT   float       length  // Accurate length of the segment (v1 -> v2).
    FIXED   fixed_t     offset
    PTR     side_s*     sidedef
    PTR     line_s*     linedef
    PTR     sector_s*   frontsector
    PTR     sector_s*   backsector
    ANGLE   angle_t     angle
    BYTE    byte        flags
end

struct subsector
    -       subsectorinfo_s* info
    PTR     sector_s*   sector
    INT     int         linecount
    INT     int         firstline
    PTR     polyobj_s*  poly    // NULL, if there is no polyobj.
    BYTE    byte        flags
    -       short       numverts
    -       fvertex_t*  verts   // A sorted list of edge vertices.
    -       fvertex_t   bbox[2] // Min and max points.
    -       fvertex_t   midpoint    // Center of vertices.
end

struct surface
    -       surfaceinfo_s *info
    INT     int         flags   // SUF_ flags
    SHORT   short       texture
    -       boolean     isflat  // true if current texture is a flat
    -       float[3]    normal  // Surface normal.
    FIXED   fixed_t[2]  texmove // Texture movement X and Y.
    FLOAT   float       offx	// Texture x offset.
    FLOAT   float       offy	// Texture y offset.
    BYTE    byte[4]     rgba    // Surface color tint
    -       translation_s* xlat
end

struct plane
    -       planeinfo_s* info
    FIXED   fixed_t     height  // Current height.
    -       surface_t   surface
    FLOAT   float       glow    // Glow amount.
    BYTE    byte[3]     glowrgb // Glow color.
    FIXED   fixed_t     target  // Target height.
    FIXED   fixed_t     speed   // Move speed.
    PTR     degenmobj_t soundorg // Sound origin for plane.
    -       sector_s*   sector  // Owner of the plane (temp)
end

public
#define DMT_PLANE_SECTOR        DDVT_PTR
#define DMT_PLANE_OFFX          DDVT_FLOAT
#define DMT_PLANE_OFFY          DDVT_FLOAT
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
end

struct sector
    -       sectorinfo_s* info
    SHORT   short       lightlevel
    BYTE    byte[3]     rgb
    INT     int         validcount  // if == validcount, already checked.
    PTR     mobj_s*     thinglist   // List of mobjs in the sector.
    INT     int         linecount   
    PTR     line_s**    Lines       // [linecount] size.
    INT     int         subscount
    PTR     subsector_s** subsectors // [subscount] size.
    -       skyfix_t[2] skyfix      // floor, ceiling.
    PTR     degenmobj_t soundorg

    FLOAT   float[NUM_REVERB_DATA] reverb
    -       int[4]      blockbox    // Mapblock bounding box.
    INT     int         planecount
    -       plane_t**   planes      // [planecount] size.
end

struct side
    -       sideinfo_s* info
    -       surface_t   top
    -       surface_t   middle
    -       surface_t   bottom
    BLENDMODE blendmode_t blendmode
    PTR     sector_s*   sector
    SHORT   short       flags
end

struct line
    -       lineinfo_s* info
    PTR     vertex_s*   v1
    PTR     vertex_s*   v2
    SHORT   short       flags
    PTR     sector_s*   frontsector
    PTR     sector_s*   backsector
    FIXED   fixed_t     dx
    FIXED   fixed_t     dy
    INT     slopetype_t slopetype
    INT     int         validcount
    PTR     side_s*[2]  sides
    FIXED   fixed_t[4]  bbox
end

struct polyobj
    INT     int         numsegs
    PTR     seg_s**     segs
    INT     int         validcount
    PTR     degenmobj_t startSpot
    ANGLE   angle_t     angle
    INT     int         tag         // reference tag assigned in HereticEd
    -       ddvertex_t* originalPts // used as the base for the rotations
    -       ddvertex_t* prevPts     // use to restore the old point values
    FIXED   fixed_t[4]  bbox
    -       vertex_t    dest
    INT     int         speed      // Destination XY and speed.
    ANGLE   angle_t     destAngle  // Destination angle.
    ANGLE   angle_t     angleSpeed // Rotation speed.
    BOOL    boolean     crush      // should the polyobj attempt to crush mobjs?
    INT     int         seqType
    FIXED   fixed_t     size       // polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
    PTR     void*       specialdata // pointer a thinker, if the poly is moving    
end

struct node
    FIXED   fixed_t     x           // Partition line.
    FIXED   fixed_t     y           // Partition line.
    FIXED   fixed_t     dx          // Partition line.
    FIXED   fixed_t     dy          // Partition line.
    -       fixed_t[2][4] bbox      // Bounding box for each child.
    -       uint[2]     children    // If NF_SUBSECTOR it's a subsector.
end
