/**
 * @file vertex.h
 * Logical map vertex. @ingroup map
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

#ifndef LIBDENG_MAP_VERTEX
#define LIBDENG_MAP_VERTEX

#ifndef __cplusplus
#  error "map/vertex.h requires C++"
#endif

#include <de/binangle.h>
#include "resource/r_data.h"
#include "map/p_dmu.h"
#include "MapObject"

#define LO_prev     link[0]
#define LO_next     link[1]

class Vertex;

typedef struct lineowner_shadowvert_s {
    coord_t inner[2];
    coord_t extended[2];
} lineowner_shadowvert_t;

typedef struct lineowner_s {
    struct linedef_s *lineDef;
    struct lineowner_s *link[2];    ///< {prev, next} (i.e. {anticlk, clk}).
    binangle_t angle;               ///< between this and next clockwise.
    lineowner_shadowvert_t shadowOffsets;
} lineowner_t;

typedef struct mvertex_s {
    /// Vertex index. Always valid after loading and pruning of unused
    /// vertices has occurred.
    int index;

    /// Reference count. When building normal node info, unused vertices
    /// will be pruned.
    int refCount;

    /// Usually NULL, unless this vertex occupies the same location as a
    /// previous vertex. Only used during the pruning phase.
    Vertex *equiv;
} mvertex_t;

struct vertex_s {
    coord_t origin[2];
    unsigned int numLineOwners; ///< Number of line owners.
    lineowner_t *lineOwners;    ///< Lineowner base ptr [numlineowners] size. A doubly, circularly linked list. The base is the line with the lowest angle and the next-most with the largest angle.
    mvertex_t buildData;
};

class Vertex : public de::MapObject, public vertex_s
{
public:
    Vertex() : de::MapObject(DMU_VERTEX)
    {
        memset(static_cast<vertex_s *>(this), 0, sizeof(vertex_s));
    }
};

/**
 * Count the total number of linedef "owners" of this vertex. An owner in
 * this context is any linedef whose start or end vertex is this.
 *
 * @pre Vertex linedef owner rings must have already been calculated.
 * @pre @a oneSided and/or @a twoSided must have already been initialized.
 *
 * @param vtx       Vertex instance.
 * @param oneSided  Total number of one-sided lines is written here. Can be @a NULL.
 * @param twoSided  Total number of two-sided lines is written here. Can be @a NULL.
 */
void Vertex_CountLineOwners(Vertex const *vtx, uint* oneSided, uint* twoSided);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param vertex    Vertex instance.
 * @param args      Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Vertex_GetProperty(const Vertex* vertex, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param vertex    Vertex instance.
 * @param args      Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Vertex_SetProperty(Vertex* vertex, const setargs_t* args);

#endif /// LIBDENG_MAP_VERTEX
