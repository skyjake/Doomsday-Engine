/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_terraintype.c:
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "dmu_lib.h"
#include "p_terraintype.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    material_t*     material;
    uint            terrainNum;
} materialterraintype_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static terraintype_t terrainTypes[] =
{
    {"Default", 0}, // Default type (no special attributes).
#if __JDOOM__ || __JDOOM64__
    {"Water",   TTF_NONSOLID|TTF_FLOORCLIP},
    {"Lava",    TTF_NONSOLID|TTF_FLOORCLIP},
    {"Blood",   TTF_NONSOLID|TTF_FLOORCLIP},
    {"Nukage",  TTF_NONSOLID|TTF_FLOORCLIP},
    {"Slime",   TTF_NONSOLID|TTF_FLOORCLIP},
#endif
#if __JHERETIC__
    {"Water",   TTF_NONSOLID|TTF_FLOORCLIP|TTF_SPAWN_SPLASHES},
    {"Lava",    TTF_NONSOLID|TTF_FLOORCLIP|TTF_SPAWN_SMOKE},
    {"Sludge",  TTF_NONSOLID|TTF_FLOORCLIP|TTF_SPAWN_SLUDGE},
#endif
#if __JHEXEN__
    {"Water",   TTF_NONSOLID|TTF_FLOORCLIP|TTF_SPAWN_SPLASHES},
    {"Lava",    TTF_NONSOLID|TTF_FLOORCLIP|TTF_FRICTION_HIGH|TTF_DAMAGING|TTF_SPAWN_SMOKE},
    {"Sludge",  TTF_NONSOLID|TTF_FLOORCLIP|TTF_SPAWN_SLUDGE},
    {"Ice",     TTF_FRICTION_LOW},
#endif
    {NULL, 0}
};

static materialterraintype_t* materialTTypes = NULL;
static uint numMaterialTTypes = 0;

// CODE --------------------------------------------------------------------

static void createMaterialTerrainType(material_t* mat, uint idx)
{
    uint                i;
    materialterraintype_t* mtt;

    // If we've already assigned this material to a terrain type, override
    // the previous assignation.
    for(i = 0; i < numMaterialTTypes; ++i)
        if(materialTTypes[i].material == mat)
        {
            materialTTypes[i].terrainNum = idx;
            return;
        }

    // Its a new material.
    materialTTypes = (materialterraintype_t*)
        Z_Realloc(materialTTypes, sizeof(*materialTTypes) * ++numMaterialTTypes,
                  PU_STATIC);

    mtt = &materialTTypes[numMaterialTTypes-1];

    mtt->material = mat;
    mtt->terrainNum = idx - 1;
}

/**
 * @param name          The symbolic terrain type name.
 */
static uint getTerrainTypeNumForName(const char* name)
{
    uint                i;

    if(name && name[0])
    {
        for(i = 0; terrainTypes[i].name; ++i)
        {
            terraintype_t*      tt = &terrainTypes[i];

            if(!stricmp(tt->name, name))
                return i + 1; // 1-based index.
        }
    }

    return 0;
}

static terraintype_t* getTerrainTypeForMaterial(material_t* mat)
{
    uint                i;

    if(mat)
    {
        for(i = 0; i < numMaterialTTypes; ++i)
        {
            materialterraintype_t* mtt = &materialTTypes[i];

            if(mtt->material == mat)
                return &terrainTypes[mtt->terrainNum];
        }
    }

    return NULL;
}

/**
 * Called during (re)init.
 */
void P_InitTerrainTypes(void)
{
    struct matttypedef_s {
        const char*     matName;
        material_namespace_t matGroup;
        const char*     ttName;
    } matTTypeDefs[] =
    {
#if __JDOOM__ || __JDOOM64__
        {"FWATER1",  MN_FLATS, "Water"},
        {"LAVA1",    MN_FLATS, "Lava"},
        {"BLOOD1",   MN_FLATS, "Blood"},
        {"NUKAGE1",  MN_FLATS, "Nukage"},
        {"SLIME01",  MN_FLATS, "Slime"},
#endif
#if __JHERETIC__
        {"FLTWAWA1", MN_FLATS, "Water"},
        {"FLTFLWW1", MN_FLATS, "Water"},
        {"FLTLAVA1", MN_FLATS, "Lava"},
        {"FLATHUH1", MN_FLATS, "Lava"},
        {"FLTSLUD1", MN_FLATS, "Sludge"},
#endif
#if __JHEXEN__
        {"X_005",    MN_FLATS, "Water"},
        {"X_001",    MN_FLATS, "Lava"},
        {"X_009",    MN_FLATS, "Sludge"},
        {"F_033",    MN_FLATS, "Ice"},
#endif
        {NULL, material_namespace_t(0), NULL}
    };
    uint                i;

    if(materialTTypes)
        Z_Free(materialTTypes);
    materialTTypes = NULL;
    numMaterialTTypes = 0;

    for(i = 0; matTTypeDefs[i].matName; ++i)
    {
        uint                idx =
            getTerrainTypeNumForName(matTTypeDefs[i].ttName);

        if(idx)
        {
            material_t*         mat = (material_t*)
                P_ToPtr(DMU_MATERIAL,
                        P_MaterialCheckNumForName(matTTypeDefs[i].matName,
                                                  matTTypeDefs[i].matGroup));
            if(mat)
            {
                Con_Message("P_InitTerrainTypes: Material '%s' linked to terrain type '%s'.\n",
                            matTTypeDefs[i].matName, matTTypeDefs[i].ttName);
                createMaterialTerrainType(mat, idx);
            }
        }
    }
}

/**
 * Return the terrain type of the specified material.
 *
 * @param num           The material to check.
 */
const terraintype_t* P_TerrainTypeForMaterial(material_t* mat)
{
    const terraintype_t* tt = getTerrainTypeForMaterial(mat);

    if(tt)
        return tt; // Known, return it.

    // Return the default type.
    return &terrainTypes[0];
}
