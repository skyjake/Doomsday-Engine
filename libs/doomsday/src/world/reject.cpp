/** @file reject.h World map sector LOS reject LUT building.
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3).
 * @see http://sourceforge.net/projects/glbsp/
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

#if 0 // Needs updating.
#include <cmath>

#include "de_base.h"
#include "world/map.h"

using namespace de;

void BuildRejectForMap(const Map &map)
{
    int *secGroups = M_Malloc(sizeof(int) * map.sectorCount());
    int group = 0;

    for(uint i = 0; i < map.sectorCount(); ++i)
    {
        Sector &sector = map.sectors[i];
        secGroups[i] = group++;
        sector.rejNext = sector.rejPrev = &sector;
    }

    for(uint i = 0; i < map.lineCount(); ++i)
    {
        Line *line = &map.lines[i];

        if(!line->front().hasSector() || !line->back().hasSector())
            continue;

        Sector *sec1 = line->front().sectorPtr();
        Sector *sec2 = line->back().sectorPtr();

        if(!sec1 || !sec2 || sec1 == sec2)
            continue;

        // Already in the same group?
        if(secGroups[sec1->index] == secGroups[sec2->index])
            continue;

        // Swap sectors so that the smallest group is added to the biggest
        // group. This is based on the assumption that sector numbers in
        // wads will generally increase over the set of lines, and so
        // (by swapping) we'll tend to add small groups into larger
        // groups, thereby minimising the updates to 'rej_group' fields
        // that is required when merging.
        Sector *p = 0;
        if(secGroups[sec1->index] > secGroups[sec2->index])
        {
            p = sec1;
            sec1 = sec2;
            sec2 = p;
        }

        // Update the group numbers in the second group
        secGroups[sec2->index] = secGroups[sec1->index];
        for(p = sec2->rejNext; p != sec2; p = p->rejNext)
        {
            secGroups[p->index] = secGroups[sec1->index];
        }

        // Merge 'em baby...
        sec1->rejNext->rejPrev = sec2;
        sec2->rejNext->rejPrev = sec1;

        p = sec1->rejNext;
        sec1->rejNext = sec2->rejNext;
        sec2->rejNext = p;
    }

    size_t rejectSize = (map.sectorCount() * map.sectorCount() + 7) / 8;
    byte *matrix = Z_Calloc(rejectSize, PU_MAPSTATIC, 0);

    for(int view = 0; view < map.sectorCount(); ++view)
    for(int target = 0; target < view; ++target)
    {
        if(secGroups[view] == secGroups[target])
            continue;

        // For symmetry, do two bits at a time.
        int p1 = view * map.sectorCount() + target;
        int p2 = target * map.sectorCount() + view;

        matrix[p1 >> 3] |= (1 << (p1 & 7));
        matrix[p2 >> 3] |= (1 << (p2 & 7));
    }

    M_Free(secGroups);

    return matrix;
}
#endif
