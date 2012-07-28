/**
 * @file id1map_load.h @ingroup wadmapconverter
 *
 * @authors Copyright &copy; 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef __WADMAPCONVERTER_ID1MAP_H__
#define __WADMAPCONVERTER_ID1MAP_H__

#include "doomsday.h"
#include "dd_types.h"
#include "maplumpinfo.h"
#include "id1map_datatypes.h"
#include "id1map_load.h"
#include "id1map_util.h"
#include <vector>
#include <list>

typedef std::vector<mline_t> Lines;
typedef std::vector<mside_t> Sides;
typedef std::vector<msector_t> Sectors;
typedef std::vector<mthing_t> Things;
typedef std::vector<surfacetint_t> SurfaceTints;
typedef std::list<mpolyobj_t> Polyobjs;

class Id1Map
{
public:
    uint            numVertexes;
    coord_t*        vertexes; ///< Array of vertex coords [v0:X, vo:Y, v1:X, v1:Y, ..]

    Lines           lines;
    Sides           sides;
    Sectors         sectors;
    Things          things;
    SurfaceTints    surfaceTints;
    Polyobjs        polyobjs;

    StringPool*     materials; ///< Material dictionary.

    Id1Map();
    ~Id1Map();

    int load(MapLumpInfo* lumpInfos[NUM_MAPLUMP_TYPES]);

    void analyze(void);

    int transfer(void);

    MaterialDictId addMaterialToDictionary(const char* name, MaterialDictGroup group);

private:
    inline const Str* findMaterialInDictionary(MaterialDictId id)
    {
        return StringPool_String(materials, id);
    }

    bool loadVertexes(Reader* reader, uint numElements);
    bool loadLineDefs(Reader* reader, uint numElements);
    bool loadSideDefs(Reader* reader, uint numElements);
    bool loadSectors(Reader* reader, uint numElements);
    bool loadThings(Reader* reader, uint numElements);
    bool loadSurfaceTints(Reader* reader, uint numElements);

    void transferVertexes(void);
    void transferSectors(void);
    void transferLinesAndSides(void);
    void transferSurfaceTints(void);
    void transferPolyobjs(void);
    void transferThings(void);
};

#endif /* __WADMAPCONVERTER_ID1MAP_H__ */
