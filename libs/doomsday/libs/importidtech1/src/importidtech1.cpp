/** @file importidtech1.cpp  Importer plugin for id Tech 1 resource formats.
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

#include "importidtech1.h"
#include "mapinfotranslator.h"
#include <doomsday/filesys/lumpindex.h>
#include <de/app.h>
#include <de/extension.h>
#include <de/log.h>

using namespace de;
using namespace idtech1;
using namespace res;

static inline AutoStr *readFileIntoString(const String &path, bool *isCustom = 0)
{
    dd_bool _isCustom;
    ddstring_t sourcePath;
    AutoStr *string = M_ReadFileIntoString(Str_Set(Str_InitStd(&sourcePath), path), &_isCustom);
    if(isCustom) *isCustom = _isCustom;
    Str_Free(&sourcePath);
    return string;
}

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
static int ConvertMapHook(int /*hookType*/, int /*parm*/, void *context)
{
    DE_ASSERT(context != 0);
    const auto &recognizer = *reinterpret_cast<Id1MapRecognizer *>(context);

    if (recognizer.format() != Id1MapRecognizer::UnknownFormat &&
        recognizer.format() != Id1MapRecognizer::UniversalFormat)
    {
        LOG_AS("importidtech1");
        // Attempt a conversion...
        try
        {
            std::unique_ptr<MapImporter> map(new MapImporter(recognizer));

            // The archived map data was read successfully.
            // Transfer to the engine via the runtime map editing interface.
            /// @todo Build it using native components directly...
            map->transfer();
            return true; // success
        }
        catch(const MapImporter::LoadError &er)
        {
            LOG_MAP_ERROR("Load error: ") << er.asText();
        }
    }

    return false; // failure :(
}

static void convertMapInfos(const StringList &pathsInLoadOrder, String &xlat, String &xlatCustom)
{
    xlat.clear();
    xlatCustom.clear();

    MapInfoTranslator translator;

    bool haveTranslation = false;
    for(const String &sourcePath : pathsInLoadOrder)
    {
        if(sourcePath.isEmpty()) continue;

        bool sourceIsCustom;
        if(AutoStr *definitions = readFileIntoString(sourcePath, &sourceIsCustom))
        {
            translator.merge(*definitions, sourcePath, sourceIsCustom);
            haveTranslation = true;
        }
    }
    if(!haveTranslation) return;

    translator.translate(xlat, xlatCustom);
}

/**
 * This function will be called when Doomsday needs to translate a MAPINFO definition set.
 * @return  @c true if successful (always).
 */
static int ConvertMapInfoHook(int /*hookType*/, int /*parm*/, void *context)
{
    LOG_AS("importidtech1");
    DE_ASSERT(context);
    auto &parm = *static_cast<ddhook_mapinfo_convert_t *>(context);
    StringList allPathsInLoadOrder = String(Str_Text(&parm.paths)).split(";");
    String xlat, xlatCustom;
    convertMapInfos(allPathsInLoadOrder, xlat, xlatCustom);
    Str_Set(&parm.translated, xlat);
    Str_Set(&parm.translatedCustom, xlatCustom);
    return true;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
static void DP_Initialize()
{
    Plug_AddHook(HOOK_MAP_CONVERT,     ConvertMapHook);
    Plug_AddHook(HOOK_MAPINFO_CONVERT, ConvertMapInfoHook);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
static const char *deng_LibraryType()
{
    return "deng-plugin/generic";
}

DE_ENTRYPOINT void *extension_importidtech1_symbol(const char *name)
{
    DE_SYMBOL_PTR(name, deng_LibraryType)
    DE_SYMBOL_PTR(name, DP_Initialize);
    warning("\"%s\" not found in importidtech1", name);
    return nullptr;
}

//#else

//DE_DECLARE_API(Base);
//DE_DECLARE_API(F);
//DE_DECLARE_API(Map);
//DE_DECLARE_API(Material);
//DE_DECLARE_API(MPE);
//DE_DECLARE_API(Uri);

//DE_API_EXCHANGE(
//    DE_GET_API(DE_API_BASE, Base);
//    DE_GET_API(DE_API_FILE_SYSTEM, F);
//    DE_GET_API(DE_API_MAP, Map);
//    DE_GET_API(DE_API_MATERIALS, Material);
//    DE_GET_API(DE_API_MAP_EDIT, MPE);
//    DE_GET_API(DE_API_URI, Uri);
//)

//#endif
