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
#include <de/textapp.h>
#include <de/arrayvalue.h>
#include <de/filesystem.h>
#include <de/fixedbytearray.h>
#include <de/logbuffer.h>
#include <de/nativefile.h>
#include <de/numbervalue.h>
#include <de/reader.h>
#include <de/writer.h>
#include <de/ziparchive.h>
#include "id1translator.h"
#include "savegametool.h"

extern de::Path composeMapUriPath(de::duint32 episode, de::duint32 map);
extern de::Folder &outputFolder();

using namespace de;

DE_PIMPL(Id1Translator)
{
    FormatId id;
    const File *saveFilePtr;
    dint32 saveVersion;

    Impl(Public *i)
        : Base(i)
        , id         (DoomV9)
        , saveFilePtr(0)
        , saveVersion(0)
    {}

    ~Impl()
    {
        closeFile();
    }

    /**
     * Returns the native "magic" identifier, used for format recognition.
     */
    dint32 magic() const
    {
        switch (id)
        {
        case DoomV9:     return 0x1DEAD600;
        case HereticV13: return 0x7D9A1200;
        }
        DE_ASSERT_FAIL("Id1Translator::magic: Invalid format id");
        return 0;
    }

    bool knownFormatVersion(int verId) const
    {
        switch (id)
        {
        case DoomV9:     return verId == 90;
        case HereticV13: return verId == 130;
        }
        DE_ASSERT_FAIL("Id1Translator::knownFormatVersion: Invalid format id");
        return false;
    }

    const File *saveFile() const
    {
        DE_ASSERT(saveFilePtr != 0);
        return saveFilePtr;
    }

    void openFile(Path path)
    {
        LOG_TRACE("openFile: Opening \"%s\"", path);
        DE_ASSERT(saveFilePtr == 0);
        try
        {
            saveFilePtr = &DE_TEXT_APP->fileSystem().find<File const>(path);
            return;
        }
        catch (...)
        {}
        closeFile();
        throw FileOpenError("Id1Translator", "Failed opening \"" + path + "\"");
    }

    void closeFile()
    {
        if (saveFilePtr)
        {
            saveFilePtr = 0;
        }
    }

    Block *bufferFile(Reader &from) const
    {
        const IByteArray &source = *from.source();
        return new Block(source, from.offset(), source.size() - from.offset());
    }

    void translateMetadata(GameStateMetadata &metadata, Reader &from)
    {
#define SM_NOTHINGS     -1
#define SM_BABY         0
#define NUM_SKILL_MODES 5
#define MAXPLAYERS      16

        Block desc;
        from.readBytes(24, desc);
        metadata.set("userDescription", desc.constData());

        Block vcheck;
        from.readBytes(16, vcheck);
        saveVersion = String(vcheck.c_str() + 8).toInt(0, 10, String::AllowSuffix);
        DE_ASSERT(knownFormatVersion(saveVersion));

        // Id Tech 1 formats omitted the majority of the game rules...
        std::unique_ptr<Record> rules(new Record);
        dbyte skill;
        from >> skill;
        // Interpret skill levels outside the normal range as "spawn no things".
        if (skill >= NUM_SKILL_MODES)
        {
            skill = SM_NOTHINGS;
        }
        rules->set("skill", skill);
        metadata.add("gameRules", rules.release());

        uint episode, map;
        from.readAs<dchar>(episode);
        from.readAs<dchar>(map);
        DE_ASSERT(map > 0);
        metadata.set("mapUri", composeMapUriPath(episode, map - 1).asText());

        ArrayValue *array = new ArrayValue;
        int idx = 0;
        while (idx++ < 4)
        {
            dbyte playerPresent;
            from >> playerPresent;
            *array << NumberValue(playerPresent != 0, NumberValue::Boolean);
        }
        while (idx++ <= MAXPLAYERS)
        {
            *array << NumberValue(false, de::NumberValue::Boolean);
        }
        metadata.set("players", array);

        // Get the map time.
        int a, b, c;
        from.readAs<dchar>(a);
        from.readAs<dchar>(b);
        from.readAs<dchar>(c);
        metadata.set("mapTime", ((a << 16) + (b << 8) + c));

        if (fallbackGameId.isEmpty())
        {
            /// @throw Error Game identity key could not be determined unambiguously.
            throw AmbigousGameIdError("translateMetadata", "Game identity key is ambiguous");
        }
        metadata.set("gameIdentityKey",     fallbackGameId);

        metadata.set("sessionId",           0);

#undef MAXPLAYERS
#undef NUM_SKILL_MODES
#undef SM_BABY
#undef SM_NOTHINGS
    }
};

Id1Translator::Id1Translator(FormatId id, StringList knownExtensions, StringList baseGameIdKeys)
    : PackageFormatter(std::move(knownExtensions), std::move(baseGameIdKeys))
    , d(new Impl(this))
{
    d->id = id;
}

Id1Translator::~Id1Translator()
{}

String Id1Translator::formatName() const
{
    switch (d->id)
    {
    case DoomV9:      return "Doom (id Tech 1)";
    case HereticV13:  return "Heretic (id Tech 1)";
    }
    DE_ASSERT_FAIL("Id1Translator::formatName: Invalid format id");
    return "";
}

bool Id1Translator::recognize(Path path)
{
    LOG_AS("Id1Translator");

    bool recognized = false;
    try
    {
        d->openFile(path);
        Reader from(*d->saveFile());
        // We'll use the version number string for recognition purposes.
        // Seek past the user description.
        from.seek(24);
        Block vcheck;
        from.readBytes(16, vcheck);
        if (vcheck.beginsWith("version "))
        {
            // The version id can be used to determine which game format the save is in.
            int verId = String(vcheck.c_str() + 8).toInt(0, 10, String::AllowSuffix);
            if (d->knownFormatVersion(verId))
            {
                recognized = true;
            }
        }
    }
    catch (...)
    {}
    d->closeFile();
    return recognized;
}

void Id1Translator::convert(Path path)
{
    LOG_AS("Id1Translator");

    /// @todo try all known extensions at the given path, if not specified.
    String saveName = path.lastSegment().toLowercaseString();

    d->openFile(path);
    const String nativeFilePath = d->saveFile()->source()->as<NativeFile>().nativePath();
    std::unique_ptr<Reader> from(new Reader(*d->saveFile()));

    // Read and translate the game session metadata.
    GameStateMetadata metadata;
    d->translateMetadata(metadata, *from);

    ZipArchive arch;
    arch.add("Info", composeInfo(metadata, nativeFilePath, d->saveVersion).toUtf8());

    // The only serialized map state follows the session metadata in the game state file.
    // Buffer the rest of the file and write it out to a new map state file.
    if (Block *xlatedData = d->bufferFile(*from))
    {
        // Append the remaining translated data to header, forming the new serialized
        // map state data file.
        Block *mapStateData = composeMapStateHeader(d->magic(), 14);
        *mapStateData += *xlatedData;
        delete xlatedData;

        arch.add(Path("maps") / metadata.gets("mapUri") + "State", *mapStateData);
        delete mapStateData;
    }

    d->closeFile();

    File &outFile = outputFolder().replaceFile(saveName.fileNameWithoutExtension() + ".save");
    Writer(outFile) << arch;
    outFile.flush();
    LOG_MSG("Wrote ") << outFile.as<NativeFile>().nativePath().pretty();
}
