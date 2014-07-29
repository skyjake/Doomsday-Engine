/** @file idtech1converter.cpp  Converter plugin for id Tech 1 resource formats.
 *
 * @ingroup idtech1converter
 *
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#include "idtech1converter.h"
#include "mapinfotranslator.h"
#include <de/Error>
#include <de/Log>

using namespace de;
using namespace idtech1;

/**
 * This function will be called when Doomsday is asked to load a map that is not
 * available in its native map format.
 *
 * Our job is to read in the map data structures then use the Doomsday map editing
 * interface to recreate the map in native format.
 *
 * @note In the future the Id1MapRecognizer will @em not be supplied by the engine.
 * Instead the Wad format interpreter, the LumpIndex and all associated components
 * will be implemented by this plugin. During application init the plugin should
 * register the Wad format interpreter and locate the resources when such a file
 * is interpreted. Therefore, Id1MapRecognizer instances will be plugin-local and
 * associated with the unique identifier of the map. When such a map resource must
 * be converted, the engine will specify this identifier and the plugin will then
 * locate the recognizer with which to perform the conversion.
 */
int ConvertMapHook(int /*hookType*/, int /*parm*/, void *context)
{
    DENG2_ASSERT(context != 0);
    Id1MapRecognizer const &recognizer = *reinterpret_cast<de::Id1MapRecognizer *>(context);

    if(recognizer.format() != Id1MapRecognizer::UnknownFormat)
    {
        // Attempt a conversion...
        try
        {
            QScopedPointer<MapImporter> map(new MapImporter(recognizer));

            // The archived map data was read successfully.
            // Transfer to the engine via the runtime map editing interface.
            /// @todo Build it using native components directly...
            LOG_AS("IdTech1Converter");
            map->transfer();
            return true; // success
        }
        catch(MapImporter::LoadError const &er)
        {
            LOG_AS("IdTech1Converter");
            LOG_MAP_ERROR("Load error: ") << er.asText();
        }
    }

    return false; // failure :(
}

static void readOneMapInfoDefinition(MapInfoParser &parser, AutoStr const &buffer, String sourceFile)
{
    LOG_RES_VERBOSE("Parsing \"%s\"...") << NativePath(sourceFile).pretty();
    try
    {
        parser.parse(buffer, sourceFile);
    }
    catch(MapInfoParser::ParseError const &er)
    {
        LOG_RES_WARNING("Failed parsing \"%s\" as MAPINFO:\n%s")
                << NativePath(sourceFile).pretty() << er.asText();
    }
}

void ConvertMapInfo()
{
    HexDefs hexDefs;

    // Initialize a new parser.
    MapInfoParser parser(hexDefs);

    // Read the primary MAPINFO (from the IWAD).
    AutoStr *sourceFile = AutoStr_FromText("Lumps:MAPINFO");
    dd_bool sourceIsCustom;
    AutoStr *buffer = M_ReadFileIntoString(sourceFile, &sourceIsCustom);
    if(buffer && !Str_IsEmpty(buffer))
    {
        readOneMapInfoDefinition(parser, *buffer, Str_Text(sourceFile));

#ifdef __JHEXEN__
        // MAPINFO in the Hexen IWAD contains a bunch of broken definitions.
        // As later map definitions now replace earlier ones, these broken defs
        // override the earlier "good" defs. For now we'll kludge around this
        // issue by patching the affected defs with the expected values.
        if(!sourceIsCustom && (gameModeBits & (GM_HEXEN|GM_HEXEN_V10)))
        {
            MapInfo *info = hexDefs.getMapInfo(de::Uri("Maps:MAP07", RC_NULL));
            info->set("warpTrans", "@wt:6");
        }
#endif
    }
    else
    {
        LOG_RES_WARNING("MapInfoParser: Failed to open definition/script file \"%s\" for reading")
                << NativePath(Str_Text(sourceFile)).pretty();
    }

    // Translate internal "warp trans" numbers to URIs.
    hexDefs.translateMapWarpNumbers();

#ifdef DENG_IDTECH1CONVERTER_DEBUG
    for(HexDefs::MapInfos::const_iterator i = hexDefs.mapInfos.begin(); i != hexDefs.mapInfos.end(); ++i)
    {
        MapInfo const &info = i->second;
        LOG_RES_MSG("MAPINFO %s { title: \"%s\" hub: %i map: %s warp: %i nextMap: %s }")
                << i->first.c_str() << info.gets("title")
                << info.geti("hub") << info.gets("map") << info.geti("warpTrans") << info.gets("nextMap");
    }
#endif
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
extern "C" void DP_Initialize()
{
    Plug_AddHook(HOOK_MAP_CONVERT, ConvertMapHook);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
extern "C" char const *deng_LibraryType()
{
    return "deng-plugin/generic";
}

DENG_DECLARE_API(Base);
DENG_DECLARE_API(F);
DENG_DECLARE_API(Material);
DENG_DECLARE_API(Map);
DENG_DECLARE_API(MPE);
DENG_DECLARE_API(Plug);
DENG_DECLARE_API(Uri);

DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_BASE, Base);
    DENG_GET_API(DE_API_FILE_SYSTEM, F);
    DENG_GET_API(DE_API_MATERIALS, Material);
    DENG_GET_API(DE_API_MAP, Map);
    DENG_GET_API(DE_API_MAP_EDIT, MPE);
    DENG_GET_API(DE_API_PLUGIN, Plug);
    DENG_GET_API(DE_API_URI, Uri);
)
