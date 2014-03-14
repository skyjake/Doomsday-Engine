/** @file savegameconverter.cpp  Utility for converting legacy savegames.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "resource/savegameconverter.h"

#include "dd_main.h"
#include <de/Error>
#include <de/game/SavedSessionRepository>
#include <de/Log>
#include <de/NativePath>

namespace de {

static void tryConversion(Path const &inputFilePath, Path const &outputFilePath)
{
    LOG_DEBUG("Attempting \"%s\"...") << NativePath(inputFilePath).pretty();

    ddhook_savegame_convert_t parm;
    Str_Set(Str_InitStd(&parm.inputFilePath),  NativePath(inputFilePath).expand().asText().toUtf8().constData());
    Str_Set(Str_InitStd(&parm.outputFilePath), NativePath(outputFilePath).expand().asText().toUtf8().constData());

    // Try to convert the savegame via each plugin in turn.
    dd_bool success = DD_CallHooks(HOOK_MAP_CONVERT, 0, &parm);

    Str_Free(&parm.inputFilePath);
    Str_Free(&parm.outputFilePath);

    if(!success)
    {
        /// @throw Error Seemingly no plugin was able to fulfill our request.
        throw Error("SavegameConverter", "Savegame file format was not recognized");
    }
}

bool convertSavegame(Path inputFilePath, game::SavedSession &session)
{
    DENG2_ASSERT(!inputFilePath.isEmpty());

    LOG_AS("SavegameConverter");
    try
    {
        tryConversion(inputFilePath, session.repository().folder().path() / session.fileName());
        // Successful, update the relevant feed and saved session.
        session.repository().folder().populate(Folder::PopulateOnlyThisFolder);
        session.updateFromFile();
        return true;
    }
    catch(Error const &er)
    {
        LOG_RES_WARNING("Failed conversion of \"%s\":\n")
                << NativePath(inputFilePath).pretty() << er.asText();
    }
    return false;
}

} // namespace de
