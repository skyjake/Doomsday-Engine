/** @file savegameconverter.h  Legacy savegame converter plugin.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#include <de/App>
#include <de/CommandLine>
#include <de/DirectoryFeed>
#include <de/Error>
#include <de/Log>
#include <de/NativeFile>
#include <de/NativePath>

#include "savegameconverter.h"

using namespace de;

static NativePath findSavegameTool()
{
#ifdef MACOSX
    return App::executablePath().fileNamePath() / "../Resources/savegametool";
    /// @todo fixme: Need to try alternate locations?
#elif WIN32
    return App::executablePath().fileNamePath() / "savegametool.exe";
#else // UNIX
    return App::executablePath().fileNamePath() / "savegametool";
    /// @todo fixme: Need to try alternate locations?
#endif

}

int SavegameConvertHook(int /*hook_type*/, int /*parm*/, void *data)
{
    DENG2_ASSERT(data != 0);
    ddhook_savegame_convert_t const &parm = *static_cast<ddhook_savegame_convert_t *>(data);

    LOG_AS("SavegameConverter");

    // First locate the savegametool executable.
    NativePath bin = findSavegameTool();
    if(!bin.exists())
    {
        LOG_RES_ERROR("Failed to locate Savegame Tool");
        return false;
    }
    CommandLine cmd;
    cmd << bin;

    // Specify the fallback game identity key for ambiguous format resolution.
    cmd << "-idkey" << Str_Text(&parm.fallbackGameId);

    // We can only convert native files and output to native folders using Savegame Tool.
    Path const outputPath(Str_Text(&parm.outputPath));
    Path const sourcePath(Str_Text(&parm.sourcePath));
    try
    {
        // Redirect output to the folder specified.
        cmd << "-output" << App::rootFolder().locate<Folder>(outputPath)
                                                .feeds().front()->as<DirectoryFeed>().nativePath().expand();

        // Add the path of the savegame to be converted.
        cmd << App::rootFolder().locate<NativeFile>(sourcePath).nativePath();

        LOG_RES_NOTE("Starting conversion of \"%s\" using Savegame Tool") << sourcePath;
        cmd.executeAndWait();
        return true;
    }
    catch(Error const &er)
    {
        LOG_RES_NOTE("Failed conversion of \"%s\":\n") << sourcePath << er.asText();
    }

    return false;
}

/**
 * This function is called automatically when the plugin is loaded. We let the engine know
 * what we'd like to do.
 */
void DP_Initialize()
{
    Plug_AddHook(HOOK_SAVEGAME_CONVERT, SavegameConvertHook);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called automatically
 * when the plugin is loaded.
 */
extern "C" char const *deng_LibraryType()
{
    return "deng-plugin/generic";
}

DENG_DECLARE_API(Base);
DENG_DECLARE_API(Plug);

DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_BASE, Base);
    DENG_GET_API(DE_API_PLUGIN, Plug);
)
