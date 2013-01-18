/** @file sector.h Sector implementation. 
 * @ingroup map
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

Sector::Sector() : de::MapElement(DMU_SECTOR)
{
    frameFlags = 0;
    validCount = 0;
    memset(&aaBox, 0, sizeof(aaBox));
    roughArea = 0;
    lightLevel = 0;
    oldLightLevel = 0;
    memset(rgb, 0, sizeof(rgb));
    memset(oldRGB, 0, sizeof(oldRGB));
    mobjList = 0;
    lineDefCount = 0;
    lineDefs = 0;
    bspLeafCount = 0;
    bspLeafs = 0;
    numReverbBspLeafAttributors = 0;
    reverbBspLeafs = 0;
    memset(&base, 0, sizeof(base));
    planeCount = 0;
    planes = 0;
    blockCount = 0;
    changedBlockCount = 0;
    blocks = 0;
    memset(reverb, 0, sizeof(reverb));
    memset(&buildData, 0, sizeof(buildData));
}

Sector::~Sector()
{
    if(planes)
    {
        for(uint i = 0; i < planeCount; ++i)
        {
            delete planes[i];
        }
        M_Free(planes);
    }

}

void Sector_UpdateAABox(Sector* sec)
{
    LineDef** lineIter;
    LineDef* line;
    assert(sec);

    V2d_Set(sec->aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2d_Set(sec->aaBox.max, DDMINFLOAT, DDMINFLOAT);

    lineIter = sec->lineDefs;
    if(!lineIter) return;

    line = *lineIter;
    V2d_InitBox(sec->aaBox.arvec2, line->aaBox.min);
    V2d_AddToBox(sec->aaBox.arvec2, line->aaBox.max);
    lineIter++;

    for(; *lineIter; lineIter++)
    {
        line = *lineIter;
        V2d_AddToBox(sec->aaBox.arvec2, line->aaBox.min);
        V2d_AddToBox(sec->aaBox.arvec2, line->aaBox.max);
    }
}

void Sector_UpdateArea(Sector* sec)
{
    assert(sec);
    // Only a very rough estimate is required.
    sec->roughArea = ((sec->aaBox.maxX - sec->aaBox.minX) / 128) *
                     ((sec->aaBox.maxY - sec->aaBox.minY) / 128);
}

void Sector_UpdateBaseOrigin(Sector* sec)
{
    assert(sec);
    sec->base.origin[VX] = (sec->aaBox.minX + sec->aaBox.maxX) / 2;
    sec->base.origin[VY] = (sec->aaBox.minY + sec->aaBox.maxY) / 2;
    sec->base.origin[VZ] = (sec->SP_floorheight + sec->SP_ceilheight) / 2;
}

int Sector_SetProperty(Sector* sec, const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_COLOR:
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[0], args, 0);
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[1], args, 1);
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[2], args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[0], args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[1], args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[2], args, 0);
        break;
    case DMU_LIGHT_LEVEL:
        DMU_SetValue(DMT_SECTOR_LIGHTLEVEL, &sec->lightLevel, args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_SECTOR_VALIDCOUNT, &sec->validCount, args, 0);
        break;
    default:
        Con_Error("Sector_SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}

int Sector_GetProperty(const Sector* sec, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_LIGHT_LEVEL:
        DMU_GetValue(DMT_SECTOR_LIGHTLEVEL, &sec->lightLevel, args, 0);
        break;
    case DMU_COLOR:
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[0], args, 0);
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[1], args, 1);
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[2], args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[0], args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[1], args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[2], args, 0);
        break;
    case DMU_BASE: {
        const ddmobj_base_t* base = &sec->base;
        DMU_GetValue(DMT_SECTOR_BASE, &base, args, 0);
        break; }
    case DMU_LINEDEF_COUNT: {
        int val = (int) sec->lineDefCount;
        DMU_GetValue(DDVT_INT, &val, args, 0);
        break; }
    case DMT_MOBJS:
        DMU_GetValue(DMT_SECTOR_MOBJLIST, &sec->mobjList, args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_SECTOR_VALIDCOUNT, &sec->validCount, args, 0);
        break;
    case DMU_FLOOR_PLANE: {
        Plane* pln = sec->planes[PLN_FLOOR];
        DMU_GetValue(DMT_SECTOR_FLOORPLANE, &pln, args, 0);
        break; }
    case DMU_CEILING_PLANE: {
        Plane* pln = sec->planes[PLN_CEILING];
        DMU_GetValue(DMT_SECTOR_CEILINGPLANE, &pln, args, 0);
        break; }
    default:
        Con_Error("Sector_GetProperty: No property %s.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}
