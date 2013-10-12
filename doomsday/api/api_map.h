/** @file api_map.h Public API to the world (map) data.
 *
 * World data comprises the map and all the objects in it. The public API
 * includes accessing and modifying map data objects via DMU.
 *
 * @ingroup world
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DOOMSDAY_API_MAP_H
#define DOOMSDAY_API_MAP_H

#include "apis.h"
#include <de/aabox.h>
#include <de/mathutil.h>

#define DMT_ARCHIVE_INDEX DDVT_INT

#define DMT_VERTEX_ORIGIN DDVT_DOUBLE

#define DMT_MATERIAL_FLAGS DDVT_SHORT
#define DMT_MATERIAL_WIDTH DDVT_INT
#define DMT_MATERIAL_HEIGHT DDVT_INT

#define DMT_SURFACE_FLAGS DDVT_INT     // SUF_ flags
#define DMT_SURFACE_MATERIAL DDVT_PTR
#define DMT_SURFACE_BLENDMODE DDVT_BLENDMODE
#define DMT_SURFACE_BITANGENT DDVT_FLOAT
#define DMT_SURFACE_TANGENT DDVT_FLOAT
#define DMT_SURFACE_NORMAL DDVT_FLOAT
#define DMT_SURFACE_OFFSET DDVT_FLOAT     // [X, Y] Planar offset to surface material origin.
#define DMT_SURFACE_RGBA DDVT_FLOAT    // Surface color tint

#define DMT_PLANE_EMITTER DDVT_PTR
#define DMT_PLANE_SECTOR DDVT_PTR      // Owner of the plane (temp)
#define DMT_PLANE_HEIGHT DDVT_DOUBLE   // Current height
#define DMT_PLANE_GLOW DDVT_FLOAT      // Glow amount
#define DMT_PLANE_GLOWRGB DDVT_FLOAT   // Glow color
#define DMT_PLANE_TARGET DDVT_DOUBLE   // Target height
#define DMT_PLANE_SPEED DDVT_DOUBLE    // Move speed

#define DMT_SECTOR_FLOORPLANE DDVT_PTR
#define DMT_SECTOR_CEILINGPLANE DDVT_PTR

#define DMT_SECTOR_VALIDCOUNT DDVT_INT // if == validCount, already checked.
#define DMT_SECTOR_LIGHTLEVEL DDVT_FLOAT
#define DMT_SECTOR_RGB DDVT_FLOAT
#define DMT_SECTOR_MOBJLIST DDVT_PTR   // List of mobjs in the sector.
#define DMT_SECTOR_LINECOUNT DDVT_UINT
#define DMT_SECTOR_LINES DDVT_PTR
#define DMT_SECTOR_EMITTER DDVT_PTR
#define DMT_SECTOR_PLANECOUNT DDVT_UINT
#define DMT_SECTOR_REVERB DDVT_FLOAT

#define DMT_SIDE_SECTOR DDVT_PTR
#define DMT_SIDE_LINE DDVT_PTR
#define DMT_SIDE_FLAGS DDVT_INT

#define DMT_LINE_SIDE DDVT_PTR
#define DMT_LINE_AABOX DDVT_DOUBLE
#define DMT_LINE_V DDVT_PTR
#define DMT_LINE_FLAGS DDVT_INT     // Public DDLF_* flags.
#define DMT_LINE_SLOPETYPE DDVT_INT
#define DMT_LINE_VALIDCOUNT DDVT_INT
#define DMT_LINE_DX DDVT_DOUBLE
#define DMT_LINE_DY DDVT_DOUBLE
#define DMT_LINE_LENGTH DDVT_DOUBLE

/**
 * Public definitions of the internal map data pointers.  These can be
 * accessed externally, but only as identifiers to data instances.
 * For example, a game could use Sector to identify to sector to
 * change with the Map Update API.
 *
 * Define @c DENG_INTERNAL_DATA_ACCESS if access to the internal map data
 * structures is needed.
 */
#if !defined __DOOMSDAY__ && !defined DENG_INTERNAL_DATA_ACCESS

// Opaque types for public use.
struct bspleaf_s;
struct bspnode_s;
struct segment_s;
struct line_s;
struct mobj_s;
struct plane_s;
struct sector_s;
struct side_s;
struct vertex_s;
struct material_s;

typedef struct bspleaf_s    BspLeaf;
typedef struct bspnode_s    BspNode;
typedef struct line_s       Line;
typedef struct plane_s      Plane;
typedef struct sector_s     Sector;
typedef struct side_s       Side;
typedef struct vertex_s     Vertex;
typedef struct material_s   Material;

#elif defined __cplusplus

// Foward declarations.
class Line;
class Sector;
class Material;
class BspLeaf;

#endif

/**
 * @defgroup lineSightFlags Line Sight Flags
 * Flags used to dictate logic within P_CheckLineSight().
 * @ingroup apiFlags map
 */
///@{
#define LS_PASSLEFT            0x1 ///< Ray may cross one-sided lines from left to right.
#define LS_PASSOVER            0x2 ///< Ray may cross over sector ceiling height on ray-entry side.
#define LS_PASSUNDER           0x4 ///< Ray may cross under sector floor height on ray-entry side.
///@}

/**
 * Describes the @em sharp coordinates of the opening between sectors which
 * interface at a given map line. The open range is defined as the gap between
 * foor and ceiling on the front side clipped by the floor and ceiling planes on
 * the back side (if present).
 */
typedef struct lineopening_s {
    /// Top and bottom z of the opening.
    float top, bottom;

    /// Distance from top to bottom.
    float range;

    /// Z height of the lowest Plane at the opening on the X|Y axis.
    /// @todo Does not belong here?
    float lowFloor;

#ifdef __cplusplus
    lineopening_s() : top(0), bottom(0), range(0), lowFloor(0) {}
    lineopening_s(Line const &line);
    lineopening_s &operator = (lineopening_s const &other);
#endif
} LineOpening;

/**
 * @defgroup pathTraverseFlags  Path Traverse Flags
 * @ingroup apiFlags map
 */
///@{
#define PTF_LINE            0x1 ///< Intercept with map lines.
#define PTF_MOBJ            0x2 ///< Intercept with mobjs.

/// Process all interceptable map element types.
#define PTF_ALL             PTF_LINE | PTF_MOBJ
///@}

typedef enum intercepttype_e {
    ICPT_MOBJ,
    ICPT_LINE
} intercepttype_t;

typedef struct intercept_s {
    float distance; ///< Along trace vector as a fraction.
    intercepttype_t type;
    union {
        struct mobj_s *mobj;
        Line *line;
    } d;
} intercept_t;

/**
 * POD structure representing a line trace state. This data should @em not be
 * modified by trace callback functions!
 */
typedef struct {
    divline_t line;
    LineOpening opening;
} TraceState;

typedef int (*traverser_t) (TraceState *trace, intercept_t const *intercept, void *context);

/**
 * @defgroup mobjLinkFlags Mobj Link Flags
 * @ingroup apiFlags world
 */
///@{
#define MLF_SECTOR          0x1 ///< Link to map sectors.
#define MLF_BLOCKMAP        0x2 ///< Link in the map's mobj blockmap.
#define MLF_NOLINE          0x4 ///< Do not link to map lines.
///@}

/**
 * @defgroup lineIteratorFlags Line Iterator Flags
 * @ingroup apiFlags world
 */
///@{
#define LIF_SECTOR          0x1 ///< Process map lines defining sectors
#define LIF_POLYOBJ         0x2 ///< Process map lines defining polyobjs

/// Process all map line types.
#define LIF_ALL             LIF_SECTOR | LIF_POLYOBJ
///@}

typedef void *MapElementPtr;
typedef void const *MapElementPtrConst;

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup world
///@{

DENG_API_TYPEDEF(Map)
{
    de_api_t api;

    /**
     * Is there a known map referenced by @a uri and if so, is it available for
     * loading?
     *
     * @param uri  Uri identifying the map to be searched for.
     * @return  @c true: a known and loadable map.
     */
    boolean         (*Exists)(char const *uri);

    boolean         (*IsCustom)(char const *uri);

    /**
     * Retrieve the name of the source file containing the map referenced by @a
     * uri if known and available for loading.
     *
     * @param uri  Uri identifying the map to be searched for.
     * @return  Fully qualified (i.e., absolute) path to the source file.
     */
    AutoStr        *(*SourceFile)(char const *uri);

    /**
     * Change the current map (will be loaded if necessary).
     *
     * @param uri  Uri identifying the map to change to.
     * @return  @c true= map was changed successfully.
     */
    boolean         (*Change)(char const *uri);

    // Lines

    /**
     * Lines and Polyobj Lines (note Polyobj Lines are iterated first).
     *
     * The validCount flags are used to avoid checking lines that are marked in
     * multiple mapblocks, so increment validCount before the first call, then
     * make one or more calls to it.
     *
     * @param flags  @ref lineBoxIteratorFlags
     */
    int             (*L_BoxIterator)(AABoxd const *box, int flags, int (*callback) (Line *, void *), void *context);

    int             (*L_BoxOnSide)(Line *line, AABoxd const *box);
    int             (*L_BoxOnSide_FixedPrecision)(Line *line, AABoxd const *box);
    coord_t         (*L_PointDistance)(Line *line, coord_t const point[2], coord_t *offset);
    coord_t         (*L_PointOnSide)(Line const *line, coord_t const point[2]);
    int             (*L_MobjsIterator)(Line *line, int (*callback) (struct mobj_s *, void *), void *context);
    void            (*L_Opening)(Line *line, LineOpening *opening);

    // Sectors

    /**
     * Increment validCount before using this. 'func' is called for each mobj
     * that is (even partly) inside the sector. This is not a 3D test, the
     * mobjs may actually be above or under the sector.
     *
     * (Lovely name; actually this is a combination of SectorMobjs and
     * a bunch of LineMobjs iterations.)
     */
    int             (*S_TouchingMobjsIterator)(Sector *sector, int (*callback) (struct mobj_s *, void *), void *context);

    /**
     * Determine the Sector on the back side of the binary space partition that
     * lies in front of the specified point within the CURRENT map's coordinate
     * space.
     *
     * A valid Sector is always returned unless the BSP leaf at that point is
     * fully degenerate (thus no sector can be determnied).
     *
     * Note: The point may not actually lay within the sector returned! (however,
     * it is on the same side of the space partition!).
     *
     * @param x  X coordinate of the point to test.
     * @param y  Y coordinate of the point to test.
     *
     * @return  Sector attributed to the BSP leaf at the specified point.
     */
    Sector         *(*S_AtPoint_FixedPrecision)(coord_t const point[2]);

    // Map Objects

    struct mobj_s  *(*MO_CreateXYZ)(thinkfunc_t function, coord_t x, coord_t y, coord_t z, angle_t angle, coord_t radius, coord_t height, int ddflags);
    void            (*MO_Destroy)(struct mobj_s *mobj);
    struct mobj_s  *(*MO_MobjById)(int id);
    int             (*MO_BoxIterator)(AABoxd const *box, int (*callback) (struct mobj_s *, void *), void *context);

    /**
     * @param statenum  Must be a valid state (not null!).
     */
    void            (*MO_SetState)(struct mobj_s *mobj, int statenum);

    /**
     * To be called after a move, to link the mobj back into the world.
     *
     * @param mobj   Mobj instance.
     * @param flags  @ref mobjLinkFlags
     */
    void            (*MO_Link)(struct mobj_s *mobj, int flags);

    /**
     * Unlinks a mobj from the world so that it can be moved.
     *
     * @param mobj   Mobj instance.
     */
    void            (*MO_Unlink)(struct mobj_s *mobj);

    void            (*MO_SpawnDamageParticleGen)(struct mobj_s *mobj, struct mobj_s *inflictor, int amount);

    /**
     * The callback function will be called once for each line that crosses
     * trough the object. This means all the lines will be two-sided.
     */
    int             (*MO_LinesIterator)(struct mobj_s *mobj, int (*callback) (Line *, void *), void *context);

    /**
     * Increment validCount before calling this routine. The callback function
     * will be called once for each sector the mobj is touching (totally or
     * partly inside). This is not a 3D check; the mobj may actually reside
     * above or under the sector.
     */
    int             (*MO_SectorsIterator)(struct mobj_s *mobj, int (*callback) (Sector *, void *), void *context);

    /**
     * Calculate the visible @a origin of @a mobj in world space, including
     * any short range offset.
     */
    void            (*MO_OriginSmoothed)(struct mobj_s *mobj, coord_t origin[3]);
    angle_t         (*MO_AngleSmoothed)(struct mobj_s *mobj);

    /**
     * Returns the sector attributed to the BSP leaf in which the mobj's origin
     * currently falls. If the mobj is not yet linked then @c 0 is returned.
     *
     * Note: The mobj is necessarily within the bounds of the sector!
     *
     * @param mobj  Mobj instance.
     */
    Sector         *(*MO_Sector)(struct mobj_s const *mobj);

    // Polyobjs

    boolean         (*PO_MoveXY)(struct polyobj_s *po, coord_t x, coord_t y);

    /**
     * Rotate @a polyobj in the map coordinate space.
     */
    boolean         (*PO_Rotate)(struct polyobj_s *po, angle_t angle);

    /**
     * Link @a polyobj to the current map. To be called after moving, rotating
     * or any other translation of the Polyobj within the map.
     */
    void            (*PO_Link)(struct polyobj_s *po);

    /**
     * Unlink @a polyobj from the current map. To be called prior to moving,
     * rotating or any other translation of the Polyobj within the map.
     */
    void            (*PO_Unlink)(struct polyobj_s *po);

    /**
     * Returns a pointer to the first Line in the polyobj.
     */
    Line           *(*PO_FirstLine)(struct polyobj_s *po);

    /**
     * Lookup a Polyobj on the current map by unique ID.
     *
     * @param id  Unique identifier of the Polyobj to be found.
     * @return  Found Polyobj instance else @c NULL.
     */
    struct polyobj_s *(*PO_PolyobjById)(int id);

    /**
     * Lookup a Polyobj on the current map by tag.
     *
     * @param tag  Tag associated with the Polyobj to be found.
     * @return  Found Polyobj instance, or @c NULL.
     */
    struct polyobj_s *(*PO_PolyobjByTag)(int tag);

    int             (*PO_BoxIterator)(AABoxd const *box, int (*callback) (struct polyobj_s *, void *), void *context);

    /**
     * The po_callback is called when a (any) polyobj hits a mobj.
     */
    void            (*PO_SetCallback)(void (*func)(struct mobj_s *, void *, void *));

    int             (*BL_BoxIterator)(AABoxd const *box, int (*callback) (BspLeaf *, void *), void *context);

    // Traversers

    int             (*PathTraverse)(coord_t const from[2], coord_t const to[2], int (*callback) (TraceState *trace, struct intercept_s const *, void *context), void *context);
    int             (*PathTraverse2)(coord_t const from[2], coord_t const to[2], int flags, int (*callback) (TraceState *trace, struct intercept_s const *, void *context), void *context);

    /**
     * Traces a line of sight.
     *
     * @param from          World position, trace origin coordinates.
     * @param to            World position, trace target coordinates.
     * @param bottomSlope   Lower limit to the Z axis angle/slope range.
     * @param topSlope      Upper limit to the Z axis angle/slope range.
     * @param flags         @ref lineSightFlags dictate trace behavior/logic.
     *
     * @return  @c true if the traverser function returns @c true for all
     *          visited lines.
     */
    boolean         (*CheckLineSight)(coord_t const from[3], coord_t const to[3],
                                      coord_t bottomSlope, coord_t topSlope, int flags);

    /**
     * Update the "opening" state for the specified trace according to heights
     * defined by the minimal planes on trace side, which intercept @a line.
     */
    void            (*TraceAdjustOpening)(TraceState *trace, Line *line);

    /*
     * Map Updates (DMU):
     *
     * The Map Update API is used for accessing and making changes to map data
     * during gameplay. From here, the relevant engine's subsystems will be
     * notified of changes in the map data they use, thus allowing them to
     * update their status whenever needed.
     */

    /**
     * Translate a DMU element/property constant to a string. Primarily intended
     * for error/debug messages.
     */
    char const     *(*Str)(uint prop);

    /**
     * Determines the type of the map data object.
     *
     * @param ptr  Pointer to a map data object.
     */
    int             (*GetType)(MapElementPtrConst ptr);

    /**
     * Convert a pointer to DMU object to an element index.
     */
    int             (*ToIndex)(MapElementPtrConst ptr);

    /**
     * Convert an element index to a DMU object pointer.
     */
    void           *(*ToPtr)(int type, int index);

    /**
     * Returns the total number of DMU objects of @a type. For example, if the
     * type is @c DMU_LINE then the total number of map Lines is returned.
     */
    int             (*Count)(int type);

    /**
     * Call a callback function on a selecton of DMU objects specified with an
     * object type and element index.
     *
     * @param type          DMU type for selected object(s).
     * @param index         Index of the selected object(s).
     * @param callback      Function to be called for each object.
     * @param context       Data context pointer passed to the callback.
     *
     * @return  @c =false if all callbacks return @c false. If a non-zero value
     * is returned by the callback function, iteration is aborted immediately
     * and the value returned by the callback is returned.
     */
    int             (*Callback)(int type, int index, int (*callback)(MapElementPtr p, void *context), void *context);

    /**
     * @ref Callback() alternative where the set of selected objects is instead
     * specified with an object type and element @a pointer. Behavior and function
     * are otherwise identical.
     *
     * @param type          DMU type for selected object(s).
     * @param pointer       DMU element pointer to make callback(s) for.
     * @param callback      Function to be called for each object.
     * @param context       Data context pointer passed to the callback.
     *
     * @return  @c =false if all callbacks return @c false. If a non-zero value
     * is returned by the callback function, iteration is aborted immediately
     * and the value returned by the callback is returned.
     */
    int             (*Callbackp)(int type, MapElementPtr pointer, int (*callback)(MapElementPtr p, void *context), void *context);

    /**
     * An efficient alternative mechanism for iterating a selection of sub-objects
     * and performing a callback on each.
     *
     * @param pointer       DMU object to iterate sub-objects of.
     * @param prop          DMU property type identifying the sub-objects to iterate.
     * @param callback      Function to be called for each object.
     * @param context       Data context pointer passsed to the callback.
     *
     * @return  @c =false if all callbacks return @c false. If a non-zero value
     * is returned by the callback function, iteration is aborted immediately
     * and the value returned by the callback is returned.
     */
    int             (*Iteratep)(MapElementPtr pointer, uint prop, int (*callback) (MapElementPtr p, void *context), void *context);

    /**
     * Allocates a new dummy object.
     *
     * @param type          DMU type of the dummy object.
     * @param extraData     Extra data pointer of the dummy. Points to
     *                      caller-allocated memory area of extra data for the
     *                      dummy.
     */
    MapElementPtr   (*AllocDummy)(int type, void *extraData);

    /**
     * Frees a dummy object.
     */
    void            (*FreeDummy)(MapElementPtr dummy);

    /**
     * Determines if a map data object is a dummy.
     */
    boolean         (*IsDummy)(MapElementPtrConst dummy);

    /**
     * Returns the extra data pointer of the dummy, or NULL if the object is not
     * a dummy object.
     */
    void           *(*DummyExtraData)(MapElementPtr dummy);

    // Map Entities
    uint            (*CountMapObjs)(int entityId);
    byte            (*GetGMOByte)(int entityId, int elementIndex, int propertyId);
    short           (*GetGMOShort)(int entityId, int elementIndex, int propertyId);
    int             (*GetGMOInt)(int entityId, int elementIndex, int propertyId);
    fixed_t         (*GetGMOFixed)(int entityId, int elementIndex, int propertyId);
    angle_t         (*GetGMOAngle)(int entityId, int elementIndex, int propertyId);
    float           (*GetGMOFloat)(int entityId, int elementIndex, int propertyId);

    /* index-based write functions */
    void            (*SetBool)(int type, int index, uint prop, boolean param);
    void            (*SetByte)(int type, int index, uint prop, byte param);
    void            (*SetInt)(int type, int index, uint prop, int param);
    void            (*SetFixed)(int type, int index, uint prop, fixed_t param);
    void            (*SetAngle)(int type, int index, uint prop, angle_t param);
    void            (*SetFloat)(int type, int index, uint prop, float param);
    void            (*SetDouble)(int type, int index, uint prop, double param);
    void            (*SetPtr)(int type, int index, uint prop, void *param);

    void            (*SetBoolv)(int type, int index, uint prop, boolean *params);
    void            (*SetBytev)(int type, int index, uint prop, byte *params);
    void            (*SetIntv)(int type, int index, uint prop, int *params);
    void            (*SetFixedv)(int type, int index, uint prop, fixed_t *params);
    void            (*SetAnglev)(int type, int index, uint prop, angle_t *params);
    void            (*SetFloatv)(int type, int index, uint prop, float *params);
    void            (*SetDoublev)(int type, int index, uint prop, double *params);
    void            (*SetPtrv)(int type, int index, uint prop, void *params);

    /* pointer-based write functions */
    void            (*SetBoolp)(MapElementPtr ptr, uint prop, boolean param);
    void            (*SetBytep)(MapElementPtr ptr, uint prop, byte param);
    void            (*SetIntp)(MapElementPtr ptr, uint prop, int param);
    void            (*SetFixedp)(MapElementPtr ptr, uint prop, fixed_t param);
    void            (*SetAnglep)(MapElementPtr ptr, uint prop, angle_t param);
    void            (*SetFloatp)(MapElementPtr ptr, uint prop, float param);
    void            (*SetDoublep)(MapElementPtr ptr, uint prop, double param);
    void            (*SetPtrp)(MapElementPtr ptr, uint prop, void* param);

    void            (*SetBoolpv)(MapElementPtr ptr, uint prop, boolean *params);
    void            (*SetBytepv)(MapElementPtr ptr, uint prop, byte *params);
    void            (*SetIntpv)(MapElementPtr ptr, uint prop, int *params);
    void            (*SetFixedpv)(MapElementPtr ptr, uint prop, fixed_t *params);
    void            (*SetAnglepv)(MapElementPtr ptr, uint prop, angle_t *params);
    void            (*SetFloatpv)(MapElementPtr ptr, uint prop, float *params);
    void            (*SetDoublepv)(MapElementPtr ptr, uint prop, double *params);
    void            (*SetPtrpv)(MapElementPtr ptr, uint prop, void *params);

    /* index-based read functions */
    boolean         (*GetBool)(int type, int index, uint prop);
    byte            (*GetByte)(int type, int index, uint prop);
    int             (*GetInt)(int type, int index, uint prop);
    fixed_t         (*GetFixed)(int type, int index, uint prop);
    angle_t         (*GetAngle)(int type, int index, uint prop);
    float           (*GetFloat)(int type, int index, uint prop);
    double          (*GetDouble)(int type, int index, uint prop);
    void*           (*GetPtr)(int type, int index, uint prop);

    void            (*GetBoolv)(int type, int index, uint prop, boolean *params);
    void            (*GetBytev)(int type, int index, uint prop, byte *params);
    void            (*GetIntv)(int type, int index, uint prop, int *params);
    void            (*GetFixedv)(int type, int index, uint prop, fixed_t *params);
    void            (*GetAnglev)(int type, int index, uint prop, angle_t *params);
    void            (*GetFloatv)(int type, int index, uint prop, float *params);
    void            (*GetDoublev)(int type, int index, uint prop, double *params);
    void            (*GetPtrv)(int type, int index, uint prop, void *params);

    /* pointer-based read functions */
    boolean         (*GetBoolp)(MapElementPtr ptr, uint prop);
    byte            (*GetBytep)(MapElementPtr ptr, uint prop);
    int             (*GetIntp)(MapElementPtr ptr, uint prop);
    fixed_t         (*GetFixedp)(MapElementPtr ptr, uint prop);
    angle_t         (*GetAnglep)(MapElementPtr ptr, uint prop);
    float           (*GetFloatp)(MapElementPtr ptr, uint prop);
    double          (*GetDoublep)(MapElementPtr ptr, uint prop);
    void*           (*GetPtrp)(MapElementPtr ptr, uint prop);

    void            (*GetBoolpv)(MapElementPtr ptr, uint prop, boolean *params);
    void            (*GetBytepv)(MapElementPtr ptr, uint prop, byte *params);
    void            (*GetIntpv)(MapElementPtr ptr, uint prop, int *params);
    void            (*GetFixedpv)(MapElementPtr ptr, uint prop, fixed_t *params);
    void            (*GetAnglepv)(MapElementPtr ptr, uint prop, angle_t *params);
    void            (*GetFloatpv)(MapElementPtr ptr, uint prop, float *params);
    void            (*GetDoublepv)(MapElementPtr ptr, uint prop, double *params);
    void            (*GetPtrpv)(MapElementPtr ptr, uint prop, void *params);
}
DENG_API_T(Map);

#ifndef DENG_NO_API_MACROS_MAP
#define P_MapExists                         _api_Map.Exists
#define P_MapIsCustom                       _api_Map.IsCustom
#define P_MapSourceFile                     _api_Map.SourceFile
#define P_MapChange                         _api_Map.Change

#define Line_BoxIterator                    _api_Map.L_BoxIterator
#define Line_BoxOnSide                      _api_Map.L_BoxOnSide
#define Line_BoxOnSide_FixedPrecision       _api_Map.L_BoxOnSide_FixedPrecision
#define Line_PointDistance                  _api_Map.L_PointDistance
#define Line_PointOnSide                    _api_Map.L_PointOnSide
#define Line_TouchingMobjsIterator          _api_Map.L_MobjsIterator
#define Line_Opening                        _api_Map.L_Opening

#define Sector_TouchingMobjsIterator        _api_Map.S_TouchingMobjsIterator
#define Sector_AtPoint_FixedPrecision       _api_Map.S_AtPoint_FixedPrecision

#define Mobj_CreateXYZ                      _api_Map.MO_CreateXYZ
#define Mobj_Destroy                        _api_Map.MO_Destroy
#define Mobj_ById                           _api_Map.MO_MobjById
#define Mobj_BoxIterator                    _api_Map.MO_BoxIterator
#define Mobj_SetState                       _api_Map.MO_SetState
#define Mobj_Link                           _api_Map.MO_Link
#define Mobj_Unlink                         _api_Map.MO_Unlink
#define Mobj_SpawnDamageParticleGen         _api_Map.MO_SpawnDamageParticleGen
#define Mobj_TouchedLinesIterator           _api_Map.MO_LinesIterator
#define Mobj_TouchedSectorsIterator         _api_Map.MO_SectorsIterator
#define Mobj_AngleSmoothed                  _api_Map.MO_AngleSmoothed
#define Mobj_OriginSmoothed                 _api_Map.MO_OriginSmoothed
#define Mobj_Sector                         _api_Map.MO_Sector

#define Polyobj_MoveXY                      _api_Map.PO_MoveXY
#define Polyobj_Rotate                      _api_Map.PO_Rotate
#define Polyobj_Link                        _api_Map.PO_Link
#define Polyobj_Unlink                      _api_Map.PO_Unlink
#define Polyobj_FirstLine                   _api_Map.PO_FirstLine
#define Polyobj_ById                        _api_Map.PO_PolyobjById
#define Polyobj_ByTag                       _api_Map.PO_PolyobjByTag
#define Polyobj_BoxIterator                 _api_Map.PO_BoxIterator
#define Polyobj_SetCallback                 _api_Map.PO_SetCallback

#define BspLeaf_BoxIterator                 _api_Map.BL_BoxIterator

#define P_PathTraverse                      _api_Map.PathTraverse
#define P_PathTraverse2                     _api_Map.PathTraverse2
#define P_CheckLineSight                    _api_Map.CheckLineSight
#define P_TraceAdjustOpening                _api_Map.TraceAdjustOpening
#define P_SetTraceOpening                   _api_Map.SetTraceOpening

#define DMU_Str                             _api_Map.Str
#define DMU_GetType                         _api_Map.GetType
#define P_ToIndex                           _api_Map.ToIndex
#define P_ToPtr                             _api_Map.ToPtr
#define P_Count                             _api_Map.Count
#define P_Callback                          _api_Map.Callback
#define P_Callbackp                         _api_Map.Callbackp
#define P_Iteratep                          _api_Map.Iteratep
#define P_AllocDummy                        _api_Map.AllocDummy
#define P_FreeDummy                         _api_Map.FreeDummy
#define P_IsDummy                           _api_Map.IsDummy
#define P_DummyExtraData                    _api_Map.DummyExtraData
#define P_CountMapObjs                      _api_Map.CountMapObjs
#define P_GetGMOByte                        _api_Map.GetGMOByte
#define P_GetGMOShort                       _api_Map.GetGMOShort
#define P_GetGMOInt                         _api_Map.GetGMOInt
#define P_GetGMOFixed                       _api_Map.GetGMOFixed
#define P_GetGMOAngle                       _api_Map.GetGMOAngle
#define P_GetGMOFloat                       _api_Map.GetGMOFloat
#define P_SetBool                           _api_Map.SetBool
#define P_SetByte                           _api_Map.SetByte
#define P_SetInt                            _api_Map.SetInt
#define P_SetFixed                          _api_Map.SetFixed
#define P_SetAngle                          _api_Map.SetAngle
#define P_SetFloat                          _api_Map.SetFloat
#define P_SetDouble                         _api_Map.SetDouble
#define P_SetPtr                            _api_Map.SetPtr
#define P_SetBoolv                          _api_Map.SetBoolv
#define P_SetBytev                          _api_Map.SetBytev
#define P_SetIntv                           _api_Map.SetIntv
#define P_SetFixedv                         _api_Map.SetFixedv
#define P_SetAnglev                         _api_Map.SetAnglev
#define P_SetFloatv                         _api_Map.SetFloatv
#define P_SetDoublev                        _api_Map.SetDoublev
#define P_SetPtrv                           _api_Map.SetPtrv
#define P_SetBoolp                          _api_Map.SetBoolp
#define P_SetBytep                          _api_Map.SetBytep
#define P_SetIntp                           _api_Map.SetIntp
#define P_SetFixedp                         _api_Map.SetFixedp
#define P_SetAnglep                         _api_Map.SetAnglep
#define P_SetFloatp                         _api_Map.SetFloatp
#define P_SetDoublep                        _api_Map.SetDoublep
#define P_SetPtrp                           _api_Map.SetPtrp
#define P_SetBoolpv                         _api_Map.SetBoolpv
#define P_SetBytepv                         _api_Map.SetBytepv
#define P_SetIntpv                          _api_Map.SetIntpv
#define P_SetFixedpv                        _api_Map.SetFixedpv
#define P_SetAnglepv                        _api_Map.SetAnglepv
#define P_SetFloatpv                        _api_Map.SetFloatpv
#define P_SetDoublepv                       _api_Map.SetDoublepv
#define P_SetPtrpv                          _api_Map.SetPtrpv
#define P_GetBool                           _api_Map.GetBool
#define P_GetByte                           _api_Map.GetByte
#define P_GetInt                            _api_Map.GetInt
#define P_GetFixed                          _api_Map.GetFixed
#define P_GetAngle                          _api_Map.GetAngle
#define P_GetFloat                          _api_Map.GetFloat
#define P_GetDouble                         _api_Map.GetDouble
#define P_GetPtr                            _api_Map.GetPtr
#define P_GetBoolv                          _api_Map.GetBoolv
#define P_GetBytev                          _api_Map.GetBytev
#define P_GetIntv                           _api_Map.GetIntv
#define P_GetFixedv                         _api_Map.GetFixedv
#define P_GetAnglev                         _api_Map.GetAnglev
#define P_GetFloatv                         _api_Map.GetFloatv
#define P_GetDoublev                        _api_Map.GetDoublev
#define P_GetPtrv                           _api_Map.GetPtrv
#define P_GetBoolp                          _api_Map.GetBoolp
#define P_GetBytep                          _api_Map.GetBytep
#define P_GetIntp                           _api_Map.GetIntp
#define P_GetFixedp                         _api_Map.GetFixedp
#define P_GetAnglep                         _api_Map.GetAnglep
#define P_GetFloatp                         _api_Map.GetFloatp
#define P_GetDoublep                        _api_Map.GetDoublep
#define P_GetPtrp                           _api_Map.GetPtrp
#define P_GetBoolpv                         _api_Map.GetBoolpv
#define P_GetBytepv                         _api_Map.GetBytepv
#define P_GetIntpv                          _api_Map.GetIntpv
#define P_GetFixedpv                        _api_Map.GetFixedpv
#define P_GetAnglepv                        _api_Map.GetAnglepv
#define P_GetFloatpv                        _api_Map.GetFloatpv
#define P_GetDoublepv                       _api_Map.GetDoublepv
#define P_GetPtrpv                          _api_Map.GetPtrpv
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(Map);
#endif

///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DOOMSDAY_API_MAP_H
