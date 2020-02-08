/** @file api_map.h  C API to the world and map data.
 *
 * @ingroup world
 *
 * World data comprises the map and all the objects in it. The public API
 * includes accessing and modifying map data objects via DMU.
 *
 * This was originally created to provide safe access to Doomsday's
 * internal data structures as defined by the Doomsday client executable.
 * Now as part of libdoomsday the data would be accessible directly via
 * the world classes, however this API remains for C language support and
 * backwards compatibility.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#pragma once

#include "libdoomsday.h"
#include "world/thinker.h"
#include "world/valuetype.h"

#include <de/legacy/aabox.h>
#include <de/legacy/mathutil.h>
#include <de/legacy/str.h>

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
#define DMT_SIDE_EMITTER DDVT_PTR

#define DMT_LINE_SIDE DDVT_PTR
#define DMT_LINE_BOUNDS DDVT_PTR
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
 */
#if !defined __DOOMSDAY__ && !defined __LIBDOOMSDAY__

// Opaque types for public use.
struct convexsubspace_s;
struct interceptor_s;
struct line_s;
struct mobj_s;
struct material_s;
struct plane_s;
struct side_s;
struct sector_s;
struct vertex_s;
struct polyobj_s;

typedef struct convexsubspace_s ConvexSubspace;
typedef struct interceptor_s    Interceptor;
typedef struct line_s           Line;
typedef struct plane_s          Plane;
typedef struct side_s           Side;
typedef struct sector_s         Sector;
typedef struct vertex_s         Vertex;

typedef struct interceptor_s    world_Interceptor;
typedef struct line_s           world_Line;
typedef struct material_s       world_Material;
typedef struct sector_s         world_Sector;
typedef struct subsector_s      world_Subsector;

#elif defined(__cplusplus)

// Foward declarations.
namespace world
{
    class ConvexSubspace;
    class Material;
    class Line;
    class Plane;
    class Sector;
    class Subsector;
    class Vertex;
    class Interceptor;
}

using world_Line        = world::Line;
using world_Material    = world::Material;
using world_Interceptor = world::Interceptor;
using world_Sector      = world::Sector;
using world_Subsector   = world::Subsector;

#endif

struct lineopening_s;

typedef struct lineopening_s LineOpening;

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
    intercepttype_t type;
    union {
        struct mobj_s *mobj;
        world_Line *line;
    };
    double distance;    ///< Along trace vector as a fraction.
    world_Interceptor *trace; ///< Trace which produced the intercept.
} Intercept;

typedef int (*traverser_t) (const Intercept *intercept, void *context);

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
typedef const void *MapElementPtrConst;

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup world
///@{

/**
 * Determines whether the given @a uri references a known map.
 */
LIBDOOMSDAY_PUBLIC dd_bool         P_MapExists(const char *uri);

/**
 * Determines whether the given @a uri references a known map, which, does
 * not originate form the currently loaded game.
 */
LIBDOOMSDAY_PUBLIC dd_bool         P_MapIsCustom(const char *uri);

/**
 * Determines whether the given @a uri references a known map and if so returns
 * the full path of the source file which contains it.
 *
 * @return  Fully qualified (i.e., absolute) path to the source file if known;
 *          otherwise a zero-length string.
 */
LIBDOOMSDAY_PUBLIC AutoStr *       P_MapSourceFile(const char *uri);

/**
 * Attempt to change the current map (will be loaded if necessary) to that
 * referenced by @a uri.
 *
 * @return  @c true= the current map was changed.
 */
LIBDOOMSDAY_PUBLIC dd_bool         P_MapChange(const char *uri);

// Lines

/**
 * Lines and Polyobj Lines (note Polyobj Lines are iterated first).
 *
 * The validCount flags are used to avoid checking lines that are marked in
 * multiple mapblocks, so increment validCount before the first call, then
 * make one or more calls to it.
 *
 * @param flags  @ref lineIteratorFlags
 */
LIBDOOMSDAY_PUBLIC int             Line_BoxIterator(const AABoxd *box, int flags, int (*callback) (world_Line *, void *), void *context);

LIBDOOMSDAY_PUBLIC int             Line_BoxOnSide(world_Line *line, const AABoxd *box);
LIBDOOMSDAY_PUBLIC int             Line_BoxOnSide_FixedPrecision(world_Line *line, const AABoxd *box);
LIBDOOMSDAY_PUBLIC coord_t         Line_PointDistance(world_Line *line, coord_t const point[2], coord_t *offset);
LIBDOOMSDAY_PUBLIC coord_t         Line_PointOnSide(const world_Line *line, coord_t const point[2]);
LIBDOOMSDAY_PUBLIC int             Line_TouchingMobjsIterator(world_Line *line, int (*callback) (struct mobj_s *, void *), void *context);
LIBDOOMSDAY_PUBLIC void            Line_Opening(world_Line *line, LineOpening *opening);

// Sectors

/**
 * Increment validCount before using this. 'func' is called for each mobj
 * that is (even partly) inside the sector. This is not a 3D test, the
 * mobjs may actually be above or under the sector.
 *
 * (Lovely name; actually this is a combination of SectorMobjs and
 * a bunch of LineMobjs iterations.)
 */
LIBDOOMSDAY_PUBLIC int             Sector_TouchingMobjsIterator(world_Sector *sector, int (*callback) (struct mobj_s *, void *), void *context);

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
LIBDOOMSDAY_PUBLIC world_Sector   *Sector_AtPoint_FixedPrecision(const coord_t point[2]);

// Map Objects

LIBDOOMSDAY_PUBLIC struct mobj_s  *Mobj_CreateXYZ(thinkfunc_t function, coord_t x, coord_t y, coord_t z, angle_t angle, coord_t radius, coord_t height, int ddflags);
LIBDOOMSDAY_PUBLIC void            Mobj_Destroy(struct mobj_s *mobj);
LIBDOOMSDAY_PUBLIC struct mobj_s  *Mobj_ById(int id);

/**
 * @note validCount should be incremented before calling this to begin a
 * new logical traversal. Otherwise Mobjs marked with a validCount equal
 * to this will be skipped over (can be used to avoid processing a mobj
 * multiple times during a complex and/or non-linear traversal.
 */
LIBDOOMSDAY_PUBLIC int             Mobj_BoxIterator(const AABoxd *box, int (*callback) (struct mobj_s *, void *), void *context);

/**
 * @param statenum  Must be a valid state (not null!).
 */
LIBDOOMSDAY_PUBLIC void            Mobj_SetState(struct mobj_s *mobj, int statenum);

/**
 * To be called after a move, to link the mobj back into the world.
 *
 * @param mobj   Mobj instance.
 * @param flags  @ref mobjLinkFlags
 */
LIBDOOMSDAY_PUBLIC void            Mobj_Link(struct mobj_s *mobj, int flags);

/**
 * Unlinks a mobj from the world so that it can be moved.
 *
 * @param mobj   Mobj instance.
 */
LIBDOOMSDAY_PUBLIC void            Mobj_Unlink(struct mobj_s *mobj);

/**
 * The callback function will be called once for each line that crosses
 * trough the object. This means all the lines will be two-sided.
 */
LIBDOOMSDAY_PUBLIC int             Mobj_TouchedLinesIterator(struct mobj_s *mobj, int (*callback) (world_Line *, void *), void *context);

/**
 * Increment validCount before calling this routine. The callback function
 * will be called once for each sector the mobj is touching (totally or
 * partly inside). This is not a 3D check; the mobj may actually reside
 * above or under the sector.
 */
LIBDOOMSDAY_PUBLIC int             Mobj_TouchedSectorsIterator(struct mobj_s *mobj, int (*callback) (world_Sector *, void *), void *context);

// Polyobjs

LIBDOOMSDAY_PUBLIC dd_bool         Polyobj_MoveXY(struct polyobj_s *po, coord_t x, coord_t y);

/**
 * Rotate @a polyobj in the map coordinate space.
 */
LIBDOOMSDAY_PUBLIC dd_bool         Polyobj_Rotate(struct polyobj_s *po, angle_t angle);

/**
 * Link @a polyobj to the current map. To be called after moving, rotating
 * or any other translation of the Polyobj within the map.
 */
LIBDOOMSDAY_PUBLIC void            Polyobj_Link(struct polyobj_s *po);

/**
 * Unlink @a polyobj from the current map. To be called prior to moving,
 * rotating or any other translation of the Polyobj within the map.
 */
LIBDOOMSDAY_PUBLIC void            Polyobj_Unlink(struct polyobj_s *po);

/**
 * Returns a pointer to the first Line in the polyobj.
 */
LIBDOOMSDAY_PUBLIC world_Line      *Polyobj_FirstLine(struct polyobj_s *po);

/**
 * Lookup a Polyobj on the current map by unique ID.
 *
 * @param id  Unique identifier of the Polyobj to be found.
 * @return  Found Polyobj instance else @c NULL.
 */
LIBDOOMSDAY_PUBLIC struct polyobj_s *Polyobj_ById(int id);

/**
 * Lookup a Polyobj on the current map by tag.
 *
 * @param tag  Tag associated with the Polyobj to be found.
 * @return  Found Polyobj instance, or @c NULL.
 */
LIBDOOMSDAY_PUBLIC struct polyobj_s *Polyobj_ByTag(int tag);

/**
 * @note validCount should be incremented before calling this to begin a
 * new logical traversal. Otherwise Polyobjs marked with a validCount equal
 * to this will be skipped over (can be used to avoid processing a polyobj
 * multiple times during a complex and/or non-linear traversal.
 */
LIBDOOMSDAY_PUBLIC int             Polyobj_BoxIterator(const AABoxd *box, int (*callback) (struct polyobj_s *, void *), void *context);

/**
 * The po_callback is called when a (any) polyobj hits a mobj.
 */
LIBDOOMSDAY_PUBLIC void            Polyobj_SetCallback(void (*func)(struct mobj_s *, void *, void *));

/**
 * @note validCount should be incremented before calling this to begin a
 * new logical traversal. Otherwise Polyobjs marked with a validCount equal
 * to this will be skipped over (can be used to avoid processing a polyobj
 * multiple times during a complex and/or non-linear traversal.
 */
LIBDOOMSDAY_PUBLIC int             Subspace_BoxIterator(const AABoxd *box, int (*callback) (struct convexsubspace_s *, void *), void *context);

// Traversers

LIBDOOMSDAY_PUBLIC int             P_PathTraverse(coord_t const from[2], coord_t const to[2], traverser_t callback, void *context);
LIBDOOMSDAY_PUBLIC int             P_PathTraverse2(coord_t const from[2], coord_t const to[2], int flags, traverser_t callback, void *context);

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
LIBDOOMSDAY_PUBLIC dd_bool P_CheckLineSight(coord_t const from[3],
                                            coord_t const to[3],
                                            coord_t       bottomSlope,
                                            coord_t       topSlope,
                                            int           flags);

/**
 * Provides read-only access to the origin in map space for the given @a trace.
 */
LIBDOOMSDAY_PUBLIC const coord_t * Interceptor_Origin(const world_Interceptor *trace);

/**
 * Provides read-only access to the direction in map space for the given @a trace.
 */
LIBDOOMSDAY_PUBLIC const coord_t * Interceptor_Direction(const world_Interceptor *trace);

/**
 * Provides read-only access to the line opening state for the given @a trace.
 */
LIBDOOMSDAY_PUBLIC const LineOpening *Interceptor_Opening(const world_Interceptor *trace);

/**
 * Update the "opening" state for the specified @a trace in accordance with
 * the heights defined by the minimal planes which intercept @a line.
 *
 * @return  @c true iff after the adjustment the opening range is positive,
 * i.e., the top Z coordinate is greater than the bottom Z.
 */
LIBDOOMSDAY_PUBLIC dd_bool         Interceptor_AdjustOpening(world_Interceptor *trace, world_Line *line);

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
LIBDOOMSDAY_PUBLIC const char *    DMU_Str(uint prop);

/**
 * Determines the type of the map data object.
 *
 * @param ptr  Pointer to a map data object.
 */
LIBDOOMSDAY_PUBLIC int             DMU_GetType(MapElementPtrConst ptr);

/**
 * Convert a pointer to DMU object to an element index.
 */
LIBDOOMSDAY_PUBLIC int             P_ToIndex(MapElementPtrConst ptr);

/**
 * Convert an element index to a DMU object pointer.
 */
LIBDOOMSDAY_PUBLIC void           *P_ToPtr(int type, int index);

/**
 * Returns the total number of DMU objects of @a type. For example, if the
 * type is @c DMU_LINE then the total number of map Lines is returned.
 */
LIBDOOMSDAY_PUBLIC int             P_Count(int type);

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
LIBDOOMSDAY_PUBLIC int             P_Callback(int type, int index, int (*callback)(MapElementPtr p, void *context), void *context);

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
LIBDOOMSDAY_PUBLIC int             P_Callbackp(int type, MapElementPtr pointer, int (*callback)(MapElementPtr p, void *context), void *context);

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
LIBDOOMSDAY_PUBLIC int             P_Iteratep(MapElementPtr pointer, uint prop, int (*callback) (MapElementPtr p, void *context), void *context);

/**
 * Allocates a new dummy object.
 *
 * @param type          DMU type of the dummy object.
 * @param extraData     Extra data pointer of the dummy. Points to
 *                      caller-allocated memory area of extra data for the
 *                      dummy.
 */
LIBDOOMSDAY_PUBLIC MapElementPtr   P_AllocDummy(int type, void *extraData);

/**
 * Frees a dummy object.
 */
LIBDOOMSDAY_PUBLIC void            P_FreeDummy(MapElementPtr dummy);

/**
 * Determines if a map data object is a dummy.
 */
LIBDOOMSDAY_PUBLIC dd_bool         P_IsDummy(MapElementPtrConst dummy);

/**
 * Returns the extra data pointer of the dummy, or NULL if the object is not
 * a dummy object.
 */
LIBDOOMSDAY_PUBLIC void           *P_DummyExtraData(MapElementPtr dummy);

// Map Entities
LIBDOOMSDAY_PUBLIC uint            P_CountMapObjs(int entityId);

/* index-based write functions */
LIBDOOMSDAY_PUBLIC void            P_SetBool(int type, int index, uint prop, dd_bool param);
LIBDOOMSDAY_PUBLIC void            P_SetByte(int type, int index, uint prop, byte param);
LIBDOOMSDAY_PUBLIC void            P_SetInt(int type, int index, uint prop, int param);
LIBDOOMSDAY_PUBLIC void            P_SetFixed(int type, int index, uint prop, fixed_t param);
LIBDOOMSDAY_PUBLIC void            P_SetAngle(int type, int index, uint prop, angle_t param);
LIBDOOMSDAY_PUBLIC void            P_SetFloat(int type, int index, uint prop, float param);
LIBDOOMSDAY_PUBLIC void            P_SetDouble(int type, int index, uint prop, double param);
LIBDOOMSDAY_PUBLIC void            P_SetPtr(int type, int index, uint prop, void *param);

LIBDOOMSDAY_PUBLIC void            P_SetBoolv(int type, int index, uint prop, dd_bool *params);
LIBDOOMSDAY_PUBLIC void            P_SetBytev(int type, int index, uint prop, byte *params);
LIBDOOMSDAY_PUBLIC void            P_SetIntv(int type, int index, uint prop, int *params);
LIBDOOMSDAY_PUBLIC void            P_SetFixedv(int type, int index, uint prop, fixed_t *params);
LIBDOOMSDAY_PUBLIC void            P_SetAnglev(int type, int index, uint prop, angle_t *params);
LIBDOOMSDAY_PUBLIC void            P_SetFloatv(int type, int index, uint prop, float *params);
LIBDOOMSDAY_PUBLIC void            P_SetDoublev(int type, int index, uint prop, double *params);
LIBDOOMSDAY_PUBLIC void            P_SetPtrv(int type, int index, uint prop, void *params);

/* pointer-based write functions */
LIBDOOMSDAY_PUBLIC void            P_SetBoolp(MapElementPtr ptr, uint prop, dd_bool param);
LIBDOOMSDAY_PUBLIC void            P_SetBytep(MapElementPtr ptr, uint prop, byte param);
LIBDOOMSDAY_PUBLIC void            P_SetIntp(MapElementPtr ptr, uint prop, int param);
LIBDOOMSDAY_PUBLIC void            P_SetFixedp(MapElementPtr ptr, uint prop, fixed_t param);
LIBDOOMSDAY_PUBLIC void            P_SetAnglep(MapElementPtr ptr, uint prop, angle_t param);
LIBDOOMSDAY_PUBLIC void            P_SetFloatp(MapElementPtr ptr, uint prop, float param);
LIBDOOMSDAY_PUBLIC void            P_SetDoublep(MapElementPtr ptr, uint prop, double param);
LIBDOOMSDAY_PUBLIC void            P_SetPtrp(MapElementPtr ptr, uint prop, void* param);

LIBDOOMSDAY_PUBLIC void            P_SetBoolpv(MapElementPtr ptr, uint prop, dd_bool *params);
LIBDOOMSDAY_PUBLIC void            P_SetBytepv(MapElementPtr ptr, uint prop, byte *params);
LIBDOOMSDAY_PUBLIC void            P_SetIntpv(MapElementPtr ptr, uint prop, int *params);
LIBDOOMSDAY_PUBLIC void            P_SetFixedpv(MapElementPtr ptr, uint prop, fixed_t *params);
LIBDOOMSDAY_PUBLIC void            P_SetAnglepv(MapElementPtr ptr, uint prop, angle_t *params);
LIBDOOMSDAY_PUBLIC void            P_SetFloatpv(MapElementPtr ptr, uint prop, float *params);
LIBDOOMSDAY_PUBLIC void            P_SetDoublepv(MapElementPtr ptr, uint prop, double *params);
LIBDOOMSDAY_PUBLIC void            P_SetPtrpv(MapElementPtr ptr, uint prop, void *params);

/* index-based read functions */
LIBDOOMSDAY_PUBLIC dd_bool         P_GetBool(int type, int index, uint prop);
LIBDOOMSDAY_PUBLIC byte            P_GetByte(int type, int index, uint prop);
LIBDOOMSDAY_PUBLIC int             P_GetInt(int type, int index, uint prop);
LIBDOOMSDAY_PUBLIC fixed_t         P_GetFixed(int type, int index, uint prop);
LIBDOOMSDAY_PUBLIC angle_t         P_GetAngle(int type, int index, uint prop);
LIBDOOMSDAY_PUBLIC float           P_GetFloat(int type, int index, uint prop);
LIBDOOMSDAY_PUBLIC double          P_GetDouble(int type, int index, uint prop);
LIBDOOMSDAY_PUBLIC void*           P_GetPtr(int type, int index, uint prop);

LIBDOOMSDAY_PUBLIC void            P_GetBoolv(int type, int index, uint prop, dd_bool *params);
LIBDOOMSDAY_PUBLIC void            P_GetBytev(int type, int index, uint prop, byte *params);
LIBDOOMSDAY_PUBLIC void            P_GetIntv(int type, int index, uint prop, int *params);
LIBDOOMSDAY_PUBLIC void            P_GetFixedv(int type, int index, uint prop, fixed_t *params);
LIBDOOMSDAY_PUBLIC void            P_GetAnglev(int type, int index, uint prop, angle_t *params);
LIBDOOMSDAY_PUBLIC void            P_GetFloatv(int type, int index, uint prop, float *params);
LIBDOOMSDAY_PUBLIC void            P_GetDoublev(int type, int index, uint prop, double *params);
LIBDOOMSDAY_PUBLIC void            P_GetPtrv(int type, int index, uint prop, void *params);

/* pointer-based read functions */
LIBDOOMSDAY_PUBLIC dd_bool         P_GetBoolp(MapElementPtr ptr, uint prop);
LIBDOOMSDAY_PUBLIC byte            P_GetBytep(MapElementPtr ptr, uint prop);
LIBDOOMSDAY_PUBLIC int             P_GetIntp(MapElementPtr ptr, uint prop);
LIBDOOMSDAY_PUBLIC fixed_t         P_GetFixedp(MapElementPtr ptr, uint prop);
LIBDOOMSDAY_PUBLIC angle_t         P_GetAnglep(MapElementPtr ptr, uint prop);
LIBDOOMSDAY_PUBLIC float           P_GetFloatp(MapElementPtr ptr, uint prop);
LIBDOOMSDAY_PUBLIC double          P_GetDoublep(MapElementPtr ptr, uint prop);
LIBDOOMSDAY_PUBLIC void*           P_GetPtrp(MapElementPtr ptr, uint prop);

LIBDOOMSDAY_PUBLIC void            P_GetBoolpv(MapElementPtr ptr, uint prop, dd_bool *params);
LIBDOOMSDAY_PUBLIC void            P_GetBytepv(MapElementPtr ptr, uint prop, byte *params);
LIBDOOMSDAY_PUBLIC void            P_GetIntpv(MapElementPtr ptr, uint prop, int *params);
LIBDOOMSDAY_PUBLIC void            P_GetFixedpv(MapElementPtr ptr, uint prop, fixed_t *params);
LIBDOOMSDAY_PUBLIC void            P_GetAnglepv(MapElementPtr ptr, uint prop, angle_t *params);
LIBDOOMSDAY_PUBLIC void            P_GetFloatpv(MapElementPtr ptr, uint prop, float *params);
LIBDOOMSDAY_PUBLIC void            P_GetDoublepv(MapElementPtr ptr, uint prop, double *params);
LIBDOOMSDAY_PUBLIC void            P_GetPtrpv(MapElementPtr ptr, uint prop, void *params);

#ifdef __cplusplus
} // extern "C"
#endif
