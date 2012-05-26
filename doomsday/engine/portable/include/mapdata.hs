# $Id$
# Runtime map data definitions. Processed by the makedmt.py script.

public
#define DMT_VERTEX_ORIGIN  DDVT_DOUBLE
end

internal
#define LO_prev     link[0]
#define LO_next     link[1]

typedef struct shadowvert_s {
    coord_t         inner[2];
    coord_t         extended[2];
} shadowvert_t;

typedef struct lineowner_s {
    struct linedef_s *lineDef;
    struct lineowner_s *link[2];    // {prev, next} (i.e. {anticlk, clk}).
    binangle_t      angle;          // between this and next clockwise.
    shadowvert_t    shadowOffsets;
} lineowner_t;

/// Maximum number of walldivnode_ts in a walldivs_t dataset.
#define WALLDIVS_MAX_NODES          64

typedef struct walldivnode_s {
    coord_t height;
} walldivnode_t;

typedef struct walldivs_s {
    uint num;
    walldivnode_t nodes[WALLDIVS_MAX_NODES];
} walldivs_t;

typedef struct mvertex_s {
    // Vertex index. Always valid after loading and pruning of unused
    // vertices has occurred.
    int index;

    // Reference count. When building normal node info, unused vertices
    // will be pruned.
    int refCount;

    // Usually NULL, unless this vertex occupies the same location as a
    // previous vertex. Only used during the pruning phase.
    struct vertex_s* equiv;
} mvertex_t;
end

struct Vertex
    -       coord_t[2]  origin
    -       uint        numLineOwners // Number of line owners.
    -       lineowner_t* lineOwners // Lineowner base ptr [numlineowners] size. A doubly, circularly linked list. The base is the line with the lowest angle and the next-most with the largest angle.
    -       mvertex_t   buildData
end

internal
// Helper macros for accessing seg data elements.
#define FRONT 0
#define BACK  1

#define HE_v(n)                   v[(n)? 1:0]
#define HE_vorigin(n)             HE_v(n)->origin

#define HE_v1                     HE_v(0)
#define HE_v1origin               HE_v(0)->origin

#define HE_v2                     HE_v(1)
#define HE_v2origin               HE_v(1)->origin

#define HEDGE_BACK_SECTOR(h)      ((h)->twin ? (h)->twin->sector : NULL)

#define HEDGE_SIDE(h)             ((h)->lineDef ? &(h)->lineDef->L_side((h)->side) : NULL)
#define HEDGE_SIDEDEF(h)          ((h)->lineDef ? (h)->lineDef->L_sidedef((h)->side) : NULL)

// HEdge frame flags
#define HEDGEINF_FACINGFRONT      0x0001

/// @todo Refactor me away.
typedef struct mhedge_s {
    uint                index;
} mhedge_t;
end

public
#define DMT_HEDGE_SIDEDEF       DDVT_PTR
end

struct HEdge
    PTR     vertex_s*[2] v /// [Start, End] of the segment.
    PTR     hedge_s*    next
    PTR     hedge_s*    prev

    // Half-edge on the other side, or NULL if one-sided. This relationship
    // is always one-to-one -- if one of the half-edges is split, the twin
    // must also be split.
    PTR     hedge_s*    twin
    PTR     bspleaf_s*  bspLeaf

    PTR     linedef_s*  lineDef
    PTR     sector_s*   sector
    ANGLE   angle_t     angle
    BYTE    byte        side /// On which side of the LineDef (0=front, 1=back)?
    DOUBLE  coord_t     length /// Accurate length of the segment (v1 -> v2).
    DOUBLE  coord_t     offset
    -       biassurface_t*[3] bsuf /// For each @ref SideDefSection.
    -       short       frameFlags
    -       uint        index /// Unique. Set when saving the BSP.
    -       mhedge_t    buildData
end

internal
/**
 * @defgroup bspLeafFlags  Bsp Leaf Flags
 * @addtogroup map
 */
///@{
#define BLF_UPDATE_FANBASE      0x1 ///< The tri-fan base requires an update.
///@}
end

struct BspLeaf
    PTR     hedge_s*    hedge /// First HEdge in this leaf.
    -       int         flags /// @ref BspLeafFlags.
    -       uint        index /// Unique. Set when saving the BSP.
    -       int         addSpriteCount /// Frame number of last R_AddSprites.
    -       int         validCount
    UINT    uint        hedgeCount /// Number of HEdge's in this leaf.
    PTR     sector_s*   sector
    PTR     polyobj_s*  polyObj /// First polyobj in this leaf. Can be @c NULL.
    -       hedge_s*    fanBase /// HEdge whose vertex to use as the base for a trifan. If @c NULL then midPoint is used instead.
    -       shadowlink_s* shadows
    -       AABoxd      aaBox /// HEdge Vertex bounding box in the map coordinate space.
    -       coord_t[2]  midPoint /// Center of vertices.
    -       coord_t[2]  worldGridOffset /// Offset to align the top left of materials in the built geometry to the map coordinate space grid.
    -       biassurface_t** bsuf /// [sector->planeCount] size.
    -       uint[NUM_REVERB_DATA] reverb
end

internal
typedef enum {
    MEC_UNKNOWN = -1,
    MEC_FIRST = 0,
    MEC_METAL = MEC_FIRST,
    MEC_ROCK,
    MEC_WOOD,
    MEC_CLOTH,
    NUM_MATERIAL_ENV_CLASSES
} material_env_class_t;

#define VALID_MATERIAL_ENV_CLASS(v) ((v) >= MEC_FIRST && (v) < NUM_MATERIAL_ENV_CLASSES)

struct material_variantlist_node_s;
end

public
#define DMT_MATERIAL_FLAGS      DDVT_SHORT
#define DMT_MATERIAL_WIDTH      DDVT_INT
#define DMT_MATERIAL_HEIGHT     DDVT_INT
end

struct material
    -       ded_material_s* _def
    -       material_variantlist_node_s* _variants
    -       material_env_class_t _envClass // Environmental sound class.
    -       materialid_t _primaryBind // Unique identifier of the MaterialBind associated with this Material or @c NULL if not bound.
    -       Size2i*     _size // Logical dimensions in world-space units.
    -       short       _flags // @see materialFlags
    -       boolean     _inAnimGroup // @c true if belongs to some animgroup.
    -       boolean     _isCustom
    -       texture_s*  _detailTex;
    -       float       _detailScale;
    -       float       _detailStrength;
    -       texture_s*  _shinyTex;
    -       blendmode_t _shinyBlendmode;
    -       float[3]    _shinyMinColor;
    -       float       _shinyStrength;
    -       texture_s*  _shinyMaskTex;
    -       byte        _prepared;
end

internal
// Internal surface flags:
#define SUIF_PVIS             0x0001
#define SUIF_FIX_MISSING_MATERIAL 0x0002 // Current Material is a fix replacement
                                     // (not sent to clients, returned via DMU etc).
#define SUIF_BLEND            0x0004 // Surface possibly has a blended texture.
#define SUIF_NO_RADIO         0x0008 // No fakeradio for this surface.

#define SUIF_UPDATE_FLAG_MASK 0xff00
#define SUIF_UPDATE_DECORATIONS 0x8000

typedef struct surfacedecor_s {
    coord_t             origin[3]; // World coordinates of the decoration.
    BspLeaf*		bspLeaf;
    const struct ded_decorlight_s* def;
} surfacedecor_t;
end

struct Surface
    PTR     ddmobj_base_t base
    -       void*       owner // Either @c DMU_SIDEDEF, or @c DMU_PLANE
    INT     int         flags // SUF_ flags
    -       int         oldFlags
    PTR     material_t* material
    BLENDMODE blendmode_t blendMode
    FLOAT   float[3]    tangent
    FLOAT   float[3]    bitangent
    FLOAT   float[3]    normal
    FLOAT   float[2]    offset // [X, Y] Planar offset to surface material origin.
    -       float[2][2] oldOffset
    -       float[2]    visOffset
    -       float[2]    visOffsetDelta
    FLOAT   float[4]    rgba // Surface color tint
    -       short       inFlags // SUIF_* flags
    -       uint        numDecorations
    -       surfacedecor_t* decorations
end

internal
typedef enum {
    PLN_FLOOR,
    PLN_CEILING,
    PLN_MID,
    NUM_PLANE_TYPES
} planetype_t;
end

internal
#define PS_base                 surface.base
#define PS_tangent              surface.tangent
#define PS_bitangent            surface.bitangent
#define PS_normal               surface.normal
#define PS_material             surface.material
#define PS_offset               surface.offset
#define PS_visoffset            surface.visOffset
#define PS_rgba                 surface.rgba
#define PS_flags                surface.flags
#define PS_inflags              surface.inFlags
end

struct Plane
    PTR     sector_s*   sector /// Owner of the plane.
    -       Surface     surface
    DOUBLE  coord_t     height /// Current height.
    -       coord_t[2]  oldHeight
    DOUBLE  coord_t     target /// Target height.
    FLOAT   float       speed /// Move speed.
    -       coord_t     visHeight /// Visible plane height (smoothed).
    -       coord_t     visHeightDelta
    -       planetype_t type /// PLN_* type.
    -       int         planeID
end

internal
// Helper macros for accessing sector floor/ceiling plane data elements.
#define SP_plane(n)             planes[(n)]

#define SP_planesurface(n)      SP_plane(n)->surface
#define SP_planeheight(n)       SP_plane(n)->height
#define SP_planetangent(n)      SP_plane(n)->surface.tangent
#define SP_planebitangent(n)    SP_plane(n)->surface.bitangent
#define SP_planenormal(n)       SP_plane(n)->surface.normal
#define SP_planematerial(n)     SP_plane(n)->surface.material
#define SP_planeoffset(n)       SP_plane(n)->surface.offset
#define SP_planergb(n)          SP_plane(n)->surface.rgba
#define SP_planetarget(n)       SP_plane(n)->target
#define SP_planespeed(n)        SP_plane(n)->speed
#define SP_planevisheight(n)    SP_plane(n)->visHeight

#define SP_ceilsurface          SP_planesurface(PLN_CEILING)
#define SP_ceilheight           SP_planeheight(PLN_CEILING)
#define SP_ceiltangent          SP_planetangent(PLN_CEILING)
#define SP_ceilbitangent        SP_planebitangent(PLN_CEILING)
#define SP_ceilnormal           SP_planenormal(PLN_CEILING)
#define SP_ceilmaterial         SP_planematerial(PLN_CEILING)
#define SP_ceiloffset           SP_planeoffset(PLN_CEILING)
#define SP_ceilrgb              SP_planergb(PLN_CEILING)
#define SP_ceiltarget           SP_planetarget(PLN_CEILING)
#define SP_ceilspeed            SP_planespeed(PLN_CEILING)
#define SP_ceilvisheight        SP_planevisheight(PLN_CEILING)

#define SP_floorsurface         SP_planesurface(PLN_FLOOR)
#define SP_floorheight          SP_planeheight(PLN_FLOOR)
#define SP_floortangent         SP_planetangent(PLN_FLOOR)
#define SP_floorbitangent       SP_planebitangent(PLN_FLOOR)
#define SP_floornormal          SP_planenormal(PLN_FLOOR)
#define SP_floormaterial        SP_planematerial(PLN_FLOOR)
#define SP_flooroffset          SP_planeoffset(PLN_FLOOR)
#define SP_floorrgb             SP_planergb(PLN_FLOOR)
#define SP_floortarget          SP_planetarget(PLN_FLOOR)
#define SP_floorspeed           SP_planespeed(PLN_FLOOR)
#define SP_floorvisheight       SP_planevisheight(PLN_FLOOR)

#define S_skyfix(n)             skyFix[(n)? 1:0]
#define S_floorskyfix           S_skyfix(PLN_FLOOR)
#define S_ceilskyfix            S_skyfix(PLN_CEILING)
end

internal
// Sector frame flags
#define SIF_VISIBLE         0x1     // Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1     // Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

typedef struct msector_s {
    // Sector index. Always valid after loading & pruning.
    int index;
    int refCount;
} msector_t;
end

public
#define DMT_SECTOR_FLOORPLANE DDVT_PTR
#define DMT_SECTOR_CEILINGPLANE DDVT_PTR
end

struct Sector
    -       int         frameFlags
    INT     int         validCount // if == validCount, already checked.
    -       AABoxd      aaBox // Bounding box for the sector.
    -       coord_t     roughArea // Rough approximation of sector area.
    FLOAT   float       lightLevel
    -       float       oldLightLevel
    FLOAT   float[3]    rgb
    -       float[3]    oldRGB
    PTR     mobj_s*     mobjList // List of mobjs in the sector.
    UINT    uint        lineDefCount
    PTR     linedef_s** lineDefs // [lineDefCount+1] size.
    UINT    uint        bspLeafCount
    PTR     bspleaf_s** bspLeafs // [bspLeafCount+1] size.
    -       uint        numReverbBspLeafAttributors
    -       bspleaf_s** reverbBspLeafs // [numReverbBspLeafAttributors] size.
    PTR     ddmobj_base_t base
    UINT    uint        planeCount
    -       plane_s**   planes // [planeCount+1] size.
    -       uint        blockCount // Number of gridblocks in the sector.
    -       uint        changedBlockCount // Number of blocks to mark changed.
    -       ushort*     blocks // Light grid block indices.
    FLOAT   float[NUM_REVERB_DATA] reverb
    -       msector_t   buildData
end

internal
// Helper macros for accessing sidedef top/middle/bottom section data elements.
#define SW_surface(n)           sections[(n)]
#define SW_surfaceflags(n)      SW_surface(n).flags
#define SW_surfaceinflags(n)    SW_surface(n).inFlags
#define SW_surfacematerial(n)   SW_surface(n).material
#define SW_surfacetangent(n)    SW_surface(n).tangent
#define SW_surfacebitangent(n)  SW_surface(n).bitangent
#define SW_surfacenormal(n)     SW_surface(n).normal
#define SW_surfaceoffset(n)     SW_surface(n).offset
#define SW_surfacevisoffset(n)  SW_surface(n).visOffset
#define SW_surfacergba(n)       SW_surface(n).rgba
#define SW_surfaceblendmode(n)  SW_surface(n).blendMode

#define SW_middlesurface        SW_surface(SS_MIDDLE)
#define SW_middleflags          SW_surfaceflags(SS_MIDDLE)
#define SW_middleinflags        SW_surfaceinflags(SS_MIDDLE)
#define SW_middlematerial       SW_surfacematerial(SS_MIDDLE)
#define SW_middletangent        SW_surfacetangent(SS_MIDDLE)
#define SW_middlebitangent      SW_surfacebitangent(SS_MIDDLE)
#define SW_middlenormal         SW_surfacenormal(SS_MIDDLE)
#define SW_middletexmove        SW_surfacetexmove(SS_MIDDLE)
#define SW_middleoffset         SW_surfaceoffset(SS_MIDDLE)
#define SW_middlevisoffset      SW_surfacevisoffset(SS_MIDDLE)
#define SW_middlergba           SW_surfacergba(SS_MIDDLE)
#define SW_middleblendmode      SW_surfaceblendmode(SS_MIDDLE)

#define SW_topsurface           SW_surface(SS_TOP)
#define SW_topflags             SW_surfaceflags(SS_TOP)
#define SW_topinflags           SW_surfaceinflags(SS_TOP)
#define SW_topmaterial          SW_surfacematerial(SS_TOP)
#define SW_toptangent           SW_surfacetangent(SS_TOP)
#define SW_topbitangent         SW_surfacebitangent(SS_TOP)
#define SW_topnormal            SW_surfacenormal(SS_TOP)
#define SW_toptexmove           SW_surfacetexmove(SS_TOP)
#define SW_topoffset            SW_surfaceoffset(SS_TOP)
#define SW_topvisoffset         SW_surfacevisoffset(SS_TOP)
#define SW_toprgba              SW_surfacergba(SS_TOP)

#define SW_bottomsurface        SW_surface(SS_BOTTOM)
#define SW_bottomflags          SW_surfaceflags(SS_BOTTOM)
#define SW_bottominflags        SW_surfaceinflags(SS_BOTTOM)
#define SW_bottommaterial       SW_surfacematerial(SS_BOTTOM)
#define SW_bottomtangent        SW_surfacetangent(SS_BOTTOM)
#define SW_bottombitangent      SW_surfacebitangent(SS_BOTTOM)
#define SW_bottomnormal         SW_surfacenormal(SS_BOTTOM)
#define SW_bottomtexmove        SW_surfacetexmove(SS_BOTTOM)
#define SW_bottomoffset         SW_surfaceoffset(SS_BOTTOM)
#define SW_bottomvisoffset      SW_surfacevisoffset(SS_BOTTOM)
#define SW_bottomrgba           SW_surfacergba(SS_BOTTOM)
end

internal
#define FRONT                   0
#define BACK                    1

typedef struct msidedef_s {
    // Sidedef index. Always valid after loading & pruning.
    int index;
    int refCount;
} msidedef_t;
end

public
#define DMT_SIDEDEF_SECTOR DDVT_PTR
end

struct SideDef
    -       Surface[3]  sections
    PTR     linedef_s*  line
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
#define L_v(n)                  v[(n)? 1:0]
#define L_vorigin(n)            v[(n)]->origin

#define L_v1                    L_v(0)
#define L_v1origin              L_v(0)->origin

#define L_v2                    L_v(1)
#define L_v2origin              L_v(1)->origin

#define L_vo(n)                 vo[(n)? 1:0]
#define L_vo1                   L_vo(0)
#define L_vo2                   L_vo(1)

#define L_frontside             sides[0]
#define L_backside              sides[1]
#define L_side(n)               sides[(n)? 1:0]

#define L_sidedef(n)            L_side(n).sideDef
#define L_frontsidedef          L_sidedef(FRONT)
#define L_backsidedef           L_sidedef(BACK)

#define L_sector(n)             L_side(n).sector
#define L_frontsector           L_sector(FRONT)
#define L_backsector            L_sector(BACK)

// Is this line self-referencing (front sec == back sec)? 
#define LINE_SELFREF(l)         ((l)->L_frontsidedef && (l)->L_backsidedef && \
                                 (l)->L_frontsector == (l)->L_backsector)

// Internal flags:
#define LF_POLYOBJ              0x1 // Line is part of a polyobject.
end

internal
typedef struct mlinedef_s {
    // Linedef index. Always valid after loading & pruning of zero
    // length lines has occurred.
    int index;
    
    // One-sided linedef used for a special effect (windows).
    // The value refers to the opposite sector on the back side.
    /// @todo Refactor so this information is represented using the
    ///       BSP data objects.
    struct sector_s* windowEffect;
} mlinedef_t;

typedef struct lineside_s {
    struct sector_s* sector; /// Sector on this side.
    struct sidedef_s* sideDef; /// SideDef on this side.
    struct hedge_s* hedgeLeft;  /// Left-most HEdge on this side.
    struct hedge_s* hedgeRight; /// Right-most HEdge on this side.
    unsigned short shadowVisFrame; /// Framecount of last time shadows were drawn for this side.
} lineside_t;
end

public
#define DMT_LINEDEF_SECTOR DDVT_PTR
#define DMT_LINEDEF_SIDEDEF DDVT_PTR
#define DMT_LINEDEF_DX     DDVT_DOUBLE
#define DMT_LINEDEF_DY     DDVT_DOUBLE
#define DMT_LINEDEF_AABOX  DDVT_DOUBLE
end

struct LineDef
    PTR     vertex_s*[2] v
    -       lineowner_s*[2] vo /// Links to vertex line owner nodes [left, right].
    INT     int         flags /// Public DDLF_* flags.
    -       byte        inFlags /// Internal LF_* flags.
    INT     slopetype_t slopeType
    INT     int         validCount
    -       binangle_t  angle /// Calculated from front side's normal.
    -       coord_t[2]  direction
    -       coord_t     length /// Accurate length.
    -       AABoxd      aaBox
    -       boolean[DDMAXPLAYERS] mapped /// Whether the line has been mapped by each player yet.
    -       mlinedef_t  buildData
end

internal
#define RIGHT                   0
#define LEFT                    1

/**
 * An infinite line of the form point + direction vectors. 
 */
typedef struct partition_s {
    coord_t origin[2];
    coord_t direction[2];
} partition_t;
end

public
#define DMT_BSPNODE_AABOX  DDVT_DOUBLE
end

struct BspNode
    -       partition_t partition
    -       AABoxd[2]   aaBox    /// Bounding box for each child.
    PTR     runtime_mapdata_header_t*[2] children
    -       uint        index /// Unique. Set when saving the BSP.
end
