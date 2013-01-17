/** @file plane.h Map Plane.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"

using namespace de;

Plane::Plane(Sector &_sector, Vector3f const &normal, coord_t _height)
    : de::MapElement(DMU_PLANE),
    sector(&_sector), height(_height)
{
    surface.owner = this;
    memset(oldHeight, 0, sizeof(oldHeight));
    target = 0;
    speed = 0;
    visHeight = 0;
    visHeightDelta = 0;
    planeID = 0;

    setNormal(normal);
}

Plane::~Plane()
{
}

void Plane::setNormal(Vector3f const &newNormal)
{
    V3f_Set(surface.normal, newNormal.x, newNormal.y, newNormal.z);
    V3f_Normalize(surface.normal);
    V3f_BuildTangents(surface.tangent, surface.bitangent, surface.normal);

    type = (surface.normal[VZ] < 0? PLN_CEILING : PLN_FLOOR);
}

int Plane_SetProperty(Plane* pln, const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_HEIGHT:
        DMU_SetValue(DMT_PLANE_HEIGHT, &pln->height, args, 0);
        if(!ddMapSetup)
        {
            R_AddTrackedPlane(GameMap_TrackedPlanes(theMap), pln);
            R_MarkDependantSurfacesForDecorationUpdate(pln);
        }
        break;
    case DMU_TARGET_HEIGHT:
        DMU_SetValue(DMT_PLANE_TARGET, &pln->target, args, 0);
        break;
    case DMU_SPEED:
        DMU_SetValue(DMT_PLANE_SPEED, &pln->speed, args, 0);
        break;
    default:
        Con_Error("Plane_SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}

int Plane_GetProperty(const Plane* pln, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_SECTOR:
        DMU_GetValue(DMT_PLANE_SECTOR, &pln->sector, args, 0);
        break;
    case DMU_HEIGHT:
        DMU_GetValue(DMT_PLANE_HEIGHT, &pln->height, args, 0);
        break;
    case DMU_TARGET_HEIGHT:
        DMU_GetValue(DMT_PLANE_TARGET, &pln->target, args, 0);
        break;
    case DMU_SPEED:
        DMU_GetValue(DMT_PLANE_SPEED, &pln->speed, args, 0);
        break;
    default:
        Con_Error("Plane_GetProperty: No property %s.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}
