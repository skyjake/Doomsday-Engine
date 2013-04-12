/** @file api_mapedit.h Public API for creating maps.
 * @ingroup map
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DOOMSDAY_API_MAP_EDIT_H
#define DOOMSDAY_API_MAP_EDIT_H

#include <de/str.h>
#include <de/types.h>
#include "api_base.h"

/// @defgroup mapEdit Map Editor
/// @ingroup map
///@{

/// Value types.
typedef enum {
    DDVT_NONE = -1, ///< Not a read/writeable value type.
    DDVT_BOOL,
    DDVT_BYTE,
    DDVT_SHORT,
    DDVT_INT,       ///< 32 or 64 bit
    DDVT_UINT,
    DDVT_FIXED,
    DDVT_ANGLE,
    DDVT_FLOAT,
    DDVT_DOUBLE,
    DDVT_LONG,
    DDVT_ULONG,
    DDVT_PTR,
    DDVT_BLENDMODE
} valuetype_t;

DENG_API_TYPEDEF(MPE)
{
    de_api_t api;

    /**
     * Called by the game to register the map object types it wishes us to make
     * public via the MPE interface.
     */
    boolean         (*RegisterMapObj)(int identifier, const char* name);

    /**
     * Called by the game to add a new property to a previously registered
     * map object type definition.
     */
    boolean         (*RegisterMapObjProperty)(int identifier, int propIdentifier, const char* propName, valuetype_t type);

    /**
     * To be called to begin the map building process.
     */
    boolean         (*Begin)(const char* mapUri);

    /**
     * To be called to end the map building process.
     */
    boolean         (*End)(void);

    /**
     * Create a new vertex in currently loaded editable map.
     *
     * @param x  X map space coordinate of the new vertex.
     * @param y  Y map space coordinate of the new vertex.
     *
     * @return  Index number of the newly created vertex else @c =0 if the vertex
     *          could not be created for some reason.
     */
    uint            (*VertexCreate)(coord_t x, coord_t y);

    /**
     * Create many new vertices in the currently loaded editable map.
     *
     * @param num  Number of vertexes to be created.
     * @param values  Array containing the coordinates for all vertexes to be
     *                created [v0:X, vo:Y, v1:X, v1:Y, ..]
     * @param indices  If not @c =NULL, the indices of the newly created vertexes
     *                 will be written back here.
     * @return  @c =true iff all vertexes were created successfully.
     */
    boolean         (*VertexCreatev)(size_t num, coord_t* values, uint* indices);

    /**
     * Create a new linedef in the editable map.
     *
     * @param v1            Idx of the start vertex.
     * @param v2            Idx of the end vertex.
     * @param frontSector   Idx of the front sector.
     * @param backSector    Idx of the back sector.
     * @param frontSide     Idx of the front sidedef.
     * @param backSide      Idx of the back sidedef.
     * @param flags         DDLF_* flags.
     *
     * @return  Index of the newly created linedef else @c 0 if there was an error.
     */
    uint            (*LinedefCreate)(uint v1, uint v2, uint frontSector, uint backSector, int flags);
    void            (*LinedefAddSide)(uint line, int side, short flags, const ddstring_t* topMaterial, float topOffsetX, float topOffsetY, float topRed, float topGreen, float topBlue, const ddstring_t* middleMaterial, float middleOffsetX, float middleOffsetY, float middleRed, float middleGreen, float middleBlue, float middleAlpha, const ddstring_t* bottomMaterial, float bottomOffsetX, float bottomOffsetY, float bottomRed, float bottomGreen, float bottomBlue);
    uint            (*SectorCreate)(float lightlevel, float red, float green, float blue);
    uint            (*PlaneCreate)(uint sector, coord_t height, const ddstring_t* materialUri, float matOffsetX, float matOffsetY, float r, float g, float b, float a, float normalX, float normalY, float normalZ);
    uint            (*PolyobjCreate)(uint* lines, uint linecount, int tag, int sequenceType, coord_t originX, coord_t originY);
    boolean         (*GameObjProperty)(const char* objName, uint idx, const char* propName, valuetype_t type, void* data);
}
DENG_API_T(MPE);

#ifndef DENG_NO_API_MACROS_MAP_EDIT
#define P_RegisterMapObj    _api_MPE.RegisterMapObj
#define P_RegisterMapObjProperty _api_MPE.RegisterMapObjProperty
#define MPE_Begin           _api_MPE.Begin
#define MPE_End             _api_MPE.End
#define MPE_VertexCreate    _api_MPE.VertexCreate
#define MPE_VertexCreatev   _api_MPE.VertexCreatev
#define MPE_LinedefCreate   _api_MPE.LinedefCreate
#define MPE_LinedefAddSide  _api_MPE.LinedefAddSide
#define MPE_SectorCreate    _api_MPE.SectorCreate
#define MPE_PlaneCreate     _api_MPE.PlaneCreate
#define MPE_PolyobjCreate   _api_MPE.PolyobjCreate
#define MPE_GameObjProperty _api_MPE.GameObjProperty
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(MPE);
#endif

///@}

#endif // DOOMSDAY_API_MAP_EDIT_H
