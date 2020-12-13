/** @file importudmf.cpp  Importer plugin for UDMF maps.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "importudmf.h"
#include "udmfparser.h"

#include <doomsday/filesys/lumpindex.h>
#include <gamefw/mapspot.h>
#include <de/App>
#include <de/Log>

using namespace de;

template <valuetype_t VALUE_TYPE, typename Type>
void gmoSetThingProperty(int index, char const *propertyId, Type value)
{
    MPE_GameObjProperty("Thing", index, propertyId, VALUE_TYPE, &value);
}

template <valuetype_t VALUE_TYPE, typename Type>
void gmoSetSectorProperty(int index, char const *propertyId, Type value)
{
    MPE_GameObjProperty("XSector", index, propertyId, VALUE_TYPE, &value);
}

template <valuetype_t VALUE_TYPE, typename Type>
void gmoSetLineProperty(int index, char const *propertyId, Type value)
{
    MPE_GameObjProperty("XLinedef", index, propertyId, VALUE_TYPE, &value);
}

/**
 * This function will be called when Doomsday is asked to load a map that is not
 * available in its native map format.
 *
 * Our job is to read in the map data structures then use the Doomsday map editing
 * interface to recreate the map in native format.
 */
static int importMapHook(int /*hookType*/, int /*parm*/, void *context)
{
    if (Id1MapRecognizer const *recognizer = reinterpret_cast<Id1MapRecognizer *>(context))
    {
        if (recognizer->format() == Id1MapRecognizer::UniversalFormat)
        {
            LOG_AS("importudmf");
            try
            {
                // Read the contents of the TEXTMAP lump.
                auto *src = recognizer->lumps()[Id1MapRecognizer::UDMFTextmapData];
                Block bytes(src->size());
                src->read(bytes.data(), false);

                // Parse the UDMF source and use the MPE API to create the map elements.
                UDMFParser parser;

                struct ImportState
                {
                    bool isHexen = false;
                    bool isDoom64 = false;

                    int thingCount = 0;
                    int vertexCount = 0;
                    int sectorCount = 0;

                    QList<UDMFParser::Block> linedefs;
                    QList<UDMFParser::Block> sidedefs;
                };
                ImportState importState;

                parser.setGlobalAssignmentHandler([&importState] (String const &ident, QVariant const &value)
                {
                    if (ident == UDMFLex::NAMESPACE)
                    {
                        LOG_MAP_VERBOSE("UDMF namespace: %s") << value.toString();
                        String const ns = value.toString().toLower();
                        if (ns == "hexen")
                        {
                            importState.isHexen = true;
                        }
                        else if (ns == "doom64")
                        {
                            importState.isDoom64 = true;
                        }
                    }
                });

                parser.setBlockHandler([&importState] (String const &type, UDMFParser::Block const &block)
                {
                    if (type == UDMFLex::THING)
                    {
                        int const index = importState.thingCount++;

                        // Properties common to all games.
                        gmoSetThingProperty<DDVT_DOUBLE>(index, "X", block["x"].toDouble());
                        gmoSetThingProperty<DDVT_DOUBLE>(index, "Y", block["y"].toDouble());
                        gmoSetThingProperty<DDVT_DOUBLE>(index, "Z", block["z"].toDouble());
                        gmoSetThingProperty<DDVT_ANGLE>(index, "Angle", angle_t(double(block["angle"].toInt()) / 180.0 * ANGLE_180));
                        gmoSetThingProperty<DDVT_INT>(index, "DoomEdNum", block["type"].toInt());

                        // Map spot flags.
                        {
                            gfw_mapspot_flags_t gfwFlags = 0;

                            if (block["ambush"].toBool())      gfwFlags |= GFW_MAPSPOT_DEAF;
                            if (block["single"].toBool())      gfwFlags |= GFW_MAPSPOT_SINGLE;
                            if (block["dm"].toBool())          gfwFlags |= GFW_MAPSPOT_DM;
                            if (block["coop"].toBool())        gfwFlags |= GFW_MAPSPOT_COOP;
                            if (block["friend"].toBool())      gfwFlags |= GFW_MAPSPOT_MBF_FRIEND;
                            if (block["dormant"].toBool())     gfwFlags |= GFW_MAPSPOT_DORMANT;
                            if (block["class1"].toBool())      gfwFlags |= GFW_MAPSPOT_CLASS1;
                            if (block["class2"].toBool())      gfwFlags |= GFW_MAPSPOT_CLASS2;
                            if (block["class3"].toBool())      gfwFlags |= GFW_MAPSPOT_CLASS3;
                            if (block["standing"].toBool())    gfwFlags |= GFW_MAPSPOT_STANDING;
                            if (block["strifeally"].toBool())  gfwFlags |= GFW_MAPSPOT_STRIFE_ALLY;
                            if (block["translucent"].toBool()) gfwFlags |= GFW_MAPSPOT_TRANSLUCENT;
                            if (block["invisible"].toBool())   gfwFlags |= GFW_MAPSPOT_INVISIBLE;

                            gmoSetThingProperty<DDVT_INT>(index, "Flags",
                                    gfw_MapSpot_TranslateFlagsToInternal(gfwFlags));
                        }

                        // Skill level bits.
                        {
                            static String const labels[5] = {
                                "skill1", "skill2", "skill3", "skill4", "skill5",
                            };
                            int skillModes = 0;
                            for (int skill = 0; skill < 5; ++skill)
                            {
                                if (block[labels[skill]].toBool())
                                    skillModes |= 1 << skill;
                            }
                            gmoSetThingProperty<DDVT_INT>(index, "SkillModes", skillModes);
                        }

                        if (importState.isHexen || importState.isDoom64)
                        {
                            gmoSetThingProperty<DDVT_INT>(index, "ID", block["id"].toInt());
                        }
                        if (importState.isHexen)
                        {
                            gmoSetThingProperty<DDVT_INT>(index, "Special", block["special"].toInt());
                            gmoSetThingProperty<DDVT_INT>(index, "Arg0", block["arg0"].toInt());
                            gmoSetThingProperty<DDVT_INT>(index, "Arg1", block["arg1"].toInt());
                            gmoSetThingProperty<DDVT_INT>(index, "Arg2", block["arg2"].toInt());
                            gmoSetThingProperty<DDVT_INT>(index, "Arg3", block["arg3"].toInt());
                            gmoSetThingProperty<DDVT_INT>(index, "Arg4", block["arg4"].toInt());
                        }
                    }
                    else if (type == UDMFLex::VERTEX)
                    {
                        int const index = importState.vertexCount++;

                        MPE_VertexCreate(block["x"].toDouble(), block["y"].toDouble(), index);
                    }
                    else if (type == UDMFLex::LINEDEF)
                    {
                        importState.linedefs.append(block);
                    }
                    else if (type == UDMFLex::SIDEDEF)
                    {
                        importState.sidedefs.append(block);
                    }
                    else if (type == UDMFLex::SECTOR)
                    {
                        const int index = importState.sectorCount++;
                        const int lightlevel = block.contains("lightlevel")? block["lightlevel"].toInt() : 160;
                        const struct de_api_sector_hacks_s hacks{{0, 0}, -1};

                        MPE_SectorCreate(float(lightlevel)/255.f, 1.f, 1.f, 1.f, &hacks, index);

                        MPE_PlaneCreate(index,
                                        block["heightfloor"].toDouble(),
                                        de::Str("Flats:" + block["texturefloor"].toString()),
                                        0.f, 0.f,
                                        1.f, 1.f, 1.f,  // color
                                        1.f,            // opacity
                                        0, 0, 1.f,      // normal
                                        -1);            // index in archive

                        MPE_PlaneCreate(index,
                                        block["heightceiling"].toDouble(),
                                        de::Str("Flats:" + block["textureceiling"].toString()),
                                        0.f, 0.f,
                                        1.f, 1.f, 1.f,  // color
                                        1.f,            // opacity
                                        0, 0, -1.f,     // normal
                                        -1);            // index in archive

                        gmoSetSectorProperty<DDVT_INT>(index, "Type", block["special"].toInt());
                        gmoSetSectorProperty<DDVT_INT>(index, "Tag",  block["id"].toInt());
                    }
                });

                parser.parse(String::fromUtf8(bytes));

                // Now that all the linedefs and sidedefs are read, let's create them.
                for (int index = 0; index < importState.linedefs.size(); ++index)
                {
                    UDMFParser::Block const &linedef = importState.linedefs.at(index);

                    int sidefront = linedef["sidefront"].toInt();
                    int sideback  = linedef.contains("sideback")? linedef["sideback"].toInt() : -1;

                    UDMFParser::Block const &front = importState.sidedefs.at(sidefront);
                    UDMFParser::Block const *back  =
                            (sideback >= 0? &importState.sidedefs.at(sideback) : nullptr);

                    int frontSectorIdx = front["sector"].toInt();
                    int backSectorIdx  = back? (*back)["sector"].toInt() : -1;

                    // Line flags.
                    int ddLineFlags = 0;
                    short sideFlags = 0;
                    {
                        bool const blocking      = linedef["blocking"].toBool();
                        bool const dontpegtop    = linedef["dontpegtop"].toBool();
                        bool const dontpegbottom = linedef["dontpegbottom"].toBool();
                        bool const twosided      = linedef["twosided"].toBool();

                        if (blocking)      ddLineFlags |= DDLF_BLOCKING;
                        if (dontpegtop)    ddLineFlags |= DDLF_DONTPEGTOP;
                        if (dontpegbottom) ddLineFlags |= DDLF_DONTPEGBOTTOM;

                        if (!twosided && back)
                        {
                            sideFlags |= SDF_SUPPRESS_BACK_SECTOR;
                        }
                    }

                    MPE_LineCreate(linedef["v1"].toInt(),
                                   linedef["v2"].toInt(),
                                   frontSectorIdx,
                                   backSectorIdx,
                                   ddLineFlags,
                                   index);

                    auto texName = [] (QVariant tex) -> String {
                        if (tex.toString().isEmpty()) return String();
                        return "Textures:" + tex.toString();
                    };

                    auto addSide = [&texName, sideFlags](
                                       int index, const UDMFParser::Block &side, int sideIndex)
                    {
                        const int offsetx = side["offsetx"].toInt();
                        const int offsety = side["offsety"].toInt();
                        float     opacity = 1.f;

                        const auto topTex = texName(side["texturetop"]   ).toUtf8();
                        const auto midTex = texName(side["texturemiddle"]).toUtf8();
                        const auto botTex = texName(side["texturebottom"]).toUtf8();

                        struct de_api_side_section_s top = {
                            topTex,
                            {float(offsetx), float(offsety)},
                            {1, 1, 1, 1}
                        };

                        struct de_api_side_section_s mid = {
                            midTex,
                            {float(offsetx), float(offsety)},
                            {1, 1, 1, opacity}
                        };

                        struct de_api_side_section_s bot = {
                            botTex,
                            {float(offsetx), float(offsety)},
                            {1, 1, 1, 1}
                        };

                        MPE_LineAddSide(
                            index,
                            0 /* front */,
                            sideFlags,
                            &top,
                            &mid,
                            &bot,
                            sideIndex);
                    };

                    // Front side.
                    addSide(index, front, sidefront);

                    // Back side.
                    if (back)
                    {
                        addSide(index, *back, sideback);
                    }

                    // More line flags.
                    {
                        short flags = 0;

                        // TODO: Check all the flags.

                        gmoSetLineProperty<DDVT_SHORT>(index, "Flags", flags);
                    }

                    gmoSetLineProperty<DDVT_INT>(index, "Type", linedef["special"].toInt());

                    if (!importState.isHexen)
                    {
                        gmoSetLineProperty<DDVT_INT>(index, "Tag",
                                                     linedef.contains("id")?
                                                         linedef["id"].toInt() : -1);
                    }
                    if (importState.isHexen)
                    {
                        gmoSetLineProperty<DDVT_INT>(index, "Arg0", linedef["arg0"].toInt());
                        gmoSetLineProperty<DDVT_INT>(index, "Arg1", linedef["arg1"].toInt());
                        gmoSetLineProperty<DDVT_INT>(index, "Arg2", linedef["arg2"].toInt());
                        gmoSetLineProperty<DDVT_INT>(index, "Arg3", linedef["arg3"].toInt());
                        gmoSetLineProperty<DDVT_INT>(index, "Arg4", linedef["arg4"].toInt());
                    }
                }
                LOG_MAP_WARNING("Loading UDMF maps is an experimental feature");
                return true;
            }
            catch (Error const &er)
            {
                LOG_MAP_ERROR("Error while loading UDMF: ") << er.asText();
            }
        }
    }
    return false;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
DENG_ENTRYPOINT void DP_Initialize()
{
    Plug_AddHook(HOOK_MAP_CONVERT, importMapHook);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
DENG_ENTRYPOINT char const *deng_LibraryType()
{
    return "deng-plugin/generic";
}

#if defined (DENG_STATIC_LINK)

DENG_EXTERN_C void *staticlib_importudmf_symbol(char const *name)
{
    DENG_SYMBOL_PTR(name, deng_LibraryType)
    DENG_SYMBOL_PTR(name, DP_Initialize);
    qWarning() << name << "not found in importudmf";
    return nullptr;
}

#else

DENG_DECLARE_API(Map);
DENG_DECLARE_API(Material);
DENG_DECLARE_API(MPE);

DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_MAP, Map);
    DENG_GET_API(DE_API_MATERIALS, Material);
    DENG_GET_API(DE_API_MAP_EDIT, MPE);
)

#endif
