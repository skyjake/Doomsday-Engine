/** @file sidedef.cpp Map SideDef.
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

#include <de/Log>
#include "de_base.h"
#include "map/linedef.h"

#include "map/sidedef.h"

using namespace de;

SideDef::SideDef() : de::MapElement(DMU_SIDEDEF)
{
    line = 0;
    flags = 0;
    std::memset(&buildData, 0, sizeof(buildData));
    fakeRadioUpdateCount = 0;
    std::memset(topCorners, 0, sizeof(topCorners));
    std::memset(bottomCorners, 0, sizeof(bottomCorners));
    std::memset(sideCorners, 0, sizeof(sideCorners));
    std::memset(spans, 0, sizeof(spans));
}

SideDef::~SideDef()
{}

void SideDef::updateBaseOrigins()
{
    if(!line) return;
    SW_middlesurface.updateBaseOrigin();
    SW_bottomsurface.updateBaseOrigin();
    SW_topsurface.updateBaseOrigin();
}

void SideDef::updateSurfaceTangents()
{
    if(!line) return;

    byte sid = line->frontSideDefPtr() == this? FRONT : BACK;
    Surface *surface = &SW_topsurface;
    surface->normal[VY] = (line->vertexOrigin(sid  )[VX] - line->vertexOrigin(sid^1)[VX]) / line->length;
    surface->normal[VX] = (line->vertexOrigin(sid^1)[VY] - line->vertexOrigin(sid  )[VY]) / line->length;
    surface->normal[VZ] = 0;
    V3f_BuildTangents(surface->tangent, surface->bitangent, surface->normal);

    // All surfaces of a sidedef have the same vectors.
    std::memcpy(SW_middletangent,   surface->tangent,   sizeof(surface->tangent));
    std::memcpy(SW_middlebitangent, surface->bitangent, sizeof(surface->bitangent));
    std::memcpy(SW_middlenormal,    surface->normal,    sizeof(surface->normal));

    std::memcpy(SW_bottomtangent,   surface->tangent,   sizeof(surface->tangent));
    std::memcpy(SW_bottombitangent, surface->bitangent, sizeof(surface->bitangent));
    std::memcpy(SW_bottomnormal,    surface->normal,    sizeof(surface->normal));
}

int SideDef::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_SECTOR: {
        Sector *sector = line->sectorPtr(this == line->frontSideDefPtr()? FRONT : BACK);
        DMU_GetValue(DMT_SIDEDEF_SECTOR, &sector, &args, 0);
        break; }
    case DMU_LINEDEF:
        DMU_GetValue(DMT_SIDEDEF_LINE, &line, &args, 0);
        break;
    case DMU_FLAGS:
        DMU_GetValue(DMT_SIDEDEF_FLAGS, &flags, &args, 0);
        break;
    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("SideDef::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
    }
    return false; // Continue iteration.
}

int SideDef::setProperty(setargs_t const &args)
{
    switch(args.prop)
    {
    case DMU_FLAGS:
        DMU_SetValue(DMT_SIDEDEF_FLAGS, &flags, &args, 0);
        break;

    case DMU_LINEDEF:
        DMU_SetValue(DMT_SIDEDEF_LINE, &line, &args, 0);
        break;

    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("SideDef::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }
    return false; // Continue iteration.
}
