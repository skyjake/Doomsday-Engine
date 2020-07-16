/** @file nativetranslator.cpp  Savegame tool.
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

#include <de/list.h>
#include <de/arrayvalue.h>
#include <de/filesystem.h>
#include <de/logbuffer.h>
#include <de/nativefile.h>
#include <de/numbervalue.h>
#include <de/reader.h>
#include <de/textapp.h>
#include <de/writer.h>
#include <de/ziparchive.h>
#include "lzss.h"
#include "nativetranslator.h"
#include "savegametool.h"

extern de::Path composeMapUriPath(de::duint32 episode, de::duint32 map);
extern de::Folder &outputFolder();

namespace internal {

/**
 * Reader for the old native save format, which is compressed with LZSS.
 */
class LZReader
{
public:
    LZFILE *_file;
    explicit LZReader(LZFILE *file = 0)
        : _file(const_cast<LZFILE *>(file))
    {
        DE_ASSERT(_file != 0);
    }

    void seek(uint offset)
    {
        lzSeek(&file(), offset);
    }

    void read(de::dint8 *data, int len)
    {
        lzRead(data, len, &file());
    }

    LZReader &operator >> (char &byte)
    {
        *this >> reinterpret_cast<de::duchar &>(byte);
        return *this;
    }

    LZReader &operator >> (de::dchar &byte)
    {
        *this >> reinterpret_cast<de::duchar &>(byte);
        return *this;
    }

    LZReader &operator >> (de::duchar &byte)
    {
        byte = lzGetC(&file());
        return *this;
    }

    LZReader &operator >> (de::dint16 &word)
    {
        *this >> reinterpret_cast<de::duint16 &>(word);
        return *this;
    }

    LZReader &operator >> (de::duint16 &word)
    {
        word = lzGetW(&file());
        return *this;
    }

    LZReader &operator >> (de::dint32 &dword)
    {
        *this >> reinterpret_cast<de::duint32 &>(dword);
        return *this;
    }

    LZReader &operator >> (de::duint32 &dword)
    {
        dword = lzGetL(&file());
        return *this;
    }

    LZReader &operator >> (float &value)
    {
        *this >> *reinterpret_cast<de::duint32 *>(&value);
        return *this;
    }

private:
    LZFILE &file() const
    {
        DE_ASSERT(_file != 0);
        return *_file;
    }
};

} // namespace internal

using namespace internal;
using namespace de;

DE_PIMPL(NativeTranslator)
{
    typedef enum savestatesegment_e {
        ASEG_MAP_HEADER = 102,  // Hexen only
        ASEG_MAP_ELEMENTS,
        ASEG_POLYOBJS,          // Hexen only
        ASEG_MOBJS,             // Hexen < ver 4 only
        ASEG_THINKERS,
        ASEG_SCRIPTS,           // Hexen only
        ASEG_PLAYERS,
        ASEG_SOUNDS,            // Hexen only
        ASEG_MISC,              // Hexen only
        ASEG_END,               // = 111
        ASEG_MATERIAL_ARCHIVE,
        ASEG_MAP_HEADER2,
        ASEG_PLAYER_HEADER,
        ASEG_WORLDSCRIPTDATA   // Hexen only
    } savestatesegment_t;

    FormatId id;
    dint32 saveVersion;
    LZFILE *saveFilePtr;

    Impl(Public *i)
        : Base(i)
        , id         (Doom)
        , saveVersion(0)
        , saveFilePtr(0)
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
        case Doom:    return 0x1DEAD666;
        case Heretic: return 0x7D9A12C5;
        case Hexen:   return 0x1B17CC00;
        }
        DE_ASSERT_FAIL("NativeTranslator::magic: Invalid format id");
        return 0;
    }

    LZFILE *saveFile()
    {
        DE_ASSERT(saveFilePtr != 0);
        return saveFilePtr;
    }

    const LZFILE *saveFile() const
    {
        DE_ASSERT(saveFilePtr != 0);
        return saveFilePtr;
    }

    String translateGamemode(int gamemode)
    {
        static const char *doomGameIdentityKeys[] = {
            /*doom_shareware*/    "doom1-share",
            /*doom*/              "doom1",
            /*doom_ultimate*/     "doom1-ultimate",
            /*doom_chex*/         "chex",
            /*doom2*/             "doom2",
            /*doom2_plut*/        "doom2-plut",
            /*doom2_tnt*/         "doom2-tnt",
            /*doom2_hacx*/        "hacx"
        };
        static const char *hereticGameIdentityKeys[] = {
            /*heretic_shareware*/ "heretic-share",
            /*heretic*/           "heretic",
            /*heretic_extended*/  "heretic-ext"
        };
        static const char *hexenGameIdentityKeys[] = {
            /*hexen_demo*/        "hexen-demo",
            /*hexen*/             "hexen",
            /*hexen_deathkings*/  "hexen-dk",
            /*hexen_betademo*/    "hexen-betademo",
            /*hexen_v10*/         "hexen-v10"
        };

        // The gamemode may first need to be translated if "really old".
        if (id == Doom && saveVersion < 9)
        {
            static int const oldGamemodes[] = {
                /*doom_shareware*/      0,
                /*doom*/                1,
                /*doom2*/               4,
                /*doom_ultimate*/       2
            };
            DE_ASSERT(gamemode >= 0 && (unsigned)gamemode < sizeof(oldGamemodes) / sizeof(oldGamemodes[0]));
            gamemode = oldGamemodes[gamemode];

            // Older versions did not differentiate between versions of Doom2, meaning we cannot
            // determine the game ID unambigously, without some assistance.
            if (gamemode == 4 /*doom2*/)
            {
                if (!fallbackGameId.isEmpty())
                {
                    return fallbackGameId;
                }
                /// @throw Error Game ID could not be determined unambiguously.
                throw AmbigousGameIdError("translateGamemode", "Game ID is ambiguous");
            }
        }
        if (id == Heretic && saveVersion < 8)
        {
            static int const oldGamemodes[] = {
                /*heretic_shareware*/   0,
                /*heretic*/             1,
                /*heretic_extended*/    2
            };
            DE_ASSERT(gamemode >= 0 && (unsigned)gamemode < sizeof(oldGamemodes) / sizeof(oldGamemodes[0]));
            gamemode = oldGamemodes[gamemode];
        }

        switch (id)
        {
        case Doom:
            DE_ASSERT(gamemode >= 0 && (unsigned)gamemode < sizeof(doomGameIdentityKeys)    / sizeof(doomGameIdentityKeys[0]));
            return doomGameIdentityKeys[gamemode];

        case Heretic:
            DE_ASSERT(gamemode >= 0 && (unsigned)gamemode < sizeof(hereticGameIdentityKeys) / sizeof(hereticGameIdentityKeys[0]));
            return hereticGameIdentityKeys[gamemode];

        case Hexen:
            DE_ASSERT(gamemode >= 0 && (unsigned)gamemode < sizeof(hexenGameIdentityKeys)   / sizeof(hexenGameIdentityKeys[0]));
            return hexenGameIdentityKeys[gamemode];
        }
        DE_ASSERT(false);
        return "";
    }

    void openFile(Path path)
    {
        LOG_TRACE("openFile: Opening \"%s\"", path);
        DE_ASSERT(saveFilePtr == 0);
        try
        {
            const NativeFile &nativeFile = DE_TEXT_APP->fileSystem().find<NativeFile const>(path);
            const NativePath nativeFilePath = nativeFile.nativePath();
            saveFilePtr = lzOpen(nativeFilePath, "rp");
            return;
        }
        catch (const FileSystem::NotFoundError &)
        {} // We'll thow our own.

        throw FileOpenError("NativeTranslator", "Failed opening \"" + path + "\"");
    }

    void closeFile()
    {
        if (saveFilePtr)
        {
            lzClose(saveFilePtr);
            saveFilePtr = 0;
        }
    }

    Block *bufferFile()
    {
#define BLOCKSIZE 1024  // Read in 1kb blocks.

        Block *buffer = 0;
        dint8 readBuf[BLOCKSIZE];

        while (!lzEOF(saveFile()))
        {
            long bytesRead = lzRead(readBuf, BLOCKSIZE, saveFile());
            if (!buffer)
            {
                buffer = new Block;
            }
            buffer->append((char *)readBuf, int(bytesRead));
        }

        return buffer;

#undef BLOCKSIZE
    }

    /**
     * Supports native formats up to and including version 13.
     */
    void translateMetadata(GameStateMetadata &metadata, LZReader &from)
    {
#define SM_NOTHINGS     -1
#define SM_BABY         0
#define NUM_SKILL_MODES 5

        dint32 oldMagic;
        from >> oldMagic >> saveVersion;

        if (saveVersion < 0 || saveVersion > 13)
        {
            /// @throw UnknownFormatError Format is from the future.
            throw UnknownFormatError("translateMetadata", "Incompatible format version " + String::asText(saveVersion));
        }
        // We are incompatible with v3 saves due to an invalid test used to determine present
        // sides (ver3 format's sides contain chunks of junk data).
        if (id == Hexen && saveVersion == 3)
        {
            /// @throw UnknownFormatError Map state is in an unsupported format.
            throw UnknownFormatError("translateMetadata", "Unsupported format version " + String::asText(saveVersion));
        }

        // Translate gamemode identifiers from older save versions.
        dint32 oldGamemode;
        from >> oldGamemode;
        metadata.set("gameIdentityKey", translateGamemode(oldGamemode));

        // User description. A fixed 24 characters in length in "really old" versions.
        dint32 len = 24;
        if (saveVersion >= 10)
        {
            from >> len;
        }
        dint8 *descBuf = (dint8 *)malloc(len + 1);
        DE_ASSERT(descBuf != 0);
        from.read(descBuf, len);
        metadata.set("userDescription", String((char *)descBuf, len));
        free(descBuf); descBuf = 0;

        std::unique_ptr<Record> rules(new Record);
        if (id != Hexen && saveVersion < 13)
        {
            // In DOOM the high bit of the skill mode byte is also used for the
            // "fast" game rule dd_bool. There is more confusion in that SM_NOTHINGS
            // will result in 0xff and thus always set the fast bit.
            //
            // Here we decipher this assuming that if the skill mode is invalid then
            // by default this means "spawn no things" and if so then the "fast" game
            // rule is meaningless so it is forced off.
            dbyte skillModePlusFastBit;
            from >> skillModePlusFastBit;
            int skill = (skillModePlusFastBit & 0x7f);
            if (skill >= NUM_SKILL_MODES)
            {
                skill = SM_NOTHINGS;
                rules->addBoolean("fast", false);
            }
            else
            {
                rules->addBoolean("fast", (skillModePlusFastBit & 0x80) != 0);
            }
            rules->set("skill", skill);
        }
        else
        {
            dbyte skill;
            from >> skill;
            skill &= 0x7f;
            // Interpret skill levels outside the normal range as "spawn no things".
            if (skill >= NUM_SKILL_MODES)
            {
                skill = SM_NOTHINGS;
            }
            rules->set("skill", skill);
        }

        dbyte episode, map;
        from >> episode >> map;
        if (fallbackGameId.beginsWith("hexen") || fallbackGameId.beginsWith("doom2") ||
           /*fallbackGameId.beginsWith("chex") ||*/ fallbackGameId.beginsWith("hacx"))
        {
            episode = 0; // Why is this > 0??
        }
        metadata.set("mapUri", composeMapUriPath(episode, map).asText());

        dbyte deathmatch;
        from >> deathmatch;
        rules->set("deathmatch", deathmatch);

        if (id != Hexen && saveVersion == 13)
        {
            dbyte fast;
            from >> fast;
            rules->addBoolean("fast", fast);
        }

        dbyte noMonsters;
        from >> noMonsters;
        rules->addBoolean("noMonsters", noMonsters);

        if (id == Hexen)
        {
            dbyte randomClasses;
            from >> randomClasses;
            rules->addBoolean("randomClasses", randomClasses);
        }
        else
        {
            dbyte respawnMonsters;
            from >> respawnMonsters;
            rules->addBoolean("respawnMonsters", respawnMonsters);
        }

        metadata.add("gameRules", rules.release());

        if (id != Hexen)
        {
            /*skip junk*/ if (saveVersion < 10) from.seek(2);

            dint32 mapTime;
            from >> mapTime;
            metadata.set("mapTime", mapTime);

            ArrayValue *array = new de::ArrayValue;
            for (int i = 0; i < 16/*MAXPLAYERS*/; ++i)
            {
                dbyte playerPresent;
                from >> playerPresent;
                *array << NumberValue(playerPresent != 0, NumberValue::Boolean);
            }
            metadata.set("players", array);
        }

        dint32 sessionId;
        from >> sessionId;
        metadata.set("sessionId", sessionId);

#undef NUM_SKILL_MODES
#undef SM_BABY
#undef SM_NOTHINGS
    }

    /**
     * (Deferred) ACScript translator utility.
     */
    struct ACScriptTask : public IWritable
    {
        duint32 mapNumber;
        dint32 scriptNumber;
        dbyte args[4];

        static ACScriptTask *fromReader(LZReader &reader)
        {
            ACScriptTask *task = new ACScriptTask;
            task->read(reader);
            return task;
        }

        void read(LZReader &from)
        {
            from >> mapNumber >> scriptNumber;
            from.read((dint8 *)args, 4);
        }

        void operator >> (Writer &to) const
        {
            DE_ASSERT(mapNumber);

            to << composeMapUriPath(0, mapNumber).asText()
               << scriptNumber;

            // Script arguments:
            for (int i = 0; i < 4; ++i)
            {
                to << args[i];
            }
        }
    };
    typedef de::List<ACScriptTask *> ACScriptTasks;

    void translateACScriptState(ZipArchive &arch, LZReader &from)
    {
#define MAX_ACS_WORLD_VARS 64

        LOG_AS("NativeTranslator");
        if (saveVersion >= 7)
        {
            dint32 segId;
            from >> segId;
            if (segId != ASEG_WORLDSCRIPTDATA)
            {
                /// @throw ReadError Failed alignment check.
                throw ReadError("translateACScriptState", "Corrupt save game, segment #" + String::asText(ASEG_WORLDSCRIPTDATA) + " failed alignment check");
            }
        }

        dbyte ver = 1;
        if (saveVersion >= 7)
        {
            from >> ver;
        }
        if (ver < 1 || ver > 3)
        {
            /// @throw UnknownFormatError Format is from the future.
            throw UnknownFormatError("translateACScriptState", "Incompatible data segment version " + String::asText(ver));
        }

        dint32 worldVars[MAX_ACS_WORLD_VARS];
        for (int i = 0; i < MAX_ACS_WORLD_VARS; ++i)
        {
            from >> worldVars[i];
        }

        // Read the deferred tasks into a temporary buffer for translation.
        dint32 oldStoreSize;
        from >> oldStoreSize;
        ACScriptTasks tasks;

        if (oldStoreSize > 0)
        {
            tasks.reserve(oldStoreSize);
            for (int i = 0; i < oldStoreSize; ++i)
            {
                tasks.append(ACScriptTask::fromReader(from));
            }

            // Prune tasks with no map number set (unused).
            for (auto it = tasks.begin(); it != tasks.end(); )
            {
                ACScriptTask *task = *it;
                if (!task->mapNumber)
                {
                    it = tasks.erase(it);
                    delete task;
                }
                else
                {
                    ++it;
                }
            }
            LOG_XVERBOSE("Translated %i deferred ACScript tasks", tasks.count());
        }

        /* skip junk */ if (saveVersion < 7) from.seek(12);

        // Write out the translated data:
        Writer writer = Writer(arch.entryBlock("ACScriptState")).withHeader();
        for (int i = 0; i < MAX_ACS_WORLD_VARS; ++i)
        {
            writer << worldVars[i];
        }
        writer << dint32(tasks.count());
        writer.writeObjects(tasks);
        deleteAll(tasks);

#undef MAX_ACS_WORLD_VARS
    }
};

NativeTranslator::NativeTranslator(FormatId   formatId,
                                   StringList knownExtensions,
                                   StringList baseGameIdKeys)
    : PackageFormatter(std::move(knownExtensions), std::move(baseGameIdKeys))
    , d(new Impl(this))
{
    d->id = formatId;
}

NativeTranslator::~NativeTranslator()
{}

String NativeTranslator::formatName() const
{
    switch (d->id)
    {
    case Doom:      return "Doom";
    case Heretic:   return "Heretic";
    case Hexen:     return "Hexen";
    }
    DE_ASSERT_FAIL("NativeTranslator::formatName: Invalid format id");
    return "";
}

bool NativeTranslator::recognize(Path path)
{
    LOG_AS("NativeTranslator");

    bool recognized = false;
    try
    {
        d->openFile(path);
        LZReader from(d->saveFile());
        // Native save formats can be recognized by their "magic" byte identifier.
        dint32 oldMagic;
        from >> oldMagic;

        if (oldMagic == d->magic())
        {
            // Ensure this is a compatible save version.
            dint32 saveVersion;
            from >> saveVersion;
            if (saveVersion >= 0 && saveVersion <= 13)
            {
                // We are incompatible with v3 saves due to an invalid test used to determine present
                // sides (ver3 format's sides contain chunks of junk data).
                if (!(d->id == Hexen && saveVersion == 3))
                {
                    recognized = true;
                }
            }
        }
    }
    catch (...)
    {}
    d->closeFile();
    return recognized;
}

void NativeTranslator::convert(Path path)
{
    LOG_AS("NativeTranslator");

    /// @todo try all known extensions at the given path, if not specified.
    String saveName = path.lastSegment().toLowercaseString();

    d->openFile(path);
    const String nativeFilePath = DE_TEXT_APP->fileSystem().find<NativeFile const>(path).nativePath();
    LZReader from(d->saveFile());

    ZipArchive arch;

    // Read and translate the game session metadata.
    GameStateMetadata metadata;
    d->translateMetadata(metadata, from);

    if (d->id == Hexen)
    {
        // Translate and separate the serialized world ACS data into a new file.
        d->translateACScriptState(arch, from);
    }

    if (d->id == Hexen)
    {
        std::unique_ptr<Block> xlatedPlayerData(d->bufferFile());
        DE_ASSERT(xlatedPlayerData);
        d->closeFile();

        // Update the metadata with the present player info (this is not needed by Hexen
        // but we'll include it for the sake of uniformity).
        ArrayValue *array = new de::ArrayValue;
        const int presentPlayersOffset = (d->saveVersion < 4? 0 : 4 + 1 + (8 * 4) + 4);
        for (int i = 0; i < 8/*MAXPLAYERS*/; ++i)
        {
            dbyte playerPresent = xlatedPlayerData->at(presentPlayersOffset + i);
            *array << NumberValue(playerPresent != 0, NumberValue::Boolean);
        }
        metadata.set("players", array);

        // Serialized map states are in similarly named "side car" files.
        const int maxHubMaps = 99;
        for (int i = 0; i < maxHubMaps; ++i)
        {
            try
            {
                d->openFile(path.toString().fileNamePath() / saveName.fileNameWithoutExtension()
                            + Stringf("%02i", i + 1)
                            + saveName.fileNameExtension());

                if (Block *xlatedData = d->bufferFile())
                {
                    const String mapUriPath = composeMapUriPath(0, i + 1);

                    // If this is the "current" map extract the map time for the metadata.
                    if (!mapUriPath.compareWithoutCase(metadata.gets("mapUri")))
                    {
                        dint32 mapTime;
                        Reader(*xlatedData, littleEndianByteOrder, 4 + 1) >> mapTime;
                        metadata.set("mapTime", mapTime);
                    }

                    // Concatenate the translated data to form the serialized map state file.
                    std::unique_ptr<Block> mapStateData(composeMapStateHeader(d->magic(), d->saveVersion));
                    DE_ASSERT(mapStateData);
                    *mapStateData += *xlatedPlayerData;
                    *mapStateData += *xlatedData;

                    arch.add(Path("maps") / mapUriPath + "State", *mapStateData);

                    delete xlatedData;
                }
            }
            catch (const FileOpenError &)
            {} // Ignore this error.
            d->closeFile();
        }
    }
    else
    {
        // The only serialized map state follows the session metadata in the game state file.
        // Buffer the rest of the file and write it out to a new map state file.
        if (Block *xlatedData = d->bufferFile())
        {
            // Append the remaining translated data to header, forming the new serialized
            // map state data file.
            std::unique_ptr<Block> mapStateData(composeMapStateHeader(d->magic(), d->saveVersion));
            DE_ASSERT(mapStateData);
            *mapStateData += *xlatedData;

            arch.add(Path("maps") / metadata.gets("mapUri") + "State", *mapStateData);

            delete xlatedData;
        }

        d->closeFile();
    }

    // Write out the Info file with all the metadata.
    arch.add("Info", composeInfo(metadata, nativeFilePath, d->saveVersion).toUtf8());

    File &outFile = outputFolder().replaceFile(saveName.fileNameWithoutExtension() + ".save");
    Writer(outFile) << arch;
    outFile.flush();
    LOG_MSG("Wrote %s") << outFile.description();
}
