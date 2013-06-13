/** @file world/api_mapedit.cpp Internal runtime map editing interface.
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#define DENG_NO_API_MACROS_MAP_EDIT

#include <de/StringPool>

#include "de_platform.h"
#include "de_console.h"
#include "dd_main.h"

#include "Materials"

#include "world/entitydatabase.h"
#include "world/p_mapdata.h"
#include "world/map.h"

#include "edit_map.h"
#include "api_mapedit.h"

using namespace de;

static Map *editMap;

static bool editMapInited;
static bool lastBuiltMapResult;

/**
 * Material name references specified during map conversion are recorded in
 * this dictionary. A dictionary is used to avoid repeatedly resolving the same
 * URIs and to facilitate a log of missing materials encountered during the
 * process.
 *
 * The pointer user value holds a pointer to the resolved Material (if found).
 * The integer user value tracks the number of times a reference occurs.
 */
static StringPool *materialDict;

/**
 * Either print or count-the-number-of unresolved references in the
 * material dictionary.
 *
 * @param internId    Unique id associated with the reference.
 * @param parameters  If a uint pointer operate in "count" mode (total written
 *                    here). Else operate in "print" mode.
 * @return Always @c 0 (for use as an iterator).
 */
static int printMissingMaterialWorker(StringPool::Id internId, void *parameters)
{
    uint *count = (uint *)parameters;

    // A valid id?
    if(materialDict->string(internId))
    {
        // Have we resolved this reference yet?
        if(!materialDict->userPointer(internId))
        {
            // An unresolved reference.
            if(count)
            {
                // Count mode.
                *count += 1;
            }
            else
            {
                // Print mode.
                int const refCount = materialDict->userValue(internId);
                String const &materialUri = materialDict->string(internId);
                QByteArray materialUriUtf8 = materialUri.toUtf8();
                Con_Message(" %4u x \"%s\"", refCount, materialUriUtf8.constData());
            }
        }
    }
    return 0; // Continue iteration.
}

/**
 * Destroy the missing material dictionary.
 */
static void clearMaterialDict()
{
    // Initialized?
    if(!materialDict) return;

    materialDict->clear();
    delete materialDict; materialDict = 0;
}

/**
 * Print any "missing" materials in the dictionary to the log.
 */
static void printMissingMaterialsInDict()
{
    // Initialized?
    if(!materialDict) return;

    // Count missing materials.
    uint numMissing = 0;
    materialDict->iterate(printMissingMaterialWorker, &numMissing);
    if(!numMissing) return;

    Con_Message("  [110] Warning: Found %u unknown %s:", numMissing, numMissing == 1? "material":"materials");
    // List the missing materials.
    materialDict->iterate(printMissingMaterialWorker, 0);
}

/**
 * Attempt to locate a material by its URI. A dictionary of previously
 * searched-for URIs is maintained to avoid repeated searching and to record
 * "missing" materials.
 *
 * @param materialUriStr  URI of the material to search for.
 *
 * @return  Pointer to the found material; otherwise @c 0.
 */
static Material *findMaterialInDict(ddstring_t const *materialUriStr)
{
    if(!materialUriStr || Str_IsEmpty(materialUriStr)) return 0;

    // Are we yet to instantiate the dictionary?
    if(!materialDict)
    {
        materialDict = new StringPool;
    }

    de::Uri materialUri(Str_Text(materialUriStr), RC_NULL);

    // Intern this reference.
    StringPool::Id internId = materialDict->intern(materialUri.compose());
    Material *material = 0;

    // Have we previously encountered this?.
    uint refCount = materialDict->userValue(internId);
    if(refCount)
    {
        // Yes, if resolved the user pointer holds the found material.
        material = (Material *) materialDict->userPointer(internId);
    }
    else
    {
        // No, attempt to resolve this URI and update the dictionary.
        // First try the preferred scheme, then any.
        try
        {
            material = &App_Materials().find(materialUri).material();
        }
        catch(Materials::NotFoundError const &)
        {
            // Try any scheme.
            try
            {
                materialUri.setScheme("");
                material = &App_Materials().find(materialUri).material();
            }
            catch(Materials::NotFoundError const &)
            {}
        }

        // Insert the possibly resolved material into the dictionary.
        materialDict->setUserPointer(internId, material);
    }

    // There is now one more reference.
    refCount++;
    materialDict->setUserValue(internId, refCount);

    return material;
}

Map *MPE_TakeMap()
{
    Map *retMap = editMap;
    editMap = 0;
    return retMap;
}

bool MPE_GetLastBuiltMapResult()
{
    return lastBuiltMapResult;
}

#undef MPE_Begin
boolean MPE_Begin(uri_s const *mapUri)
{
    // Already been here?
    if(editMapInited) return true;

    if(editMap)
        delete editMap;

    editMap = new Map(*reinterpret_cast<de::Uri const *>(mapUri));
    lastBuiltMapResult = false; // Failed (default).

    editMapInited = true;
    return true;
}

#undef MPE_End
boolean MPE_End()
{
    if(!editMapInited)
        return false;

    /*
     * Log warnings about any issues we encountered during conversion of
     * the basic map data elements.
     */
    printMissingMaterialsInDict();
    clearMaterialDict();

    if(editMap->endEditing())
    {
        editMapInited = false;

        lastBuiltMapResult = true; // Success.
    }
    else
    {
        // Darn, clean up...
        delete editMap; editMap = 0;

        lastBuiltMapResult = false; // Failed :$
    }

    return lastBuiltMapResult;
}

#undef MPE_VertexCreate
int MPE_VertexCreate(coord_t x, coord_t y, int archiveIndex)
{
    if(!editMapInited) return -1;
    return editMap->createVertex(Vector2d(x, y), archiveIndex)->indexInMap();
}

#undef MPE_VertexCreatev
boolean MPE_VertexCreatev(size_t num, coord_t *values, int *archiveIndices, int *retIndices)
{
    if(!editMapInited || !num || !values)
        return false;

    // Create many vertexes.
    for(size_t n = 0; n < num; ++n)
    {
        Vertex *vertex = editMap->createVertex(Vector2d(values[n * 2], values[n * 2 + 1]),
                                              archiveIndices[n]);
        if(retIndices)
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
    if(!editMapInited) return -1;

    if(frontSectorIdx >= editMap->editableSectorCount()) return -1;
    if(backSectorIdx  >= editMap->editableSectorCount()) return -1;
    if(v1 < 0 || v1 >= editMap->editableVertexCount()) return -1;
    if(v2 < 0 || v2 >= editMap->editableVertexCount()) return -1;
    if(v1 == v2) return -1;

    // Next, check the length is not zero.
    /// @todo fixme: We need to allow these... -ds
    Vertex *vtx1 = editMap->editableVertexes().at(v1);
    Vertex *vtx2 = editMap->editableVertexes().at(v2);
    if(de::abs(Vector2d(vtx1->origin() - vtx2->origin()).length()) <= 0.0001) return -1;

    Sector *frontSector = (frontSectorIdx >= 0? editMap->editableSectors().at(frontSectorIdx) : 0);
    Sector *backSector  = (backSectorIdx  >= 0? editMap->editableSectors().at(backSectorIdx) : 0);

    return editMap->createLine(*vtx1, *vtx2, flags, frontSector, backSector,
                              archiveIndex)->indexInMap();
}

#undef MPE_LineAddSide
void MPE_LineAddSide(int lineIdx, int sideId, short flags, ddstring_t const *topMaterialUri,
    float topOffsetX, float topOffsetY, float topRed, float topGreen, float topBlue,
    ddstring_t const *middleMaterialUri, float middleOffsetX, float middleOffsetY, float middleRed,
    float middleGreen, float middleBlue, float middleAlpha, ddstring_t const *bottomMaterialUri,
    float bottomOffsetX, float bottomOffsetY, float bottomRed, float bottomGreen,
    float bottomBlue, int archiveIndex)
{
    if(!editMapInited) return;

    if(lineIdx < 0 || lineIdx >= editMap->editableLineCount()) return;

    Line *line = editMap->editableLines().at(lineIdx);
    Line::Side &side = line->side(sideId);

    side.setFlags(flags);
    side.setIndexInArchive(archiveIndex);

    // Ensure sections are defined if they aren't already.
    side.addSections();

    // Assign the resolved material if found.
    side.top().setMaterial(findMaterialInDict(topMaterialUri));
    side.top().setMaterialOrigin(topOffsetX, topOffsetY);
    side.top().setTintColor(topRed, topGreen, topBlue);

    side.middle().setMaterial(findMaterialInDict(middleMaterialUri));
    side.middle().setMaterialOrigin(middleOffsetX, middleOffsetY);
    side.middle().setTintColor(middleRed, middleGreen, middleBlue);
    side.middle().setOpacity(middleAlpha);

    side.bottom().setMaterial(findMaterialInDict(bottomMaterialUri));
    side.bottom().setMaterialOrigin(bottomOffsetX, bottomOffsetY);
    side.bottom().setTintColor(bottomRed, bottomGreen, bottomBlue);
}

#undef MPE_PlaneCreate
int MPE_PlaneCreate(int sectorIdx, coord_t height, ddstring_t const *materialUri,
    float matOffsetX, float matOffsetY, float tintRed, float tintGreen, float tintBlue, float opacity,
    float normalX, float normalY, float normalZ, int archiveIndex)
{
    if(!editMapInited) return -1;

    if(sectorIdx < 0 || sectorIdx >= editMap->editableSectorCount()) return -1;

    Sector *sector = editMap->editableSectors().at(sectorIdx);
    Plane *plane = sector->addPlane(Vector3f(normalX, normalY, normalZ), height);

    plane->setIndexInArchive(archiveIndex);

    plane->surface().setMaterial(findMaterialInDict(materialUri));
    plane->surface().setTintColor(tintRed, tintGreen, tintBlue);
    plane->surface().setMaterialOrigin(matOffsetX, matOffsetY);

    if(!plane->isSectorFloor() && !plane->isSectorCeiling())
    {
        plane->surface().setOpacity(opacity);
    }

    return plane->inSectorIndex();
}

#undef MPE_SectorCreate
int MPE_SectorCreate(float lightlevel, float red, float green, float blue,
                     int archiveIndex)
{
    if(!editMapInited) return -1;
    return editMap->createSector(lightlevel, Vector3f(red, green, blue),
                                archiveIndex)->indexInMap();
}

#undef MPE_PolyobjCreate
int MPE_PolyobjCreate(int *lines, int lineCount, int tag, int sequenceType,
    coord_t originX, coord_t originY, int archiveIndex)
{
    DENG_UNUSED(archiveIndex); /// @todo Use this!

    if(!editMapInited) return -1;
    if(lineCount <= 0 || !lines) return -1;

    // First check that all the line indices are valid and that they arn't
    // already part of another polyobj.
    for(int i = 0; i < lineCount; ++i)
    {
        if(lines[i] < 0 || lines[i] >= editMap->editableLineCount()) return -1;

        Line *line = editMap->editableLines().at(lines[i]);
        if(line->definesPolyobj()) return -1;
    }

    Polyobj *po = editMap->createPolyobj(Vector2d(originX, originY));
    po->setSequenceType(sequenceType);
    po->setTag(tag);

    for(int i = 0; i < lineCount; ++i)
    {
        Line *line = editMap->editableLines().at(lines[i]);

        // This line belongs to a polyobj.
        line->setPolyobj(po);
        static_cast<Polyobj::Lines *>(po->_lines)->append(line);
    }

    return po->indexInMap();
}

#undef MPE_GameObjProperty
boolean MPE_GameObjProperty(char const *entityName, int elementIndex,
    char const *propertyName, valuetype_t valueType, void *valueAdr)
{
    LOG_AS("MPE_GameObjProperty");

    if(!editMapInited) return false;

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
    catch(Error const &er)
    {
        LOG_WARNING("%s. Ignoring.") << er.asText();
    }
    return false;
}

// p_data.cpp
#undef P_RegisterMapObj
DENG_EXTERN_C boolean P_RegisterMapObj(int identifier, char const *name);

#undef P_RegisterMapObjProperty
DENG_EXTERN_C boolean P_RegisterMapObjProperty(int entityId, int propertyId, char const *propertyName, valuetype_t type);

DENG_DECLARE_API(MPE) =
{
    { DE_API_MAP_EDIT },

    P_RegisterMapObj,
    P_RegisterMapObjProperty,
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
