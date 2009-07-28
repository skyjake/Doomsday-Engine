/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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

#include <de/App>
#include <de/Zone>

using namespace de;

#include "dd_export.h"
BEGIN_EXTERN_C
#include "de_base.h"
#include "de_play.h"
#include "de_bsp.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_edit.h"
#include "de_dam.h"
END_EXTERN_C

typedef struct linelink_s {
    linedef_t      *line;
    struct linelink_s *next;
} linelink_t;

void MPE_BuildSectorLineLists(gamemap_t *map)
{
    uint                i, j;
    linedef_t          *li;
    sector_t           *sec;

    linelink_t        **sectorLineLinks;
    uint                totallinks;

    Con_Message(" Build line tables...\n");

    // build line tables for each sector.
    typedef Zone::Allocator<linelink_t> Allocator;
    Allocator* allocator = new Allocator(App::memory(), 512);
    sectorLineLinks = (linelink_t**) M_Calloc(sizeof(linelink_t*) * map->numSectors);
    totallinks = 0;
    for(i = 0, li = map->lineDefs; i < map->numLineDefs; ++i, li++)
    {
        uint        secIDX;
        linelink_t *link;

        if(li->L_frontside)
        {
            link = allocator->allocate();

            secIDX = li->L_frontsector - map->sectors;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            li->L_frontsector->lineDefCount++;
            totallinks++;
        }

        if(li->L_backside && li->L_backsector != li->L_frontsector)
        {
            link = allocator->allocate();

            secIDX = li->L_backsector - map->sectors;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            li->L_backsector->lineDefCount++;
            totallinks++;
        }
    }

    // Harden the sector line links into arrays.
    linedef_t    **linebuffer;
    linedef_t    **linebptr;

    linebuffer = (linedef_t**) Z_Malloc((totallinks + map->numSectors) * sizeof(linedef_t*), PU_MAPSTATIC, 0);
    linebptr = linebuffer;

    for(i = 0, sec = map->sectors; i < map->numSectors; ++i, sec++)
    {
        if(sectorLineLinks[i])
        {
            linelink_t *link = sectorLineLinks[i];
            sec->lineDefs = linebptr;
            j = 0;
            while(link)
            {
                sec->lineDefs[j++] = link->line;
                link = link->next;
            }
            sec->lineDefs[j] = NULL; // terminate.
            sec->lineDefCount = j;
            linebptr += j + 1;
        }
        else
        {
            sec->lineDefs = NULL;
            sec->lineDefCount = 0;
        }
    }

    // Free temporary storage.
    delete allocator;
    M_Free(sectorLineLinks);
}
