/** @file id1map.cpp  id Tech 1 map format reader.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "id1map.h"
#include <vector>
#include <list>
#include <QVector>
#include <de/libcore.h>
#include <de/Error>
#include <de/ByteRefArray>
#include <de/Log>
#include <de/Reader>
#include <de/Time>
#include <de/Vector>
#include "idtech1converter.h"

using namespace de;

namespace wadimp {
namespace internal {

/// @todo kludge - remove me.
class Id1MapElement
{
public:
    Id1MapElement(Id1Map &map) : _map(&map) {}
    Id1MapElement(Id1MapElement const &other) : _map(other._map) {}
    virtual ~Id1MapElement() {}

    Id1Map &map() const {
        DENG2_ASSERT(_map != 0);
        return *_map;
    }

    Id1Map *_map;
};

struct SideDef : public Id1MapElement
{
    dint index;
    dint16 offset[2];
    MaterialId topMaterial;
    MaterialId bottomMaterial;
    MaterialId middleMaterial;
    dint sector;

    SideDef(Id1Map &map) : Id1MapElement(map) {}

    void operator << (de::Reader &from)
    {
        Id1MapRecognizer::Format format = Id1MapRecognizer::Format(from.version());

        from >> offset[VX]
             >> offset[VY];

        dint idx;
        switch(format)
        {
        case Id1MapRecognizer::DoomFormat:
        case Id1MapRecognizer::HexenFormat: {
            Block name;
            from.readBytes(8, name);
            topMaterial    = map().toMaterialId(name.constData(), WallMaterials);

            from.readBytes(8, name);
            bottomMaterial = map().toMaterialId(name.constData(), WallMaterials);

            from.readBytes(8, name);
            middleMaterial = map().toMaterialId(name.constData(), WallMaterials);
            break; }

        case Id1MapRecognizer::Doom64Format:
            from.readAs<duint16>(idx);
            topMaterial    = map().toMaterialId(idx, WallMaterials);

            from.readAs<duint16>(idx);
            bottomMaterial = map().toMaterialId(idx, WallMaterials);

            from.readAs<duint16>(idx);
            middleMaterial = map().toMaterialId(idx, WallMaterials);
            break;

        default:
            DENG2_ASSERT(!"wadimp::SideDef::read: unknown map format!");
            break;
        };

        from.readAs<duint16>(idx);
        sector = (idx == 0xFFFF? -1 : idx);
    }
};

/**
 * @defgroup lineAnalysisFlags  Line Analysis flags
 */
///@{
#define LAF_POLYOBJ  0x1 ///< Line defines a polyobj segment.
///@}

#define PO_LINE_START     (1) ///< Polyobj line start special.
#define PO_LINE_EXPLICIT  (5)

#define SEQTYPE_NUMSEQ  (10)

struct LineDef : public Id1MapElement
{
    enum Side {
        Front,
        Back
    };

    dint index;
    dint v[2];
    dint sides[2];
    dint16 flags; ///< MF_* flags.

    // Analysis data:
    dint16 aFlags;

    // DOOM format members:
    dint16 dType;
    dint16 dTag;

    // Hexen format members:
    dint8 xType;
    dint8 xArgs[5];

    // DOOM64 format members:
    dint8 d64drawFlags;
    dint8 d64texFlags;
    dint8 d64type;
    dint8 d64useType;
    dint16 d64tag;

    dint ddFlags;
    duint validCount; ///< Used for polyobj line collection.

    LineDef(Id1Map &map) : Id1MapElement(map) {}

    int sideIndex(Side which) const
    {
        DENG2_ASSERT(which == Front || which == Back);
        return sides[which];
    }

    inline bool hasSide(Side which) const { return sideIndex(which) >= 0; }

    inline bool hasFront() const { return hasSide(Front); }
    inline bool hasBack()  const { return hasSide(Back);  }

    inline dint front()    const { return sideIndex(Front); }
    inline dint back()     const { return sideIndex(Back); }

    void operator << (de::Reader &from)
    {
        Id1MapRecognizer::Format format = Id1MapRecognizer::Format(from.version());

        dint idx;
        from.readAs<duint16>(idx);
        v[0] = (idx == 0xFFFF? -1 : idx);

        from.readAs<duint16>(idx);
        v[1] = (idx == 0xFFFF? -1 : idx);

        from >> flags;

        switch(format)
        {
        case Id1MapRecognizer::DoomFormat:
            from >> dType
                 >> dTag;
            break;

        case Id1MapRecognizer::Doom64Format:
            from >> d64drawFlags
                 >> d64texFlags
                 >> d64type
                 >> d64useType
                 >> d64tag;
            break;

        case Id1MapRecognizer::HexenFormat:
            from >> xType
                 >> xArgs[0]
                 >> xArgs[1]
                 >> xArgs[2]
                 >> xArgs[3]
                 >> xArgs[4];
            break;

        default:
            DENG2_ASSERT(!"wadimp::LineDef::read: unknown map format!");
            break;
        };

        from.readAs<duint16>(idx);
        sides[Front] = (idx == 0xFFFF? -1 : idx);

        from.readAs<duint16>(idx);
        sides[Back]  = (idx == 0xFFFF? -1 : idx);

        aFlags     = 0;
        validCount = 0;
        ddFlags    = 0;

        // Translate the line flags for Doomsday:
#define ML_BLOCKING              1 ///< Solid, is an obstacle.
#define ML_DONTPEGTOP            8 ///< Upper texture unpegged.
#define ML_DONTPEGBOTTOM        16 ///< Lower texture unpegged.

/// If set ALL flags NOT in DOOM v1.9 will be zeroed upon map load.
#define ML_INVALID              2048
#define DOOM_VALIDMASK          0x000001ff

        /**
         * Zero unused flags if ML_INVALID is set.
         *
         * @attention "This has been found to be necessary because of errors
         *  in Ultimate DOOM's E2M7, where around 1000 linedefs have
         *  the value 0xFE00 masked into the flags value.
         *  There could potentially be many more maps with this problem,
         *  as it is well-known that Hellmaker wads set all bits in
         *  mapthings that it does not understand."
         *  Thanks to Quasar for the heads up.
         *
         * Only valid for DOOM format maps.
         */
        if(format == Id1MapRecognizer::DoomFormat)
        {
            if(flags & ML_INVALID)
                flags &= DOOM_VALIDMASK;
        }

        if(flags & ML_BLOCKING)
        {
            ddFlags |= DDLF_BLOCKING;
            flags &= ~ML_BLOCKING;
        }

        if(flags & ML_DONTPEGTOP)
        {
            ddFlags |= DDLF_DONTPEGTOP;
            flags &= ~ML_DONTPEGTOP;
        }

        if(flags & ML_DONTPEGBOTTOM)
        {
            ddFlags |= DDLF_DONTPEGBOTTOM;
            flags &= ~ML_DONTPEGBOTTOM;
        }

#undef DOOM_VALIDMASK
#undef ML_INVALID
#undef ML_DONTPEGBOTTOM
#undef ML_DONTPEGTOP
#undef ML_BLOCKING
    }
};

struct SectorDef : public Id1MapElement
{
    dint index;
    dint16 floorHeight;
    dint16 ceilHeight;
    dint16 lightLevel;
    dint16 type;
    dint16 tag;
    MaterialId floorMaterial;
    MaterialId ceilMaterial;

    // DOOM64 format members:
    dint16 d64flags;
    duint16 d64floorColor;
    duint16 d64ceilingColor;
    duint16 d64unknownColor;
    duint16 d64wallTopColor;
    duint16 d64wallBottomColor;

    SectorDef(Id1Map &map) : Id1MapElement(map) {}

    void operator << (de::Reader &from)
    {
        Id1MapRecognizer::Format format = Id1MapRecognizer::Format(from.version());

        from >> floorHeight
             >> ceilHeight;

        switch(format)
        {
        case Id1MapRecognizer::DoomFormat:
        case Id1MapRecognizer::HexenFormat: {
            Block name;
            from.readBytes(8, name);
            floorMaterial= map().toMaterialId(name.constData(), PlaneMaterials);

            from.readBytes(8, name);
            ceilMaterial = map().toMaterialId(name.constData(), PlaneMaterials);

            from >> lightLevel;
            break; }

        case Id1MapRecognizer::Doom64Format: {
            duint16 idx;
            from >> idx;
            floorMaterial= map().toMaterialId(idx, PlaneMaterials);

            from >> idx;
            ceilMaterial = map().toMaterialId(idx, PlaneMaterials);

            from >> d64ceilingColor
                 >> d64floorColor
                 >> d64unknownColor
                 >> d64wallTopColor
                 >> d64wallBottomColor;

            lightLevel = 160; ///?
            break; }

        default:
            DENG2_ASSERT(!"wadimp::SectorDef::read: unknown map format!");
            break;
        };

        from >> type
             >> tag;

        if(format == Id1MapRecognizer::Doom64Format)
            from >> d64flags;
    }
};

// Thing DoomEdNums for polyobj anchors/spawn spots.
#define PO_ANCHOR_DOOMEDNUM     (3000)
#define PO_SPAWN_DOOMEDNUM      (3001)
#define PO_SPAWNCRUSH_DOOMEDNUM (3002)

/// @todo Get these from a game api header.
#define MTF_Z_FLOOR         0x20000000 ///< Spawn relative to floor height.
#define MTF_Z_CEIL          0x40000000 ///< Spawn relative to ceiling height (minus thing height).
#define MTF_Z_RANDOM        0x80000000 ///< Random point between floor and ceiling.

#define ANG45               0x20000000

struct Thing : public Id1MapElement
{
    dint index;
    dint16 origin[3];
    angle_t angle;
    dint16 doomEdNum;
    dint32 flags;
    dint32 skillModes;

    // Hexen format members:
    dint16 xTID;
    dint8 xSpecial;
    dint8 xArgs[5];

    // DOOM64 format members:
    dint16 d64TID;

    Thing(Id1Map &map) : Id1MapElement(map) {}

    void operator << (de::Reader &from)
    {
        Id1MapRecognizer::Format format = Id1MapRecognizer::Format(from.version());

        switch(format)
        {
        case Id1MapRecognizer::DoomFormat: {
#define MTF_EASY            0x00000001 ///< Can be spawned in Easy skill modes.
#define MTF_MEDIUM          0x00000002 ///< Can be spawned in Medium skill modes.
#define MTF_HARD            0x00000004 ///< Can be spawned in Hard skill modes.
#define MTF_DEAF            0x00000008 ///< Mobj will be deaf spawned deaf.
#define MTF_NOTSINGLE       0x00000010 ///< (BOOM) Can not be spawned in single player gamemodes.
#define MTF_NOTDM           0x00000020 ///< (BOOM) Can not be spawned in the Deathmatch gameMode.
#define MTF_NOTCOOP         0x00000040 ///< (BOOM) Can not be spawned in the Co-op gameMode.
#define MTF_FRIENDLY        0x00000080 ///< (BOOM) friendly monster.

#define MASK_UNKNOWN_THING_FLAGS (0xffffffff \
    ^ (MTF_EASY|MTF_MEDIUM|MTF_HARD|MTF_DEAF|MTF_NOTSINGLE|MTF_NOTDM|MTF_NOTCOOP|MTF_FRIENDLY))

            origin[VZ]   = 0;
            from >> origin[VX]
                 >> origin[VY];

            from.readAs<dint16>(angle);
            angle = (angle / 45) * ANG45;

            from >> doomEdNum;
            from.readAs<dint16>(flags);

            skillModes = 0;
            if(flags & MTF_EASY)   skillModes |= 0x00000001 | 0x00000002;
            if(flags & MTF_MEDIUM) skillModes |= 0x00000004;
            if(flags & MTF_HARD)   skillModes |= 0x00000008 | 0x00000010;

            flags &= ~MASK_UNKNOWN_THING_FLAGS;
            // DOOM format things spawn on the floor by default unless their
            // type-specific flags override.
            flags |= MTF_Z_FLOOR;

#undef MASK_UNKNOWN_THING_FLAGS
#undef MTF_FRIENDLY
#undef MTF_NOTCOOP
#undef MTF_NOTDM
#undef MTF_NOTSINGLE
#undef MTF_AMBUSH
#undef MTF_HARD
#undef MTF_MEDIUM
#undef MTF_EASY
            break; }

        case Id1MapRecognizer::Doom64Format: {
#define MTF_EASY              0x00000001 ///< Appears in easy skill modes.
#define MTF_MEDIUM            0x00000002 ///< Appears in medium skill modes.
#define MTF_HARD              0x00000004 ///< Appears in hard skill modes.
#define MTF_DEAF              0x00000008 ///< Thing is deaf.
#define MTF_NOTSINGLE         0x00000010 ///< Appears in multiplayer game modes only.
#define MTF_DONTSPAWNATSTART  0x00000020 ///< Do not spawn this thing at map start.
#define MTF_SCRIPT_TOUCH      0x00000040 ///< Mobjs spawned from this spot will envoke a script when touched.
#define MTF_SCRIPT_DEATH      0x00000080 ///< Mobjs spawned from this spot will envoke a script on death.
#define MTF_SECRET            0x00000100 ///< A secret (bonus) item.
#define MTF_NOTARGET          0x00000200 ///< Mobjs spawned from this spot will not target their attacker when hurt.
#define MTF_NOTDM             0x00000400 ///< Can not be spawned in the Deathmatch gameMode.
#define MTF_NOTCOOP           0x00000800 ///< Can not be spawned in the Co-op gameMode.

#define MASK_UNKNOWN_THING_FLAGS (0xffffffff \
    ^ (MTF_EASY|MTF_MEDIUM|MTF_HARD|MTF_DEAF|MTF_NOTSINGLE|MTF_DONTSPAWNATSTART|MTF_SCRIPT_TOUCH|MTF_SCRIPT_DEATH|MTF_SECRET|MTF_NOTARGET|MTF_NOTDM|MTF_NOTCOOP))

            from >> origin[VX]
                 >> origin[VY]
                 >> origin[VZ];

            from.readAs<dint16>(angle);
            angle = (angle / 45) * ANG45;

            from >> doomEdNum;
            from.readAs<dint32>(flags);

            skillModes = 0;
            if(flags & MTF_EASY)   skillModes |= 0x00000001;
            if(flags & MTF_MEDIUM) skillModes |= 0x00000002;
            if(flags & MTF_HARD)   skillModes |= 0x00000004 | 0x00000008;

            flags &= ~MASK_UNKNOWN_THING_FLAGS;
            // DOOM64 format things spawn relative to the floor by default
            // unless their type-specific flags override.
            flags |= MTF_Z_FLOOR;

            from >> d64TID;

#undef MASK_UNKNOWN_THING_FLAGS
#undef MTF_NOTCOOP
#undef MTF_NOTDM
#undef MTF_NOTARGET
#undef MTF_SECRET
#undef MTF_SCRIPT_DEATH
#undef MTF_SCRIPT_TOUCH
#undef MTF_DONTSPAWNATSTART
#undef MTF_NOTSINGLE
#undef MTF_DEAF
#undef MTF_HARD
#undef MTF_MEDIUM
#undef MTF_EASY
            break; }

        case Id1MapRecognizer::HexenFormat: {
#define MTF_EASY            0x00000001
#define MTF_MEDIUM          0x00000002
#define MTF_HARD            0x00000004
#define MTF_AMBUSH          0x00000008
#define MTF_DORMANT         0x00000010
#define MTF_FIGHTER         0x00000020
#define MTF_CLERIC          0x00000040
#define MTF_MAGE            0x00000080
#define MTF_GSINGLE         0x00000100
#define MTF_GCOOP           0x00000200
#define MTF_GDEATHMATCH     0x00000400
// The following are not currently used:
#define MTF_SHADOW          0x00000800 ///< (ZDOOM) Thing is 25% translucent.
#define MTF_INVISIBLE       0x00001000 ///< (ZDOOM) Makes the thing invisible.
#define MTF_FRIENDLY        0x00002000 ///< (ZDOOM) Friendly monster.
#define MTF_STILL           0x00004000 ///< (ZDOOM) Thing stands still (only useful for specific Strife monsters or friendlies).

#define MASK_UNKNOWN_THING_FLAGS (0xffffffff \
    ^ (MTF_EASY|MTF_MEDIUM|MTF_HARD|MTF_AMBUSH|MTF_DORMANT|MTF_FIGHTER|MTF_CLERIC|MTF_MAGE|MTF_GSINGLE|MTF_GCOOP|MTF_GDEATHMATCH|MTF_SHADOW|MTF_INVISIBLE|MTF_FRIENDLY|MTF_STILL))

            from >> xTID
                 >> origin[VX]
                 >> origin[VY]
                 >> origin[VZ];

            from.readAs<dint16>(angle);

            from >> doomEdNum;

            // For some reason, the Hexen format stores polyobject tags in the
            // angle field in THINGS. Thus, we cannot translate the angle until
            // we know whether it is a polyobject type or not.
            if(doomEdNum != PO_ANCHOR_DOOMEDNUM &&
               doomEdNum != PO_SPAWN_DOOMEDNUM &&
               doomEdNum != PO_SPAWNCRUSH_DOOMEDNUM)
            {
                angle = ANG45 * (angle / 45);
            }

            from.readAs<dint16>(flags);

            skillModes = 0;
            if(flags & MTF_EASY)   skillModes |= 0x00000001 | 0x00000002;
            if(flags & MTF_MEDIUM) skillModes |= 0x00000004;
            if(flags & MTF_HARD)   skillModes |= 0x00000008 | 0x00000010;

            flags &= ~MASK_UNKNOWN_THING_FLAGS;

            // Translate flags:
            // Game type logic is inverted.
            flags ^= (MTF_GSINGLE|MTF_GCOOP|MTF_GDEATHMATCH);

            // HEXEN format things spawn relative to the floor by default
            // unless their type-specific flags override.
            flags |= MTF_Z_FLOOR;

            from >> xSpecial
                 >> xArgs[0]
                 >> xArgs[1]
                 >> xArgs[2]
                 >> xArgs[3]
                 >> xArgs[4];

#undef MASK_UNKNOWN_THING_FLAGS
#undef MTF_STILL
#undef MTF_FRIENDLY
#undef MTF_INVISIBLE
#undef MTF_SHADOW
#undef MTF_GDEATHMATCH
#undef MTF_GCOOP
#undef MTF_GSINGLE
#undef MTF_MAGE
#undef MTF_CLERIC
#undef MTF_FIGHTER
#undef MTF_DORMANT
#undef MTF_AMBUSH
#undef MTF_HARD
#undef MTF_NORMAL
#undef MTF_EASY
            break; }

        default:
            DENG2_ASSERT(!"wadimp::Thing::read: unknown map format!");
            break;
        };
    }
};

struct TintColor : public Id1MapElement
{
    dint index;
    dfloat rgb[3];
    dint8 xx[3];

    TintColor(Id1Map &map) : Id1MapElement(map) {}

    void operator << (de::Reader &from)
    {
        //Id1Map::Format format = Id1Map::Format(from.version());

        from.readAs<dint8>(rgb[0]); rgb[0] /= 255;
        from.readAs<dint8>(rgb[1]); rgb[1] /= 255;
        from.readAs<dint8>(rgb[2]); rgb[2] /= 255;

        from >> xx[0]
             >> xx[1]
             >> xx[2];
    }
};

struct Polyobj
{
    typedef QVector<int> LineIndices;

    dint index;
    LineIndices lineIndices;
    dint tag;
    dint seqType;
    dint16 anchor[2];
};

} // namespace internal

using namespace internal;

static uint validCount = 0; ///< Used with Polyobj LineDef collection.

DENG2_PIMPL(Id1Map)
{
    Id1MapRecognizer::Format format;

    QVector<coord_t> vertCoords;  ///< Position coords [v0:X, vo:Y, v1:X, v1:Y, ...]

    typedef std::vector<LineDef> Lines;
    Lines lines;

    typedef std::vector<SideDef> Sides;
    Sides sides;

    typedef std::vector<SectorDef> Sectors;
    Sectors sectors;

    typedef std::vector<Thing> Things;
    Things things;

    typedef std::vector<TintColor> SurfaceTints;
    SurfaceTints surfaceTints;

    typedef std::list<Polyobj> Polyobjs;
    Polyobjs polyobjs;

    struct MaterialDict
    {
        StringPool dict;

        String const &find(MaterialId id) const
        {
            return dict.stringRef(id);
        }

        MaterialId toMaterialId(String name, MaterialGroup group)
        {
            // In original DOOM, texture name references beginning with the
            // hypen '-' character are always treated as meaning "no reference"
            // or "invalid texture" and surfaces using them were not drawn.
            if(group != PlaneMaterials && name[0] == '-')
            {
                return 0; // Not a valid id.
            }

            // Prepare the encoded URI for insertion into the dictionary.
            // Material paths must be encoded.
            AutoStr *path = Str_PercentEncode(AutoStr_FromText(name.toUtf8().constData()));
            de::Uri uri(Str_Text(path), RC_NULL);
            uri.setScheme(group == PlaneMaterials? "Flats" : "Textures");

            // Intern this material URI in the dictionary.
            return dict.intern(uri.compose());
        }

        MaterialId toMaterialId(dint uniqueId, MaterialGroup group)
        {
            // Prepare the encoded URI for insertion into the dictionary.
            de::Uri textureUrn(String("urn:%1:%2").arg(group == PlaneMaterials? "Flats" : "Textures").arg(uniqueId), RC_NULL);
            uri_s *uri = Materials_ComposeUri(P_ToIndex(DD_MaterialForTextureUri(reinterpret_cast<uri_s *>(&textureUrn))));
            String uriComposedAsString = Str_Text(Uri_Compose(uri));
            Uri_Delete(uri);

            // Intern this material URI in the dictionary.
            return dict.intern(uriComposedAsString);
        }
    } materials;

    Instance(Public *i) : Base(i), format(Id1MapRecognizer::UnknownFormat)
    {}

    inline dint vertexCount() const {
        return vertCoords.count() / 2;
    }

    inline Vector2d vertexAsVector2d(dint vertexIndex) const {
        return Vector2d(vertCoords[vertexIndex * 2], vertCoords[vertexIndex * 2 + 1]);
    }

    /// @todo fixme: A real performance killer...
    inline AutoStr *composeMaterialRef(MaterialId id) {
        return AutoStr_FromTextStd(materials.find(id).toUtf8().constData());
    }

    void readVertexes(de::Reader &from, dint numElements)
    {
        Id1MapRecognizer::Format format = Id1MapRecognizer::Format(from.version());
        for(dint n = 0; n < numElements; ++n)
        {
            switch(format)
            {
            default:
            case Id1MapRecognizer::DoomFormat: {
                dint16 x, y;
                from >> x >> y;
                vertCoords[n * 2]     = x;
                vertCoords[n * 2 + 1] = y;
                break; }

            case Id1MapRecognizer::Doom64Format: {
                dint32 x, y;
                from >> x >> y;
                vertCoords[n * 2]     = FIX2FLT(x);
                vertCoords[n * 2 + 1] = FIX2FLT(y);
                break; }
            }
        }
    }

    void readLineDefs(de::Reader &reader, dint numElements)
    {
        if(numElements <= 0) return;
        lines.reserve(lines.size() + numElements);
        for(dint n = 0; n < numElements; ++n)
        {
            lines.push_back(LineDef(self));
            LineDef &line = lines.back();
            line << reader;
            line.index = n;
        }
    }

    void readSideDefs(de::Reader &reader, dint numElements)
    {
        if(numElements <= 0) return;
        sides.reserve(sides.size() + numElements);
        for(dint n = 0; n < numElements; ++n)
        {
            sides.push_back(SideDef(self));
            SideDef &side = sides.back();
            side << reader;
            side.index = n;
        }
    }

    void readSectorDefs(de::Reader &reader, dint numElements)
    {
        if(numElements <= 0) return;
        sectors.reserve(sectors.size() + numElements);
        for(dint n = 0; n < numElements; ++n)
        {
            sectors.push_back(SectorDef(self));
            SectorDef &sector = sectors.back();
            sector << reader;
            sector.index = n;
        }
    }

    void readThings(de::Reader &reader, dint numElements)
    {
        if(numElements <= 0) return;
        things.reserve(things.size() + numElements);
        for(dint n = 0; n < numElements; ++n)
        {
            things.push_back(Thing(self));
            Thing &thing = things.back();
            thing << reader;
            thing.index = n;
        }
    }

    void readTintColors(de::Reader &reader, dint numElements)
    {
        if(numElements <= 0) return;
        surfaceTints.reserve(surfaceTints.size() + numElements);
        for(dint n = 0; n < numElements; ++n)
        {
            surfaceTints.push_back(TintColor(self));
            TintColor &tint = surfaceTints.back();
            tint << reader;
            tint.index = n;
        }
    }

    /**
     * Create a temporary polyobj.
     */
    Polyobj *createPolyobj(Polyobj::LineIndices const &lineIndices, dint tag,
        dint sequenceType, dint16 anchorX, dint16 anchorY)
    {
        // Allocate the new polyobj.
        polyobjs.push_back(Polyobj());
        Polyobj *po = &polyobjs.back();

        po->index       = polyobjs.size() - 1;
        po->tag         = tag;
        po->seqType     = sequenceType;
        po->anchor[VX]  = anchorX;
        po->anchor[VY]  = anchorY;
        po->lineIndices = lineIndices; // A copy is made.

        foreach(dint lineIdx, po->lineIndices)
        {
            LineDef *line = &lines[lineIdx];

            // This line now belongs to a polyobj.
            line->aFlags |= LAF_POLYOBJ;

            // Due a logic error in hexen.exe, when the column drawer is presented
            // with polyobj segs built from two-sided linedefs; clipping is always
            // calculated using the pegging logic for single-sided linedefs.
            //
            // Here we emulate this behavior by automatically applying bottom unpegging
            // for two-sided linedefs.
            if(line->hasBack())
            {
                line->ddFlags |= DDLF_DONTPEGBOTTOM;
            }
        }

        return po;
    }

    /**
     * Find all linedefs marked as belonging to a polyobject with the given tag
     * and attempt to create a polyobject from them.
     *
     * @param tag  Line tag of linedefs to search for.
     *
     * @return  @c true = successfully created polyobj.
     */
    bool findAndCreatePolyobj(dint16 tag, dint16 anchorX, dint16 anchorY)
    {
        Polyobj::LineIndices polyLines;

        // First look for a PO_LINE_START linedef set with this tag.
        DENG2_FOR_EACH(Lines, i, lines)
        {
            // Already belongs to another polyobj?
            if(i->aFlags & LAF_POLYOBJ) continue;

            if(!(i->xType == PO_LINE_START && i->xArgs[0] == tag)) continue;

            collectPolyobjLines(polyLines, i);
            if(!polyLines.isEmpty())
            {
                dint8 sequenceType = i->xArgs[2];
                if(sequenceType >= SEQTYPE_NUMSEQ) sequenceType = 0;

                createPolyobj(polyLines, tag, sequenceType, anchorX, anchorY);
                return true;
            }
            return false;
        }

        // Perhaps a PO_LINE_EXPLICIT linedef set with this tag?
        for(dint n = 0; ; ++n)
        {
            bool foundAnotherLine = false;

            DENG2_FOR_EACH(Lines, i, lines)
            {
                // Already belongs to another polyobj?
                if(i->aFlags & LAF_POLYOBJ) continue;

                if(i->xType == PO_LINE_EXPLICIT && i->xArgs[0] == tag)
                {
                    if(i->xArgs[1] <= 0)
                    {
                        LOG_MAP_WARNING("Linedef missing (probably #%d) in explicit polyobj (tag:%d)") << n + 1 << tag;
                        return false;
                    }

                    if(i->xArgs[1] == n + 1)
                    {
                        // Add this line to the list.
                        polyLines.append( i - lines.begin() );
                        foundAnotherLine = true;

                        // Clear any special.
                        i->xType = 0;
                        i->xArgs[0] = 0;
                    }
                }
            }

            if(foundAnotherLine)
            {
                // Check if an explicit line order has been skipped.
                // A line has been skipped if there are any more explicit lines with
                // the current tag value.
                DENG2_FOR_EACH(Lines, i, lines)
                {
                    if(i->xType == PO_LINE_EXPLICIT && i->xArgs[0] == tag)
                    {
                        LOG_MAP_WARNING("Linedef missing (#%d) in explicit polyobj (tag:%d)") << n << tag;
                        return false;
                    }
                }
            }
            else
            {
                // All lines have now been found.
                break;
            }
        }

        if(polyLines.isEmpty())
        {
            LOG_MAP_WARNING("Failed to locate a single line for polyobj (tag:%d)") << tag;
            return false;
        }

        LineDef *line = &lines[ polyLines.first() ];
        dint8 const sequenceType = line->xArgs[3];

        // Setup the mirror if it exists.
        line->xArgs[1] = line->xArgs[2];

        createPolyobj(polyLines, tag, sequenceType, anchorX, anchorY);
        return true;
    }

    void analyze()
    {
        Time begunAt;

        if(format == Id1MapRecognizer::HexenFormat)
        {
            LOGDEV_MAP_XVERBOSE("Locating polyobjs...");
            DENG2_FOR_EACH(Things, i, things)
            {
                // A polyobj anchor?
                if(i->doomEdNum == PO_ANCHOR_DOOMEDNUM)
                {
                    dint const tag = i->angle;
                    findAndCreatePolyobj(tag, i->origin[VX], i->origin[VY]);
                }
            }
        }

        LOGDEV_MAP_MSG("Analyses completed in %.2f seconds") << begunAt.since();
    }

    void transferVertexes()
    {
        LOGDEV_MAP_XVERBOSE("Transfering vertexes...");
        dint const numVertexes = vertexCount();
        dint *indices = new dint[numVertexes];
        for(int i = 0; i < numVertexes; ++i)
        {
            indices[i] = i;
        }
        MPE_VertexCreatev(numVertexes, vertCoords.constData(), indices, 0);
        delete[] indices;
    }

    void transferSectors()
    {
        LOGDEV_MAP_XVERBOSE("Transfering sectors...");

        DENG2_FOR_EACH(Sectors, i, sectors)
        {
            dint idx = MPE_SectorCreate(dfloat(i->lightLevel) / 255.0f, 1, 1, 1, i->index);

            MPE_PlaneCreate(idx, i->floorHeight, composeMaterialRef(i->floorMaterial),
                            0, 0, 1, 1, 1, 1, 0, 0, 1, -1);
            MPE_PlaneCreate(idx, i->ceilHeight, composeMaterialRef(i->ceilMaterial),
                            0, 0, 1, 1, 1, 1, 0, 0, -1, -1);

            MPE_GameObjProperty("XSector", idx, "Tag",    DDVT_SHORT, &i->tag);
            MPE_GameObjProperty("XSector", idx, "Type",   DDVT_SHORT, &i->type);

            if(format == Id1MapRecognizer::Doom64Format)
            {
                MPE_GameObjProperty("XSector", idx, "Flags",          DDVT_SHORT, &i->d64flags);
                MPE_GameObjProperty("XSector", idx, "CeilingColor",   DDVT_SHORT, &i->d64ceilingColor);
                MPE_GameObjProperty("XSector", idx, "FloorColor",     DDVT_SHORT, &i->d64floorColor);
                MPE_GameObjProperty("XSector", idx, "UnknownColor",   DDVT_SHORT, &i->d64unknownColor);
                MPE_GameObjProperty("XSector", idx, "WallTopColor",   DDVT_SHORT, &i->d64wallTopColor);
                MPE_GameObjProperty("XSector", idx, "WallBottomColor", DDVT_SHORT, &i->d64wallBottomColor);
            }
        }
    }

    void transferLinesAndSides()
    {
        LOGDEV_MAP_XVERBOSE("Transfering lines and sides...");
        DENG2_FOR_EACH(Lines, i, lines)
        {
            SideDef *front = (i->hasFront()? &sides[i->front()] : 0);
            SideDef *back  = (i->hasBack() ? &sides[i->back() ] : 0);

            dint sideFlags = (format == Id1MapRecognizer::Doom64Format? SDF_MIDDLE_STRETCH : 0);

            // Interpret the lack of a ML_TWOSIDED line flag to mean the
            // suppression of the side relative back sector.
            if(!(i->flags & 0x4 /*ML_TWOSIDED*/) && front && back)
                sideFlags |= SDF_SUPPRESS_BACK_SECTOR;

            dint lineIdx = MPE_LineCreate(i->v[0], i->v[1], front? front->sector : -1,
                                          back? back->sector : -1, i->ddFlags, i->index);
            if(front)
            {
                MPE_LineAddSide(lineIdx, LineDef::Front, sideFlags,
                                composeMaterialRef(front->topMaterial),
                                front->offset[VX], front->offset[VY], 1, 1, 1,
                                composeMaterialRef(front->middleMaterial),
                                front->offset[VX], front->offset[VY], 1, 1, 1, 1,
                                composeMaterialRef(front->bottomMaterial),
                                front->offset[VX], front->offset[VY], 1, 1, 1,
                                front->index);
            }
            if(back)
            {
                MPE_LineAddSide(lineIdx, LineDef::Back, sideFlags,
                                composeMaterialRef(back->topMaterial),
                                back->offset[VX], back->offset[VY], 1, 1, 1,
                                composeMaterialRef(back->middleMaterial),
                                back->offset[VX], back->offset[VY], 1, 1, 1, 1,
                                composeMaterialRef(back->bottomMaterial),
                                back->offset[VX], back->offset[VY], 1, 1, 1,
                                back->index);
            }

            MPE_GameObjProperty("XLinedef", lineIdx, "Flags", DDVT_SHORT, &i->flags);

            switch(format)
            {
            default:
            case Id1MapRecognizer::DoomFormat:
                MPE_GameObjProperty("XLinedef", lineIdx, "Type",  DDVT_SHORT, &i->dType);
                MPE_GameObjProperty("XLinedef", lineIdx, "Tag",   DDVT_SHORT, &i->dTag);
                break;

            case Id1MapRecognizer::Doom64Format:
                MPE_GameObjProperty("XLinedef", lineIdx, "DrawFlags", DDVT_BYTE,  &i->d64drawFlags);
                MPE_GameObjProperty("XLinedef", lineIdx, "TexFlags",  DDVT_BYTE,  &i->d64texFlags);
                MPE_GameObjProperty("XLinedef", lineIdx, "Type",      DDVT_BYTE,  &i->d64type);
                MPE_GameObjProperty("XLinedef", lineIdx, "UseType",   DDVT_BYTE,  &i->d64useType);
                MPE_GameObjProperty("XLinedef", lineIdx, "Tag",       DDVT_SHORT, &i->d64tag);
                break;

            case Id1MapRecognizer::HexenFormat:
                MPE_GameObjProperty("XLinedef", lineIdx, "Type", DDVT_BYTE, &i->xType);
                MPE_GameObjProperty("XLinedef", lineIdx, "Arg0", DDVT_BYTE, &i->xArgs[0]);
                MPE_GameObjProperty("XLinedef", lineIdx, "Arg1", DDVT_BYTE, &i->xArgs[1]);
                MPE_GameObjProperty("XLinedef", lineIdx, "Arg2", DDVT_BYTE, &i->xArgs[2]);
                MPE_GameObjProperty("XLinedef", lineIdx, "Arg3", DDVT_BYTE, &i->xArgs[3]);
                MPE_GameObjProperty("XLinedef", lineIdx, "Arg4", DDVT_BYTE, &i->xArgs[4]);
                break;
            }
        }
    }

    void transferSurfaceTints()
    {
        if(surfaceTints.empty()) return;

        LOGDEV_MAP_XVERBOSE("Transfering surface tints...");
        DENG2_FOR_EACH(SurfaceTints, i, surfaceTints)
        {
            dint idx = i - surfaceTints.begin();

            MPE_GameObjProperty("Light", idx, "ColorR",   DDVT_FLOAT, &i->rgb[0]);
            MPE_GameObjProperty("Light", idx, "ColorG",   DDVT_FLOAT, &i->rgb[1]);
            MPE_GameObjProperty("Light", idx, "ColorB",   DDVT_FLOAT, &i->rgb[2]);
            MPE_GameObjProperty("Light", idx, "XX0",      DDVT_BYTE,  &i->xx[0]);
            MPE_GameObjProperty("Light", idx, "XX1",      DDVT_BYTE,  &i->xx[1]);
            MPE_GameObjProperty("Light", idx, "XX2",      DDVT_BYTE,  &i->xx[2]);
        }
    }

    void transferPolyobjs()
    {
        if(polyobjs.empty()) return;

        LOGDEV_MAP_XVERBOSE("Transfering polyobjs...");
        DENG2_FOR_EACH(Polyobjs, i, polyobjs)
        {
            MPE_PolyobjCreate(i->lineIndices.constData(), i->lineIndices.count(),
                              i->tag, i->seqType,
                              coord_t(i->anchor[VX]), coord_t(i->anchor[VY]),
                              i->index);
        }
    }

    void transferThings()
    {
        if(things.empty()) return;

        LOGDEV_MAP_XVERBOSE("Transfering things...");
        DENG2_FOR_EACH(Things, i, things)
        {
            dint idx = i - things.begin();

            MPE_GameObjProperty("Thing", idx, "X",            DDVT_SHORT, &i->origin[VX]);
            MPE_GameObjProperty("Thing", idx, "Y",            DDVT_SHORT, &i->origin[VY]);
            MPE_GameObjProperty("Thing", idx, "Z",            DDVT_SHORT, &i->origin[VZ]);
            MPE_GameObjProperty("Thing", idx, "Angle",        DDVT_ANGLE, &i->angle);
            MPE_GameObjProperty("Thing", idx, "DoomEdNum",    DDVT_SHORT, &i->doomEdNum);
            MPE_GameObjProperty("Thing", idx, "SkillModes",   DDVT_INT,   &i->skillModes);
            MPE_GameObjProperty("Thing", idx, "Flags",        DDVT_INT,   &i->flags);

            if(format == Id1MapRecognizer::Doom64Format)
            {
                MPE_GameObjProperty("Thing", idx, "ID",       DDVT_SHORT, &i->d64TID);
            }
            else if(format == Id1MapRecognizer::HexenFormat)
            {
                MPE_GameObjProperty("Thing", idx, "Special",  DDVT_BYTE,  &i->xSpecial);
                MPE_GameObjProperty("Thing", idx, "ID",       DDVT_SHORT, &i->xTID);
                MPE_GameObjProperty("Thing", idx, "Arg0",     DDVT_BYTE,  &i->xArgs[0]);
                MPE_GameObjProperty("Thing", idx, "Arg1",     DDVT_BYTE,  &i->xArgs[1]);
                MPE_GameObjProperty("Thing", idx, "Arg2",     DDVT_BYTE,  &i->xArgs[2]);
                MPE_GameObjProperty("Thing", idx, "Arg3",     DDVT_BYTE,  &i->xArgs[3]);
                MPE_GameObjProperty("Thing", idx, "Arg4",     DDVT_BYTE,  &i->xArgs[4]);
            }
        }
    }

    /**
     * @param lineList  @c NULL, will cause IterFindPolyLines to count the number
     *                  of lines in the polyobj.
     */
    void collectPolyobjLinesWorker(Polyobj::LineIndices &lineList, Vector2d const &point)
    {
        DENG2_FOR_EACH(Lines, i, lines)
        {
            // Already belongs to another polyobj?
            if(i->aFlags & LAF_POLYOBJ) continue;

            // Have we already encounterd this?
            if(i->validCount == validCount) continue;

            if(point == vertexAsVector2d(i->v[0]))
            {
                i->validCount = validCount;
                lineList.append( i - lines.begin() );
                collectPolyobjLinesWorker(lineList, vertexAsVector2d(i->v[1]));
            }
        }
    }

    /**
     * @todo This terribly inefficent (naive) algorithm may need replacing
     *       (it is far outside an acceptable polynomial range!).
     */
    void collectPolyobjLines(Polyobj::LineIndices &lineList, Lines::iterator lineIt)
    {
        LineDef &line = *lineIt;
        line.xType    = 0;
        line.xArgs[0] = 0;

        validCount++;
        // Insert the first line.
        lineList.append(lineIt - lines.begin());
        line.validCount = validCount;
        collectPolyobjLinesWorker(lineList, vertexAsVector2d(line.v[1]));
    }
};

Id1Map::Id1Map(Id1MapRecognizer const &recognized)
    : d(new Instance(this))
{
    d->format = recognized.format();
    if(d->format == Id1MapRecognizer::UnknownFormat)
        throw LoadError("Id1Map", "Format unrecognized");

    // Allocate the vertices first as a large contiguous array suitable for
    // passing directly to Doomsday's MapEdit interface.
    duint vertexCount = recognized.lumps().find(Id1MapRecognizer::VertexData).value()->size()
                      / Id1MapRecognizer::elementSizeForDataType(d->format, Id1MapRecognizer::VertexData);
    d->vertCoords.resize(vertexCount * 2);

    DENG2_FOR_EACH_CONST(Id1MapRecognizer::Lumps, i, recognized.lumps())
    {
        Id1MapRecognizer::DataType dataType = i.key();
        File1 *lump = i.value();

        dsize lumpLength = lump->size();
        if(!lumpLength) continue;

        dsize elemSize = Id1MapRecognizer::elementSizeForDataType(d->format, dataType);
        if(!elemSize) continue;

        // Process this data lump.
        duint const elemCount = lumpLength / elemSize;
        ByteRefArray lumpData(lump->cache(), lumpLength);
        de::Reader reader(lumpData);
        reader.setVersion(d->format);
        switch(dataType)
        {
        default: break;

        case Id1MapRecognizer::VertexData:    d->readVertexes(reader, elemCount);   break;
        case Id1MapRecognizer::LineDefData:   d->readLineDefs(reader, elemCount);   break;
        case Id1MapRecognizer::SideDefData:   d->readSideDefs(reader, elemCount);   break;
        case Id1MapRecognizer::SectorDefData: d->readSectorDefs(reader, elemCount); break;
        case Id1MapRecognizer::ThingData:     d->readThings(reader, elemCount);     break;
        case Id1MapRecognizer::TintColorData: d->readTintColors(reader, elemCount); break;
        }

        lump->unlock();
    }

    // Perform post load analyses.
    d->analyze();
}

void Id1Map::transfer()
{
    LOG_AS("Id1Map");

    Time begunAt;

    MPE_Begin(0/*dummy*/);
        d->transferVertexes();
        d->transferSectors();
        d->transferLinesAndSides();
        d->transferSurfaceTints();
        d->transferPolyobjs();
        d->transferThings();
    MPE_End();

    LOGDEV_MAP_VERBOSE("Transfer completed in %.2f seconds") << begunAt.since();
}

MaterialId Id1Map::toMaterialId(String name, MaterialGroup group)
{
    return d->materials.toMaterialId(name, group);
}

MaterialId Id1Map::toMaterialId(int uniqueId, MaterialGroup group)
{
    return d->materials.toMaterialId(uniqueId, group);
}

} // namespace wadimp
