/** @file mapimport.cpp  Importer for id-formatted map data.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "gloom/world/mapimport.h"

#include <QDebug>

namespace gloom {

#if !defined (_MSC_VER)
#  define PACKED_STRUCT __attribute__((packed))
#else
#  define PACKED_STRUCT
#endif

DENG2_PIMPL_NOREF(MapImport)
{
    const res::LumpCatalog &lumps;
    Map map;

#if defined (_MSC_VER)
#  pragma pack(push, 1)
#endif

    struct DoomVertex {
        dint16 x;
        dint16 y;
    };

    struct DoomSidedef {
        dint16 xOffset;
        dint16 yOffset;
        char upperTexture[8];
        char lowerTexture[8];
        char middleTexture[8];
        dint16 sector;
    };

    struct DoomLinedef {
        dint16 startVertex;
        dint16 endVertex;
        dint16 flags;
        dint16 special;
        dint16 sector;
        dint16 frontSidedef;
        dint16 backSidedef;
    };

    struct HexenLinedef {
        dint16 startVertex;
        dint16 endVertex;
        dint16 flags;
        duint8 special;
        duint8 args[5];
        dint16 frontSidedef;
        dint16 backSidedef;
    } PACKED_STRUCT;

    struct DoomSector {
        dint16 floorHeight;
        dint16 ceilingHeight;
        char floorTexture[8];
        char ceilingTexture[8];
        duint16 lightLevel;
        duint16 type;
        duint16 tag;
    };

#if defined (_MSC_VER)
#  pragma pack(pop)
#endif

    template <typename T>
    struct DataArray
    {
        DataArray(const Block &data)
            : _data(data)
            , _entries(reinterpret_cast<const T *>(_data.constData()))
            , _size(int(_data.size() / sizeof(T)))
        {}
        int size() const
        {
            return _size;
        }
        const T &at(int pos) const
        {
            DENG2_ASSERT(pos >= 0);
            DENG2_ASSERT(pos < _size);
            return _entries[pos];
        }
        const T &operator[](int pos) const
        {
            return at(pos);
        }

    private:
        Block    _data;
        const T *_entries;
        int      _size;
    };

    Impl(const res::LumpCatalog &lc) : lumps(lc)
    {}

    bool import(const String &mapId)
    {
        map.clear();

        const auto headerPos = lumps.find(mapId);

        const DataArray<DoomVertex>  vertices(lumps.read(headerPos, 4));
        const DataArray<DoomLinedef> linedefs(lumps.read(headerPos, 2));
        const DataArray<DoomSidedef> sidedefs(lumps.read(headerPos, 3));
        const DataArray<DoomSector>  sectors (lumps.read(headerPos, 8));

        qDebug("%i vertices", vertices.size());
        for (int i = 0; i < vertices.size(); ++i)
        {
            qDebug("%i: x=%i, y=%i", i, vertices[i].x, vertices[i].y);
        }

        return true;
    }
};

MapImport::MapImport(const res::LumpCatalog &lumps)
    : d(new Impl(lumps))
{}

bool MapImport::importMap(const String &mapId)
{
    return d->import(mapId);
}

Map &MapImport::map()
{
    return d->map;
}

} // namespace gloom
