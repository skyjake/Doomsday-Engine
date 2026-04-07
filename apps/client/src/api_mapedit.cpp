/** @file api_mapedit.cpp  Internal runtime map editing interface.
 *
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#define DE_NO_API_MACROS_MAP_EDIT

#if defined(__CLIENT__)
#include "de_platform.h"
#include "api_mapedit.h"
#include "world/map.h"
#include "world/polyobjdata.h"
#include "world/line.h"
#include "world/plane.h"
#include "world/surface.h"
#include "edit_map.h"
#include "dd_main.h"
#endif

#if defined(__SERVER__)
#include <doomsday/world/map.h>
#include <doomsday/world/surface.h>
#include <doomsday/world/polyobjdata.h>
#include <doomsday/res/resources.h>
#include <doomsday/gamefw/defs.h>
#endif

#include <doomsday/world/entitydef.h>
#include <doomsday/world/factory.h>
#include <doomsday/world/mapbuilder.h>
#include <doomsday/world/materials.h>
#include <doomsday/world/sector.h>
#include <doomsday/world/entitydatabase.h>
#include <de/error.h>
#include <de/log.h>
#include <de/stringpool.h>

using namespace de;
using world::editMap;

world::Map *MPE_Map()
{
    return editMap;
}

world::Map *MPE_TakeMap()
{
    return editMap.take();
}

#undef MPE_Begin
dd_bool MPE_Begin(const uri_s * /*mapUri*/)
{
    editMap.begin();
    return true;
}

#undef MPE_End
dd_bool MPE_End()
{
    if (!editMap) return false;
    editMap.end();
    return true;
}

#undef MPE_VertexCreate
int MPE_VertexCreate(coord_t x, coord_t y, int archiveIndex)
{
    return editMap->createVertex(Vec2d(x, y), archiveIndex)->indexInMap();
}

#undef MPE_VertexCreatev
dd_bool MPE_VertexCreatev(int num, const coord_t *values, int *archiveIndices, int *retIndices)
{
    if (num <= 0 || !values)
        return false;

    // Create many vertexes.
    for (int n = 0; n < num; ++n)
    {
        auto *vertex = editMap->createVertex(Vec2d(values[n * 2], values[n * 2 + 1]),
                                             archiveIndices[n]);
        if (retIndices)
        {
            retIndices[n] = vertex->indexInMap();
        }
    }

    return true;
}

#undef MPE_LineCreate
int MPE_LineCreate(int v1, int v2, int frontSectorIdx, int backSectorIdx, int flags,
                   int archiveIndex)
{
    if(frontSectorIdx >= editMap->editableSectorCount()) return -1;
    if(backSectorIdx  >= editMap->editableSectorCount()) return -1;
    if(v1 < 0 || v1 >= editMap->vertexCount()) return -1;
    if(v2 < 0 || v2 >= editMap->vertexCount()) return -1;
    if(v1 == v2) return -1;

    // Next, check the length is not zero.
    /// @todo fixme: We need to allow these... -ds
    auto &vtx1 = editMap->vertex(v1);
    auto &vtx2 = editMap->vertex(v2);
    if(de::abs(Vec2d(vtx1.origin() - vtx2.origin()).length()) <= 0.0001) return -1;

    auto *frontSector = (frontSectorIdx >= 0? editMap->editableSectors().at(frontSectorIdx) : 0);
    auto *backSector  = (backSectorIdx  >= 0? editMap->editableSectors().at(backSectorIdx) : 0);

    return editMap->createLine(vtx1, vtx2, flags, frontSector, backSector, archiveIndex)
                        ->indexInMap();
}

#undef MPE_LineAddSide
void MPE_LineAddSide(int lineIdx, int sideId, short flags,
                     const de_api_side_section_s *top,
                     const de_api_side_section_s *middle,
                     const de_api_side_section_s *bottom,
                     int archiveIndex)
{
    if(lineIdx < 0 || lineIdx >= editMap->editableLineCount()) return;

    auto *line = editMap->editableLines().at(lineIdx);
    auto &side = line->side(sideId);

    side.setFlags(flags);
    side.setIndexInArchive(archiveIndex);

    // Ensure sections are defined if they aren't already.
    side.addSections();

    // Assign the resolved material if found.
    side.top()
        .setMaterial(editMap.findMaterialInDict(top->material))
        .setOrigin(Vec2f(top->offset[VX], top->offset[VY]))
        .setColor(Vec3f(top->color[CR], top->color[CG], top->color[CB]));

    side.middle()
        .setMaterial(editMap.findMaterialInDict(middle->material))
        .setOrigin(Vec2f(middle->offset[VX], middle->offset[VY]))
        .setColor(Vec3f(middle->color[CR], middle->color[CG], middle->color[CB]))
        .setOpacity(middle->color[CA]);

    side.bottom()
        .setMaterial(editMap.findMaterialInDict(bottom->material))
        .setOrigin(Vec2f(bottom->offset[VX], bottom->offset[VY]))
        .setColor(Vec3f(bottom->color[CR], bottom->color[CG], bottom->color[CB]));
}

#undef MPE_PlaneCreate
int MPE_PlaneCreate(int sectorIdx, coord_t height, const char *materialUri,
    float matOffsetX, float matOffsetY, float tintRed, float tintGreen, float tintBlue, float opacity,
    float normalX, float normalY, float normalZ, int archiveIndex)
{
    if(sectorIdx < 0 || sectorIdx >= editMap->editableSectorCount()) return -1;

    auto *sector = editMap->editableSectors().at(sectorIdx);
    auto *plane = sector->addPlane(Vec3f(normalX, normalY, normalZ), height);

    plane->setIndexInArchive(archiveIndex);

    plane->surface()
        .setMaterial(editMap.findMaterialInDict(materialUri))
        .setColor({tintRed, tintGreen, tintBlue})
        .setOrigin({matOffsetX, matOffsetY});

    if (!plane->isSectorFloor() && !plane->isSectorCeiling())
    {
        plane->surface().setOpacity(opacity);
    }

    return plane->indexInSector();
}

#undef MPE_SectorCreate
int MPE_SectorCreate(float lightlevel, float red, float green, float blue,
                     const struct de_api_sector_hacks_s *hacks,
                     int archiveIndex)
{
    return editMap
        ->createSector(lightlevel,
                       {red, green, blue},
                       archiveIndex,
                       hacks)
        ->indexInMap();
}

#undef MPE_PolyobjCreate
int MPE_PolyobjCreate(const int *lines, int lineCount, int tag, int sequenceType,
    coord_t originX, coord_t originY, int archiveIndex)
{
    DE_UNUSED(archiveIndex); /// @todo Use this!

    if(lineCount <= 0 || !lines) return -1;

    // First check that all the line indices are valid and that they arn't
    // already part of another polyobj.
    for(int i = 0; i < lineCount; ++i)
    {
        if(lines[i] < 0 || lines[i] >= editMap->editableLineCount()) return -1;

        auto *line = editMap->editableLines().at(lines[i]);
        if(line->definesPolyobj()) return -1;
    }

    Polyobj *po = editMap->createPolyobj(Vec2d(originX, originY));
    po->setSequenceType(sequenceType);
    po->setTag(tag);

    for(int i = 0; i < lineCount; ++i)
    {
        auto *line = editMap->editableLines().at(lines[i]);

        // This line belongs to a polyobj.
        line->setPolyobj(po);
        po->data().lines.append(line);
    }

    return po->indexInMap();
}

#undef MPE_GameObjProperty
dd_bool MPE_GameObjProperty(const char *entityName, int elementIndex,
    const char *propertyName, valuetype_t valueType, void *valueAdr)
{
    LOG_AS("MPE_GameObjProperty");

    if(!entityName || !propertyName || !valueAdr)
        return false; // Hmm...

    // Is this a known entity?
    MapEntityDef *entityDef = P_MapEntityDefByName(entityName);
    if(!entityDef)
    {
        LOG_WARNING("Unknown entity name:\"%s\", ignoring.") << entityName;
        return false;
    }

    // Is this a known property?
    MapEntityPropertyDef *propertyDef;
    if(MapEntityDef_PropertyByName(entityDef, propertyName, &propertyDef) < 0)
    {
        LOG_WARNING("Entity \"%s\" has no \"%s\" property, ignoring.")
                << entityName << propertyName;
        return false;
    }

    try
    {
        EntityDatabase &entities = editMap->entityDatabase();
        entities.setProperty(propertyDef, elementIndex, valueType, valueAdr);
        return true;
    }
    catch(const Error &er)
    {
        LOG_WARNING("%s. Ignoring.") << er.asText();
    }
    return false;
}

DE_DECLARE_API(MPE) =
{
    { DE_API_MAP_EDIT },

    MPE_Begin,
    MPE_End,
    MPE_VertexCreate,
    MPE_VertexCreatev,
    MPE_LineCreate,
    MPE_LineAddSide,
    MPE_SectorCreate,
    MPE_PlaneCreate,
    MPE_PolyobjCreate,
    MPE_GameObjProperty
};
