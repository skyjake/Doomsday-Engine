/**
 * @file bspleaf.c
 * BspLeaf implementation. @ingroup map
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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
#include "de_refresh.h"
#include "de_play.h"

int BspLeaf_SetProperty(BspLeaf* leaf, const setargs_t* args)
{
    Con_Error("BspLeaf_SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    exit(1); // Unreachable.
}

int BspLeaf_GetProperty(const BspLeaf* leaf, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_SECTOR:
        DMU_GetValue(DMT_BSPLEAF_SECTOR, &leaf->sector, args, 0);
        break;
    case DMU_LIGHT_LEVEL:
        DMU_GetValue(DMT_SECTOR_LIGHTLEVEL, &leaf->sector->lightLevel, args, 0);
        break;
    case DMT_MOBJS:
        DMU_GetValue(DMT_SECTOR_MOBJLIST, &leaf->sector->mobjList, args, 0);
        break;
    case DMU_HEDGE_COUNT: {
        int val = (int) leaf->hedgeCount;
        DMU_GetValue(DDVT_INT, &val, args, 0);
        break; }
    default:
        Con_Error("BspLeaf_GetProperty: No property %s.\n", DMU_Str(args->prop));
        exit(1); // Unreachable.
    }
    return false; // Continue iteration.
}
