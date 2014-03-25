/** @file gamesessionwriter.cpp  Serializing game state to a saved session.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "gamesessionwriter.h"

#include "g_common.h"
#include "mapstatewriter.h"
#include "p_saveio.h"
#include <de/App>
#include <de/game/SavedSessionRepository>
#include <de/Time>
#include <de/Writer>
#include <de/ZipArchive>

using namespace de;

DENG2_PIMPL_NOREF(GameSessionWriter)
{
    String savePath;

    inline de::game::SavedSessionRepository &saveRepo() {
        return G_SavedSessionRepository();
    }

    String composeInfo(SessionMetadata const &metadata)
    {
        String info;
        QTextStream os(&info);
        os.setCodec("UTF-8");

        // Write header and misc info.
        Time now;
        os <<   "# Doomsday Engine saved game session package.\n#"
           << "\n# Generator: GameSessionWriter (libcommon)"
           << "\n# Generation Date: " + now.asDateTime().toString(Qt::SystemLocaleShortDate);

        // Write metadata.
        os << "\n\n" + metadata.asTextWithInfoSyntax() + "\n";

        return info;
    }

    void writeACScriptState(ZipArchive &arch)
    {
#if __JHEXEN__
        Block data;
        de::Writer writer(data);
        Game_ACScriptInterpreter().writeWorldScriptData(writer);
        arch.add("ACScriptState", data);
#endif
    }

    void writeMapState(ZipArchive &arch, String const &mapUriStr)
    {
        Block data;
        SV_OpenFileForWrite(data);
        writer_s *writer = SV_NewWriter();
        MapStateWriter().write(writer);
        arch.add(String("maps") / mapUriStr + "State", data);
        Writer_Delete(writer);
        SV_CloseFile();
    }
};

GameSessionWriter::GameSessionWriter(String const &savePath) : d(new Instance())
{
    DENG2_ASSERT(!savePath.isEmpty());
    d->savePath = savePath;
}

void GameSessionWriter::write(SessionMetadata const &metadata)
{
    LOG_AS("GameSessionWriter");
    LOG_RES_VERBOSE("Serializing to \"%s\"...") << d->savePath;

    // Serialize the game state to a new .save package.
    ZipArchive arch;
    arch.add("Info", d->composeInfo(metadata).toUtf8());
    d->writeACScriptState(arch);
    d->writeMapState(arch, Str_Text(Uri_Compose(gameMapUri))); // current map.

    d->saveRepo().remove(d->savePath);

    // Write the new package to /home/savegames/<game-id>/<session-name>.save
    Folder &folder = DENG2_APP->rootFolder().locate<Folder>(d->savePath.fileNamePath());
    File &save = folder.replaceFile(d->savePath.fileName());
    de::Writer(save) << arch;
    save.setMode(File::ReadOnly);
    LOG_RES_MSG("Wrote ") << save.description();
    folder.populate();

    SavedSession &session = folder.locate<SavedSession>(d->savePath.fileName());
    session.cacheMetadata(metadata); // Avoid immediately reopening the .save package.
    d->saveRepo().add(session);
}
