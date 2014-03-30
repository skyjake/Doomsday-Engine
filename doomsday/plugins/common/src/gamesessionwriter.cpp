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
using de::game::SavedSession;
using de::game::SavedSessionRepository;
using de::game::SessionMetadata;

namespace common {
namespace internal {

class GameSessionWriter
{
    Folder &folder;
    String const &fileName;
    SessionMetadata const &metadata;

public:
    GameSessionWriter(Folder &saveFolder, String const &saveFileName,
                      SessionMetadata const &metadata)
        : folder  (saveFolder)
        , fileName(saveFileName)
        , metadata(metadata)
    {}

    inline SavedSessionRepository &saveRepo() {
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

#if __JHEXEN__
    Block serializeACScriptWorldState()
    {
        Block data;
        de::Writer writer(data);
        Game_ACScriptInterpreter().writeWorldState(writer);
        return data;
    }
#endif

    Block serializeCurrentMapState()
    {
        Block data;
        SV_OpenFileForWrite(data);
        writer_s *writer = SV_NewWriter();
        MapStateWriter().write(writer);
        Writer_Delete(writer);
        SV_CloseFile();
        return data;
    }

    void serializeState(Archive &arch)
    {
        arch.add("Info", composeInfo(metadata).toUtf8());

#if __JHEXEN__
        de::Writer(arch.entryBlock("ACScriptState")).withHeader()
                << serializeACScriptWorldState();
#endif

        arch.add(String("maps") / Str_Text(Uri_Compose(gameMapUri)) + "State",
                 serializeCurrentMapState());
    }

    void write()
    {
        LOG_AS("GameSessionWriter");
        LOG_RES_VERBOSE("Serializing to \"%s\"...") << (folder.path() / fileName);

        ZipArchive arch;
        serializeState(arch);

        saveRepo().remove(folder.path() / fileName);

        if(SavedSession *existing = folder.tryLocate<SavedSession>(fileName))
        {
            existing->setMode(File::Write);
        }
        File &save = folder.replaceFile(fileName);
        de::Writer(save) << arch;
        save.setMode(File::ReadOnly);
        LOG_RES_MSG("Wrote ") << save.description();

        // We can now reinterpret and populate the contents of the archive.
        File *updated = save.reinterpret();
        updated->as<Folder>().populate();

        SavedSession &session = updated->as<SavedSession>();
        session.cacheMetadata(metadata); // Avoid immediately reopening the .save package.
        saveRepo().add(session);
    }
};

} // namespace internal

void writeGameSession(Folder &saveFolder, String const &saveFileName, SessionMetadata const &metadata)
{
    return internal::GameSessionWriter(saveFolder, saveFileName, metadata).write();
}

} // namespace common
