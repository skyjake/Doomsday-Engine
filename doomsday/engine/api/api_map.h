/** @file api_map.h Map data.
 *
 * World data comprises the map and all the objects in it. The public API
 * includes accessing and modifying map data objects via DMU.
 *
 * @ingroup map
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef __DOOMSDAY_MAP_H__
#define __DOOMSDAY_MAP_H__

#include "dd_share.h"
#include <de/mathutil.h>

struct mobj_s;
struct linedef_s;
struct sector_s;
struct bspleaf_s;

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
typedef struct bspnode_s  { int type; } BspNode;
typedef struct vertex_s   { int type; } Vertex;
typedef struct linedef_s  { int type; } LineDef;
typedef struct sidedef_s  { int type; } SideDef;
typedef struct hedge_s    { int type; } HEdge;
typedef struct bspleaf_s  { int type; } BspLeaf;
typedef struct sector_s   { int type; } Sector;
typedef struct plane_s    { int type; } Plane;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup map
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
    boolean         (*Exists)(char const* uri);

    boolean         (*IsCustom)(char const* uri);

    /**
     * Retrieve the name of the source file containing the map referenced by @a
     * uri if known and available for loading.
     *
     * @param uri  Uri identifying the map to be searched for.
     * @return  Fully qualified (i.e., absolute) path to the source file.
     */
    AutoStr*        (*SourceFile)(char const* uri);

    /**
     * Begin the process of loading a new map.
     *
     * @param uri  Uri identifying the map to be loaded.
     * @return @c true, if the map was loaded successfully.
     */
    boolean         (*Load)(const char* uri);

    // Lines

    int             (*LD_BoxOnSide)(struct linedef_s* line, const AABoxd* box);
    int             (*LD_BoxOnSide_FixedPrecision)(struct linedef_s* line, const AABoxd* box);
    coord_t         (*LD_PointDistance)(struct linedef_s* lineDef, coord_t const point[2], coord_t* offset);
    coord_t         (*LD_PointXYDistance)(struct linedef_s* lineDef, coord_t x, coord_t y, coord_t* offset);
    coord_t         (*LD_PointOnSide)(struct linedef_s const *lineDef, coord_t const point[2]);
    coord_t         (*LD_PointXYOnSide)(struct linedef_s const *lineDef, coord_t x, coord_t y);
    int             (*LD_MobjsIterator)(struct linedef_s* line, int (*callback) (struct mobj_s*, void *), void* parameters);

    // Sectors

    int             (*S_TouchingMobjsIterator)(struct sector_s* sector, int (*callback) (struct mobj_s*, void*), void* parameters);

    // Map Objects

    struct mobj_s*  (*MO_CreateXYZ)(thinkfunc_t function, coord_t x, coord_t y, coord_t z, angle_t angle, coord_t radius, coord_t height, int ddflags);
    void            (*MO_Destroy)(struct mobj_s* mo);
    struct mobj_s*  (*MO_MobjForID)(int id);
    void            (*MO_SetState)(struct mobj_s* mo, int statenum);
    void            (*MO_Link)(struct mobj_s* mo, byte flags);
    int             (*MO_Unlink)(struct mobj_s* mo);
    void            (*MO_SpawnDamageParticleGen)(struct mobj_s* mo, struct mobj_s* inflictor, int amount);

    /**
     * The callback function will be called once for each line that crosses
     * trough the object. This means all the lines will be two-sided.
     */
    int             (*MO_LinesIterator)(struct mobj_s* mo, int (*callback) (struct linedef_s*, void*), void* parameters);

    /**
     * Increment validCount before calling this routine. The callback function
     * will be called once for each sector the mobj is touching (totally or
     * partly inside). This is not a 3D check; the mobj may actually reside
     * above or under the sector.
     */
    int             (*MO_SectorsIterator)(struct mobj_s* mo, int (*callback) (struct sector_s*, void*), void* parameters);

    /**
     * Calculate the visible @a origin of @a mobj in world space, including
     * any short range offset.
     */
    void            (*MO_OriginSmoothed)(struct mobj_s* mobj, coord_t origin[3]);
    angle_t         (*MO_AngleSmoothed)(struct mobj_s* mobj);

    // Polyobjs

    boolean         (*PO_MoveXY)(struct polyobj_s* po, coord_t x, coord_t y);

    /**
     * Rotate @a polyobj in the map coordinate space.
     */
    boolean         (*PO_Rotate)(struct polyobj_s* po, angle_t angle);

    /**
     * Link @a polyobj to the current map. To be called after moving, rotating
     * or any other translation of the Polyobj within the map.
     */
    void            (*PO_Link)(struct polyobj_s* po);

    /**
     * Unlink @a polyobj from the current map. To be called prior to moving,
     * rotating or any other translation of the Polyobj within the map.
     */
    void            (*PO_Unlink)(struct polyobj_s* po);

    /**
     * Lookup a Polyobj on the current map by unique ID.
     *
     * @param id  Unique identifier of the Polyobj to be found.
     * @return  Found Polyobj instance else @c NULL.
     */
    struct polyobj_s* (*PO_PolyobjByID)(uint id);

    /**
     * Lookup a Polyobj on the current map by tag.
     *
     * @param tag  Tag associated with the Polyobj to be found.
     * @return  Found Polyobj instance, or @c NULL.
     */
    struct polyobj_s* (*PO_PolyobjByTag)(int tag);

    /**
     * The po_callback is called when a (any) polyobj hits a mobj.
     */
    void            (*PO_SetCallback)(void (*func)(struct mobj_s*, void*, void*));

    // BSP Leaves

    struct bspleaf_s* (*BL_AtPoint)(coord_t const point[2]);

    /**
     * Determine the BSP leaf on the back side of the BS partition that lies in
     * front of the specified point within the CURRENT map's coordinate space.
     *
     * Always returns a valid BspLeaf although the point may not actually lay
     * within it (however it is on the same side of the space partition!).
     *
     * @param x  X coordinate of the point to test.
     * @param y  Y coordinate of the point to test.
     *
     * @return  BspLeaf instance for that BSP node's leaf.
     */
    struct bspleaf_s* (*BL_AtPointXY)(coord_t x, coord_t y);

    // Iterators

    int             (*Box_MobjsIterator)(const AABoxd* box, int (*callback) (struct mobj_s*, void*), void* parameters);
    int             (*Box_LinesIterator)(const AABoxd* box, int (*callback) (struct linedef_s*, void*), void* parameters);

    /**
     * LineDefs and Polyobj LineDefs (note Polyobj LineDefs are iterated first).
     *
     * The validCount flags are used to avoid checking lines that are marked
     * in multiple mapblocks, so increment validCount before the first call
     * to GameMap_IterateCellLineDefs(), then make one or more calls to it.
     */
    int             (*Box_AllLinesIterator)(const AABoxd* box, int (*callback) (struct linedef_s*, void*), void* parameters);

    /**
     * The validCount flags are used to avoid checking polys that are marked in
     * multiple mapblocks, so increment validCount before the first call, then
     * make one or more calls to it.
     */
    int             (*Box_PolyobjLinesIterator)(const AABoxd* box, int (*callback) (struct linedef_s*, void*), void* parameters);

    int             (*Box_BspLeafsIterator)(const AABoxd* box, struct sector_s* sector, int (*callback) (struct bspleaf_s*, void*), void* parameters);
    int             (*Box_PolyobjsIterator)(const AABoxd* box, int (*callback) (struct polyobj_s*, void*), void* parameters);
    int             (*PathTraverse2)(coord_t const from[2], coord_t const to[2], int flags, traverser_t callback, void* parameters);
    int             (*PathTraverse)(coord_t const from[2], coord_t const to[2], int flags, traverser_t callback/*parameters=NULL*/);

    /**
     * Same as P_PathTraverse except 'from' and 'to' arguments are specified
     * as two sets of separate X and Y map space coordinates.
     */
    int             (*PathXYTraverse2)(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY, int flags, traverser_t callback, void* parameters);
    int             (*PathXYTraverse)(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY, int flags, traverser_t callback/*parameters=NULL*/);

    boolean         (*CheckLineSight)(coord_t const from[3], coord_t const to[3], coord_t bottomSlope, coord_t topSlope, int flags);

    /**
     * Retrieve an immutable copy of the LOS trace line for the CURRENT map.
     *
     * @note Always returns a valid divline_t even if there is no current map.
     */
    Divline const * (*TraceLOS)(void);

    /**
     * Retrieve an immutable copy of the TraceOpening state for the CURRENT map.
     *
     * @note Always returns a valid TraceOpening even if there is no current map.
     */
    TraceOpening const *(*traceOpening)(void);

    /**
     * Update the TraceOpening state for the CURRENT map according to the opening
     * defined by the inner-minimal planes heights which intercept @a linedef
     */
    void            (*SetTraceOpening)(struct linedef_s* linedef);

    // Map Updates (DMU)

    /**
     * Determines the type of the map data object.
     *
     * @param ptr  Pointer to a map data object.
     */
    int             (*GetType)(const void* ptr);

    unsigned int    (*ToIndex)(const void* ptr);
    void*           (*ToPtr)(int type, uint index);
    int             (*Callback)(int type, uint index, void* context, int (*callback)(void* p, void* ctx));
    int             (*Callbackp)(int type, void* ptr, void* context, int (*callback)(void* p, void* ctx));
    int             (*Iteratep)(void *ptr, uint prop, void* context, int (*callback) (void* p, void* ctx));

    // Dummy Objects
    void*           (*AllocDummy)(int type, void* extraData);
    void            (*FreeDummy)(void* dummy);
    boolean         (*IsDummy)(void* dummy);
    void*           (*DummyExtraData)(void* dummy);

    // Map Entities
    uint            (*CountGameMapObjs)(int entityId);
    byte            (*GetGMOByte)(int entityId, uint elementIndex, int propertyId);
    short           (*GetGMOShort)(int entityId, uint elementIndex, int propertyId);
    int             (*GetGMOInt)(int entityId, uint elementIndex, int propertyId);
    fixed_t         (*GetGMOFixed)(int entityId, uint elementIndex, int propertyId);
    angle_t         (*GetGMOAngle)(int entityId, uint elementIndex, int propertyId);
    float           (*GetGMOFloat)(int entityId, uint elementIndex, int propertyId);

    ///@}

    /// @addtogroup dmu
    ///@{

    /* index-based write functions */
    void            (*SetBool)(int type, uint index, uint prop, boolean param);
    void            (*SetByte)(int type, uint index, uint prop, byte param);
    void            (*SetInt)(int type, uint index, uint prop, int param);
    void            (*SetFixed)(int type, uint index, uint prop, fixed_t param);
    void            (*SetAngle)(int type, uint index, uint prop, angle_t param);
    void            (*SetFloat)(int type, uint index, uint prop, float param);
    void            (*SetDouble)(int type, uint index, uint prop, double param);
    void            (*SetPtr)(int type, uint index, uint prop, void* param);

    void            (*SetBoolv)(int type, uint index, uint prop, boolean* params);
    void            (*SetBytev)(int type, uint index, uint prop, byte* params);
    void            (*SetIntv)(int type, uint index, uint prop, int* params);
    void            (*SetFixedv)(int type, uint index, uint prop, fixed_t* params);
    void            (*SetAnglev)(int type, uint index, uint prop, angle_t* params);
    void            (*SetFloatv)(int type, uint index, uint prop, float* params);
    void            (*SetDoublev)(int type, uint index, uint prop, double* params);
    void            (*SetPtrv)(int type, uint index, uint prop, void* params);

    /* pointer-based write functions */
    void            (*SetBoolp)(void* ptr, uint prop, boolean param);
    void            (*SetBytep)(void* ptr, uint prop, byte param);
    void            (*SetIntp)(void* ptr, uint prop, int param);
    void            (*SetFixedp)(void* ptr, uint prop, fixed_t param);
    void            (*SetAnglep)(void* ptr, uint prop, angle_t param);
    void            (*SetFloatp)(void* ptr, uint prop, float param);
    void            (*SetDoublep)(void* ptr, uint prop, double param);
    void            (*SetPtrp)(void* ptr, uint prop, void* param);

    void            (*SetBoolpv)(void* ptr, uint prop, boolean* params);
    void            (*SetBytepv)(void* ptr, uint prop, byte* params);
    void            (*SetIntpv)(void* ptr, uint prop, int* params);
    void            (*SetFixedpv)(void* ptr, uint prop, fixed_t* params);
    void            (*SetAnglepv)(void* ptr, uint prop, angle_t* params);
    void            (*SetFloatpv)(void* ptr, uint prop, float* params);
    void            (*SetDoublepv)(void* ptr, uint prop, double* params);
    void            (*SetPtrpv)(void* ptr, uint prop, void* params);

    /* index-based read functions */
    boolean         (*GetBool)(int type, uint index, uint prop);
    byte            (*GetByte)(int type, uint index, uint prop);
    int             (*GetInt)(int type, uint index, uint prop);
    fixed_t         (*GetFixed)(int type, uint index, uint prop);
    angle_t         (*GetAngle)(int type, uint index, uint prop);
    float           (*GetFloat)(int type, uint index, uint prop);
    double          (*GetDouble)(int type, uint index, uint prop);
    void*           (*GetPtr)(int type, uint index, uint prop);

    void            (*GetBoolv)(int type, uint index, uint prop, boolean* params);
    void            (*GetBytev)(int type, uint index, uint prop, byte* params);
    void            (*GetIntv)(int type, uint index, uint prop, int* params);
    void            (*GetFixedv)(int type, uint index, uint prop, fixed_t* params);
    void            (*GetAnglev)(int type, uint index, uint prop, angle_t* params);
    void            (*GetFloatv)(int type, uint index, uint prop, float* params);
    void            (*GetDoublev)(int type, uint index, uint prop, double* params);
    void            (*GetPtrv)(int type, uint index, uint prop, void* params);

    /* pointer-based read functions */
    boolean         (*GetBoolp)(void* ptr, uint prop);
    byte            (*GetBytep)(void* ptr, uint prop);
    int             (*GetIntp)(void* ptr, uint prop);
    fixed_t         (*GetFixedp)(void* ptr, uint prop);
    angle_t         (*GetAnglep)(void* ptr, uint prop);
    float           (*GetFloatp)(void* ptr, uint prop);
    double          (*GetDoublep)(void* ptr, uint prop);
    void*           (*GetPtrp)(void* ptr, uint prop);

    void            (*GetBoolpv)(void* ptr, uint prop, boolean* params);
    void            (*GetBytepv)(void* ptr, uint prop, byte* params);
    void            (*GetIntpv)(void* ptr, uint prop, int* params);
    void            (*GetFixedpv)(void* ptr, uint prop, fixed_t* params);
    void            (*GetAnglepv)(void* ptr, uint prop, angle_t* params);
    void            (*GetFloatpv)(void* ptr, uint prop, float* params);
    void            (*GetDoublepv)(void* ptr, uint prop, double* params);
    void            (*GetPtrpv)(void* ptr, uint prop, void* params);
}
DENG_API_T(Map);

#ifndef DENG_NO_API_MACROS_MAP
#define P_MapExists                         _api_Map.Exists
#define P_MapIsCustom                       _api_Map.IsCustom
#define P_MapSourceFile                     _api_Map.SourceFile
#define P_LoadMap                           _api_Map.Load

#define LineDef_BoxOnSide                   _api_Map.LD_BoxOnSide
#define LineDef_BoxOnSide_FixedPrecision    _api_Map.LD_BoxOnSide_FixedPrecision
#define LineDef_PointDistance               _api_Map.LD_PointDistance
#define LineDef_PointXYDistance             _api_Map.LD_PointXYDistance
#define LineDef_PointOnSide                 _api_Map.LD_PointOnSide
#define LineDef_PointXYOnSide               _api_Map.LD_PointXYOnSide
#define P_LineMobjsIterator                 _api_Map.LD_MobjsIterator

#define P_SectorTouchingMobjsIterator       _api_Map.S_TouchingMobjsIterator

#define P_MobjCreateXYZ                     _api_Map.MO_CreateXYZ
#define P_MobjDestroy                       _api_Map.MO_Destroy
#define P_MobjForID                         _api_Map.MO_MobjForID
#define P_MobjSetState                      _api_Map.MO_SetState
#define P_MobjLink                          _api_Map.MO_Link
#define P_MobjUnlink                        _api_Map.MO_Unlink
#define P_SpawnDamageParticleGen            _api_Map.MO_SpawnDamageParticleGen
#define P_MobjLinesIterator                 _api_Map.MO_LinesIterator
#define P_MobjSectorsIterator               _api_Map.MO_SectorsIterator
#define Mobj_AngleSmoothed                  _api_Map.MO_AngleSmoothed
#define Mobj_OriginSmoothed                 _api_Map.MO_OriginSmoothed

#define P_PolyobjMoveXY                     _api_Map.PO_MoveXY
#define P_PolyobjRotate                     _api_Map.PO_Rotate
#define P_PolyobjLink                       _api_Map.PO_Link
#define P_PolyobjUnlink                     _api_Map.PO_Unlink
#define P_PolyobjByID                       _api_Map.PO_PolyobjByID
#define P_PolyobjByTag                      _api_Map.PO_PolyobjByTag
#define P_SetPolyobjCallback                _api_Map.PO_SetCallback

#define P_BspLeafAtPoint                    _api_Map.BL_AtPoint
#define P_BspLeafAtPointXY                  _api_Map.BL_AtPointXY

#define P_MobjsBoxIterator                  _api_Map.Box_MobjsIterator
#define P_LinesBoxIterator                  _api_Map.Box_LinesIterator
#define P_AllLinesBoxIterator               _api_Map.Box_AllLinesIterator
#define P_PolyobjLinesBoxIterator			_api_Map.Box_PolyobjLinesIterator
#define P_BspLeafsBoxIterator               _api_Map.Box_BspLeafsIterator
#define P_PolyobjsBoxIterator               _api_Map.Box_PolyobjsIterator
#define P_PathTraverse2                     _api_Map.PathTraverse2
#define P_PathTraverse                      _api_Map.PathTraverse
#define P_PathXYTraverse2                   _api_Map.PathXYTraverse2
#define P_PathXYTraverse                    _api_Map.PathXYTraverse
#define P_CheckLineSight                    _api_Map.CheckLineSight
#define P_TraceLOS                          _api_Map.TraceLOS
#define P_TraceOpening                      _api_Map.traceOpening
#define P_SetTraceOpening                   _api_Map.SetTraceOpening

#define DMU_GetType                         _api_Map.GetType
#define P_ToIndex                           _api_Map.ToIndex
#define P_ToPtr                             _api_Map.ToPtr
#define P_Callback                          _api_Map.Callback
#define P_Callbackp                         _api_Map.Callbackp
#define P_Iteratep                          _api_Map.Iteratep
#define P_AllocDummy                        _api_Map.AllocDummy
#define P_FreeDummy                         _api_Map.FreeDummy
#define P_IsDummy                           _api_Map.IsDummy
#define P_DummyExtraData                    _api_Map.DummyExtraData
#define P_CountGameMapObjs                  _api_Map.CountGameMapObjs
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

///@}

#endif // __DOOMSDAY_MAP_H__
