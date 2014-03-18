/** @file id1translator.cpp  Savegame translator for id Tech1 formats.
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

#include <cstring> // memcpy
#include <de/TextApp>
#include <de/File>
#include <de/FixedByteArray>
#include <de/Reader>
#include <de/Writer>
#include <de/ZipArchive>
#include "id1translator.h"

extern de::String fallbackGameId;

using namespace de;

DENG2_PIMPL(Id1Translator)
{
    FormatId id;
    File *saveFilePtr;
    dint32 saveVersion;

    Instance(Public *i)
        : Base(i)
        , id         (DoomV9)
        , saveFilePtr(0)
        , saveVersion(0)
    {}

    ~Instance()
    {
        closeFile();
    }

    /**
     * Returns the native "magic" identifier, used for format recognition.
     */
    dint32 magic() const
    {
        switch(id)
        {
        case DoomV9:     return 0x1DEAD666;
        case HereticV13: return 0x7D9A12C5;
        }
        DENG2_ASSERT(!"Id1Translator::formatName: Invalid format id");
        return 0;
    }

    File *saveFile()
    {
        DENG2_ASSERT(saveFilePtr != 0);
        return saveFilePtr;
    }

    File const *saveFile() const
    {
        DENG2_ASSERT(saveFilePtr != 0);
        return saveFilePtr;
    }

    void openFile(Path path)
    {
        DENG2_ASSERT(!"openFile -- not yet implemented");
        /*LOG_TRACE("openFile: Opening \"%s\"") << NativePath(path).pretty();
        DENG2_ASSERT(saveFilePtr == 0);
        saveFilePtr = 0;
        if(!saveFilePtr)
        {
            throw FileOpenError("Id1Translator", "Failed opening \"" + NativePath(path).pretty() + "\"");
        }*/
    }

    void closeFile()
    {
        if(saveFilePtr)
        {
            saveFilePtr = 0;
        }
    }

    Block *bufferFile() const
    {
        DENG2_ASSERT(!"bufferFile -- not yet implemented");
        return 0;
    }

    void translateInfo(SessionMetadata &/*metadata*/, Reader &/*from*/)
    {
        DENG2_ASSERT(!"translateInfo -- not yet implemented");
    }
};

Id1Translator::Id1Translator(FormatId id, QStringList knownExtensions, QStringList baseGameIdKeys)
    : PackageFormatter(knownExtensions, baseGameIdKeys)
    , d(new Instance(this))
{
    d->id = id;
}

Id1Translator::~Id1Translator()
{}

String Id1Translator::formatName() const
{
    switch(d->id)
    {
    case DoomV9:      return "Doom (id Tech 1)";
    case HereticV13:  return "Heretic (id Tech 1)";
    }
    DENG2_ASSERT(!"Id1Translator::formatName: Invalid format id");
    return "";
}

bool Id1Translator::recognize(Path /*path*/)
{
    LOG_AS("Id1Translator");
    // id Tech1 save formats cannot be recognized, "fuzzy" logic is required.
    return false;
}

void Id1Translator::convert(Path oldSavePath)
{
    LOG_AS("Id1Translator");

    /// @todo try all known extensions at the given path, if not specified.
    String saveName = oldSavePath.lastSegment().toString();

    d->openFile(oldSavePath);
    Reader *from = new Reader(*d->saveFile());

    // Read and translate the game session metadata.
    SessionMetadata metadata;
    d->translateInfo(metadata, *from);

    ZipArchive arch;
    arch.add("Info", composeInfo(metadata, oldSavePath, 1).toUtf8());

    // The only serialized map state follows the session metadata in the game state file.
    // Buffer the rest of the file and write it out to a new map state file.
    if(Block *xlatedData = d->bufferFile())
    {
        // Append the remaining translated data to header, forming the new serialized
        // map state data file.
        Block *mapStateData = composeMapStateHeader(d->magic(), d->saveVersion);
        *mapStateData += *xlatedData;
        delete xlatedData;

        arch.add(Path("maps") / metadata.gets("mapUri") + "State", *mapStateData);
        delete mapStateData;
    }

    delete from;
    d->closeFile();

    File &outFile = DENG2_TEXT_APP->homeFolder().replaceFile(saveName.fileNameWithoutExtension() + ".save");
    outFile.setMode(File::Write | File::Truncate);
    Writer(outFile) << arch;
    LOG_MSG("Wrote ") << outFile.path();
}
