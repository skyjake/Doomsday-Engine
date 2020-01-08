/** @file api_mapedit.h Public API for creating maps.
 * @ingroup world
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <doomsday/world/valuetype.h>
#include "api_base.h"

/// @defgroup mapEdit Map Editor
/// @ingroup world
///@{

struct de_api_side_section_s
{
    const char *material;
    float       offset[2];
    float       color[4];
};

struct de_api_sector_hacks_s
{
    struct {
        int linkFloorPlane       : 1;
        int linkCeilingPlane     : 1;
        int missingInsideTop     : 1;
        int missingInsideBottom  : 1;
        int missingOutsideTop    : 1;
        int missingOutsideBottom : 1;
    } flags;
    int visPlaneLinkTargetSector;
};

DENG_API_TYPEDEF(MPE)
{
    de_api_t api;

    /**
     * Called by the game to register the map object types it wishes us to make
     * public via the MPE interface.
     */
    //dd_bool         (*RegisterMapObj)(int identifier, char const *name);

    /**
     * Called by the game to add a new property to a previously registered
     * map object type definition.
     */
    //dd_bool         (*RegisterMapObjProperty)(int identifier, int propIdentifier, char const *propName, valuetype_t type);

    /**
     * To be called to begin the map building process.
     */
    dd_bool         (*Begin)(Uri const *mapUri);

    /**
     * To be called to end the map building process.
     */
    dd_bool         (*End)(void);

    /**
     * Create a new vertex in currently loaded editable map.
     *
     * @param x             X map space coordinate of the new vertex.
     * @param y             Y map space coordinate of the new vertex.
     * @param archiveIndex  Archive index for the vertex. Set to @c -1 if
     *                      not relevant/known.
     *
     * @return  Index number of the newly created vertex; otherwise @c -1 if
     * the vertex could not be created for some reason.
     */
    int             (*VertexCreate)(coord_t x, coord_t y, int archiveIndex);

    /**
     * Create many new vertices in the currently loaded editable map.
     *
     * @param num             Number of vertexes to be created.
     * @param values          Array containing the coordinates for all vertexes
     *                        to be created [v0:X, vo:Y, v1:X, v1:Y, ..]
     * @param archiveIndices  Array containing the archive indices for each
     *                        vertex. Can be @c 0.
     * @param indices         If not @c 0, the indices of the newly created
     *                        vertexes will be written back here.
     * @return  @c =true iff all vertexes were created successfully.
     */
    dd_bool         (*VertexCreatev)(int num, coord_t const *values, int *archiveIndices, int *indices);

    /**
     * Create a new line in the editable map.
     *
     * @param v1            Idx of the start vertex.
     * @param v2            Idx of the end vertex.
     * @param frontSector   Idx of the front sector.
     * @param backSector    Idx of the back sector.
     * @param frontSide     Idx of the front side.
     * @param backSide      Idx of the back side.
     * @param flags         DDLF_* flags.
     * @param archiveIndex  Archive index for the line. Set to @c -1 if
     *                      not relevant/known.
     *
     * @return  Index of the newly created line else @c -1 if there was an error.
     */
    int             (*LineCreate)(int v1, int v2, int frontSector, int backSector, int flags, int archiveIndex);
    void            (*LineAddSide)(int line, int side, short flags,
                        const struct de_api_side_section_s *top,
                        const struct de_api_side_section_s *middle,
                        const struct de_api_side_section_s *bottom,
                        int archiveIndex);
    int             (*SectorCreate)(float lightlevel, float red, float green, float blue,
                        const struct de_api_sector_hacks_s *hacks,
                        int archiveIndex);
    int             (*PlaneCreate)(int sector, coord_t height, const char *materialUri, float matOffsetX, float matOffsetY, float r, float g, float b, float a, float normalX, float normalY, float normalZ, int archiveIndex);
    int             (*PolyobjCreate)(int const *lines, int linecount, int tag, int sequenceType, coord_t originX, coord_t originY, int archiveIndex);
    dd_bool         (*GameObjProperty)(char const *objName, int idx, char const *propName, valuetype_t type, void *data);
}
DENG_API_T(MPE);

#ifndef DENG_NO_API_MACROS_MAP_EDIT
//#define P_RegisterMapObj    _api_MPE.RegisterMapObj
//#define P_RegisterMapObjProperty _api_MPE.RegisterMapObjProperty
#define MPE_Begin           _api_MPE.Begin
#define MPE_End             _api_MPE.End
#define MPE_VertexCreate    _api_MPE.VertexCreate
#define MPE_VertexCreatev   _api_MPE.VertexCreatev
#define MPE_LineCreate      _api_MPE.LineCreate
#define MPE_LineAddSide     _api_MPE.LineAddSide
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
