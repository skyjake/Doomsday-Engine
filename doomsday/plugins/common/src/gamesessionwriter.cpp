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

#include "d_net.h"           // NetSv_SaveGame
#include "g_common.h"
#include "mapstatewriter.h"
#include "p_saveio.h"
#include <de/App>
#include <de/NativeFile>
#include <de/PackageFolder>
#include <de/Time>
#include <de/Writer>
#include <de/ZipArchive>

using namespace de;
using de::game::SavedSession;
using de::game::SessionMetadata;

DENG2_PIMPL_NOREF(GameSessionWriter)
{
    SavedSession &session; // Saved session to be updated. Not owned.
    Instance(SavedSession &session) : session(session) {}

    String composeInfo(SessionMetadata const &metadata) const
    {
        String info;
        QTextStream os(&info);
        os.setCodec("UTF-8");

        // Write header and misc info.
        Time now;
        os << "# Doomsday Engine saved game session package.\n#"
           << "\n# Generator: GameSessionWriter (libcommon)"
           << "\n# Generation Date: " + now.asDateTime().toString(Qt::SystemLocaleShortDate);

        // Write metadata.
        os << "\n\n" + metadata.asTextWithInfoSyntax() + "\n";

        return info;
    }
};

GameSessionWriter::GameSessionWriter(SavedSession &session)
    : d(new Instance(session))
{}

void GameSessionWriter::write(String const &userDescription)
{
    SessionMetadata const *metadata = G_CurrentSessionMetadata(userDescription);

    // In networked games the server tells the clients to save their games.
#if !__JHEXEN__
    NetSv_SaveGame(metadata->geti("sessionId"));
#endif

    // Write the Info file for this .save package.
    ZipArchive arch;
    arch.add("Info", d->composeInfo(*metadata).toUtf8());

#if __JHEXEN__
    // Serialize the world ACScript state.
    {
        Block worldACScriptData;
        de::Writer writer(worldACScriptData);
        Game_ACScriptInterpreter().writeWorldScriptData(writer);
        arch.add("ACScriptState", worldACScriptData);
    }
#endif

    // Serialize the current map state.
    {
        Block mapStateData;
        SV_OpenFileForWrite(mapStateData);
        writer_s *writer = SV_NewWriter();
        MapStateWriter().write(writer);
        arch.add(Path("maps") / metadata->gets("mapUri") + "State", mapStateData);
        Writer_Delete(writer);
    }

    File &outFile = App::rootFolder().locate<Folder>("/savegame").replaceFile(d->session.path() + ".save");
    de::Writer(outFile) << arch;
    outFile.flush();
    LOG_MSG("Wrote ") << outFile.as<NativeFile>().nativePath().pretty();

    // Update the cached metadata so we can try to avoid reopening the session.
    d->session.replaceMetadata(const_cast<SessionMetadata *>(metadata));
}
