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

#include "lzss.h"
#include <QList>
#include <QMutableListIterator>
#include <de/TextApp>
#include <de/ArrayValue>
#include <de/NativeFile>
#include <de/NumberValue>
#include <de/Writer>
#include <de/ZipArchive>
#include "nativetranslator.h"

extern de::String fallbackGameId;
extern de::Path composeMapUriPath(de::duint32 episode, de::duint32 map);

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
        DENG2_ASSERT(_file != 0);
    }

    void seek(uint offset)
    {
        lzSeek(&file(), offset);
    }

    void read(de::dint8 *data, int len)
    {
        lzRead(data, len, &file());
    }

    LZReader &LZReader::operator >> (char &byte)
    {
        *this >> reinterpret_cast<de::duchar &>(byte);
        return *this;
    }

    LZReader &LZReader::operator >> (de::dchar &byte)
    {
        *this >> reinterpret_cast<de::duchar &>(byte);
        return *this;
    }

    LZReader &LZReader::operator >> (de::duchar &byte)
    {
        byte = lzGetC(&file());
        return *this;
    }

    LZReader &LZReader::operator >> (de::dint16 &word)
    {
        *this >> reinterpret_cast<de::duint16 &>(word);
        return *this;
    }

    LZReader &LZReader::operator >> (de::duint16 &word)
    {
        word = lzGetW(&file());
        return *this;
    }

    LZReader &LZReader::operator >> (de::dint32 &dword)
    {
        *this >> reinterpret_cast<de::duint32 &>(dword);
        return *this;
    }

    LZReader &LZReader::operator >> (de::duint32 &dword)
    {
        dword = lzGetL(&file());
        return *this;
    }

    LZReader &LZReader::operator >> (de::dfloat &value)
    {
        *this >> *reinterpret_cast<de::duint32 *>(&value);
        return *this;
    }

private:
    LZFILE &file() const
    {
        DENG2_ASSERT(_file != 0);
        return *_file;
    }
};

} // namespace internal

using namespace internal;
using namespace de;

DENG2_PIMPL(NativeTranslator)
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

    Instance(Public *i)
        : Base(i)
        , id         (Doom)
        , saveVersion(0)
        , saveFilePtr(0)
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
        case Doom:    return 0x1DEAD666;
        case Heretic: return 0x7D9A12C5;
        case Hexen:   return 0x1B17CC00;
        }
        DENG2_ASSERT(!"NativeTranslator::magic: Invalid format id");
        return 0;
    }

    LZFILE *saveFile()
    {
        DENG2_ASSERT(saveFilePtr != 0);
        return saveFilePtr;
    }

    LZFILE const *saveFile() const
    {
        DENG2_ASSERT(saveFilePtr != 0);
        return saveFilePtr;
    }

    String translateGamemode(int gamemode)
    {
        static char const *doomGameIdentityKeys[] = {
            /*doom_shareware*/    "doom1-share",
            /*doom*/              "doom1",
            /*doom_ultimate*/     "doom1-ultimate",
            /*doom_chex*/         "chex",
            /*doom2*/             "doom2",
            /*doom2_plut*/        "doom2-plut",
            /*doom2_tnt*/         "doom2-tnt",
            /*doom2_hacx*/        "hacx"
        };
        static char const *hereticGameIdentityKeys[] = {
            /*heretic_shareware*/ "heretic-share",
            /*heretic*/           "heretic",
            /*heretic_extended*/  "heretic-ext"
        };
        static char const *hexenGameIdentityKeys[] = {
            /*hexen_demo*/        "hexen-demo",
            /*hexen*/             "hexen",
            /*hexen_deathkings*/  "hexen-dk",
            /*hexen_betademo*/    "hexen-betademo",
            /*hexen_v10*/         "hexen-v10"
        };

        // The gamemode may first need to be translated if "really old".
        if(id == Doom && saveVersion < 9)
        {
            static int const oldGamemodes[] = {
                /*doom_shareware*/      0,
                /*doom*/                1,
                /*doom2*/               4,
                /*doom_ultimate*/       2
            };
            DENG2_ASSERT(gamemode >= 0 && (unsigned)gamemode < sizeof(oldGamemodes) / sizeof(oldGamemodes[0]));
            gamemode = oldGamemodes[gamemode];

            // Older versions did not differentiate between versions of Doom2, meaning we cannot
            // determine the game identity key unambigously, without some assistance.
            if(gamemode == 4 /*doom2*/)
            {
                if(fallbackGameId.isEmpty())
                {
                    return fallbackGameId;
                }
                /// @throw Error Game identity key could not be determined unambiguously.
                throw AmbigousGameIdError("translateGamemode", "Game identity key is ambiguous");
            }
        }
        if(id == Heretic && saveVersion < 8)
        {
            static int const oldGamemodes[] = {
                /*heretic_shareware*/   0,
                /*heretic*/             1,
                /*heretic_extended*/    2
            };
            DENG2_ASSERT(gamemode >= 0 && (unsigned)gamemode < sizeof(oldGamemodes) / sizeof(oldGamemodes[0]));
            gamemode = oldGamemodes[gamemode];
        }

        switch(id)
        {
        case Doom:
            DENG2_ASSERT(gamemode >= 0 && (unsigned)gamemode < sizeof(doomGameIdentityKeys)    / sizeof(doomGameIdentityKeys[0]));
            return doomGameIdentityKeys[gamemode];

        case Heretic:
            DENG2_ASSERT(gamemode >= 0 && (unsigned)gamemode < sizeof(hereticGameIdentityKeys) / sizeof(hereticGameIdentityKeys[0]));
            return hereticGameIdentityKeys[gamemode];

        case Hexen:
            DENG2_ASSERT(gamemode >= 0 && (unsigned)gamemode < sizeof(hexenGameIdentityKeys)   / sizeof(hexenGameIdentityKeys[0]));
            return hexenGameIdentityKeys[gamemode];
        }
        DENG2_ASSERT(false);
        return "";
    }

    void openFile(Path path)
    {
        LOG_TRACE("openFile: Opening \"%s\"") << path;
        NativeFile const &nativeFile = DENG2_TEXT_APP->fileSystem().find<NativeFile const>(path);
        DENG2_ASSERT(saveFilePtr == 0);
        NativePath const nativeFilePath = nativeFile.nativePath();
        saveFilePtr = lzOpen(nativeFilePath.toUtf8().constData(), "rp");
        if(!saveFilePtr)
        {
            throw FileOpenError("NativeTranslator", "LZSS module failed to open \"" + nativeFilePath.pretty() + "\"");
        }
    }

    void closeFile()
    {
        if(saveFilePtr)
        {
            lzClose(saveFilePtr);
            saveFilePtr = 0;
        }
    }

    LZReader *newReader() const
    {
        return new LZReader(const_cast<Instance *>(this)->saveFile());
    }

    Block *bufferFile() const
    {
#define BLOCKSIZE 1024  // Read in 1kb blocks.

        Block *buffer = 0;
        dint8 readBuf[BLOCKSIZE];

        while(!lzEOF(saveFile()))
        {
            dsize bytesRead = lzRead(readBuf, BLOCKSIZE, const_cast<Instance *>(this)->saveFile());
            if(!buffer)
            {
                buffer = new Block;
            }
            buffer->append((char *)readBuf, bytesRead);
        }

        return buffer;

#undef BLOCKSIZE
    }

    /**
     * Supports native formats up to and including version 13.
     */
    void translateMetadata(SessionMetadata &metadata, LZReader &from)
    {
#define SM_NOTHINGS     -1
#define SM_BABY         0
#define NUM_SKILL_MODES 5
#define MAXPLAYERS      16

        dint32 oldMagic;
        from >> oldMagic >> saveVersion;

        DENG2_ASSERT(saveVersion >= 0 && saveVersion <= 13);
        if(saveVersion > 13)
        {
            /// @throw UnknownFormatError Format is from the future.
            throw UnknownFormatError("translateMetadata", "Incompatible format version " + String::number(saveVersion));
        }
        // We are incompatible with v3 saves due to an invalid test used to determine present
        // sides (ver3 format's sides contain chunks of junk data).
        if(id == Hexen && saveVersion == 3)
        {
            /// @throw UnknownFormatError Map state is in an unsupported format.
            throw UnknownFormatError("translateMetadata", "Unsupported format version " + String::number(saveVersion));
        }
        metadata.set("version",             dint(14));

        // Translate gamemode identifiers from older save versions.
        dint32 oldGamemode;
        from >> oldGamemode;
        metadata.set("gameIdentityKey",     translateGamemode(oldGamemode));

        // User description. A fixed 24 characters in length in "really old" versions.
        dint32 len = 24;
        if(saveVersion >= 10)
        {
            from >> len;
        }
        dint8 *descBuf = (dint8 *)malloc(len + 1);
        DENG2_ASSERT(descBuf != 0);
        from.read(descBuf, len);
        metadata.set("userDescription",     String((char *)descBuf, len));
        free(descBuf); descBuf = 0;

        QScopedPointer<Record> rules(new Record);
        if(id != Hexen && saveVersion < 13)
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
            if(skill < SM_BABY || skill >= NUM_SKILL_MODES)
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
            if(skill < SM_BABY || skill >= NUM_SKILL_MODES)
            {
                skill = SM_NOTHINGS;
            }
            rules->set("skill", skill);
        }

        dbyte episode, map;
        from >> episode >> map;
        DENG2_ASSERT(map > 0);
        metadata.set("mapUri",              composeMapUriPath(episode, map - 1).asText());

        dbyte deathmatch;
        from >> deathmatch;
        rules->set("deathmatch", deathmatch);

        if(id != Hexen && saveVersion == 13)
        {
            dbyte fast;
            from >> fast;
            rules->set("fast", fast);
        }

        dbyte noMonsters;
        from >> noMonsters;
        rules->set("noMonsters", noMonsters);

        if(id == Hexen)
        {
            dbyte randomClasses;
            from >> randomClasses;
            rules->set("randomClasses", randomClasses);
        }
        else
        {
            dbyte respawnMonsters;
            from >> respawnMonsters;
            rules->set("respawnMonsters", respawnMonsters);
        }

        metadata.add("gameRules",           rules.take());

        if(id != Hexen)
        {
            /*skip junk*/ if(saveVersion < 10) from.seek(2);

            dint32 mapTime;
            from >> mapTime;
            metadata.set("mapTime",         mapTime);

            ArrayValue *array = new de::ArrayValue;
            for(int i = 0; i < MAXPLAYERS; ++i)
            {
                dbyte playerPresent;
                from >> playerPresent;
                *array << NumberValue(playerPresent != 0, NumberValue::Boolean);
            }
            metadata.set("players",         array);
        }

        dint32 sessionId;
        from >> sessionId;
        metadata.set("sessionId",           sessionId);

#undef MAXPLAYERS
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

        static ACScriptTask *ACScriptTask::fromReader(LZReader &reader)
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
            DENG2_ASSERT(mapNumber);

            to << composeMapUriPath(0, mapNumber).asText()
               << scriptNumber;

            // Script arguments:
            for(int i = 0; i < 4; ++i)
            {
                to << args[i];
            }
        }
    };
    typedef QList<ACScriptTask *> ACScriptTasks;

    void translateACScriptState(LZReader &from, Writer &writer)
    {
#define MAX_ACS_WORLD_VARS 64

        LOG_AS("NativeTranslator");
        if(saveVersion >= 7)
        {
            dint32 segId;
            from >> segId;
            if(segId != ASEG_WORLDSCRIPTDATA)
            {
                /// @throw ReadError Failed alignment check.
                throw ReadError("translateACScriptState", "Corrupt save game, segment #" + String::number(ASEG_WORLDSCRIPTDATA) + " failed alignment check");
            }
        }

        dbyte ver = 1;
        if(saveVersion >= 7)
        {
            from >> ver;
        }
        if(ver < 1 || ver > 3)
        {
            /// @throw UnknownFormatError Format is from the future.
            throw UnknownFormatError("translateACScriptState", "Incompatible data segment version " + String::number(ver));
        }

        dint32 worldVars[MAX_ACS_WORLD_VARS];
        for(int i = 0; i < MAX_ACS_WORLD_VARS; ++i)
        {
            from >> worldVars[i];
        }

        // Read the deferred tasks into a temporary buffer for translation.
        dint32 oldStoreSize;
        from >> oldStoreSize;
        ACScriptTasks tasks;

        if(oldStoreSize > 0)
        {
            tasks.reserve(oldStoreSize);
            for(int i = 0; i < oldStoreSize; ++i)
            {
                tasks.append(ACScriptTask::fromReader(from));
            }

            // Prune tasks with no map number set (unused).
            QMutableListIterator<ACScriptTask *> it(tasks);
            while(it.hasNext())
            {
                ACScriptTask *task = it.next();
                if(!task->mapNumber)
                {
                    it.remove();
                    delete task;
                }
            }
            LOG_XVERBOSE("Translated %i deferred ACScript tasks") << tasks.count();
        }

        /* skip junk */ if(saveVersion < 7) from.seek(12);

        // Write out the translated data:
        writer << dbyte(4); // segment version
        for(int i = 0; i < MAX_ACS_WORLD_VARS; ++i)
        {
            writer << worldVars[i];
        }
        writer << dint32(tasks.count());
        writer.writeObjects(tasks);
        qDeleteAll(tasks);

#undef MAX_ACS_WORLD_VARS
    }
};

NativeTranslator::NativeTranslator(FormatId formatId, QStringList knownExtensions,
                                   QStringList baseGameIdKeys)
    : PackageFormatter(knownExtensions, baseGameIdKeys)
    , d(new Instance(this))
{
    d->id = formatId;
}

NativeTranslator::~NativeTranslator()
{}

String NativeTranslator::formatName() const
{
    switch(d->id)
    {
    case Doom:      return "Doom";
    case Heretic:   return "Heretic";
    case Hexen:     return "Hexen";
    }
    DENG2_ASSERT(!"NativeTranslator::formatName: Invalid format id");
    return "";
}

bool NativeTranslator::recognize(Path path)
{
    LOG_AS("NativeTranslator");

    bool recognized = false;
    try
    {
        d->openFile(path);
        LZReader *from = d->newReader();
        // Native save formats can be recognized by their "magic" byte identifier.
        dint32 oldMagic;
        *from >> oldMagic;
        recognized = (oldMagic == d->magic());
        delete from;
    }
    catch(...)
    {}
    d->closeFile();
    return recognized;
}

void NativeTranslator::convert(Path path)
{
    LOG_AS("NativeTranslator");

    /// @todo try all known extensions at the given path, if not specified.
    String saveName = path.lastSegment().toString();

    d->openFile(path);
    String const nativeFilePath = DENG2_TEXT_APP->fileSystem().find<NativeFile const>(path).nativePath().toString();
    LZReader *from = d->newReader();

    // Read and translate the game session metadata.
    SessionMetadata metadata;
    d->translateMetadata(metadata, *from);

    ZipArchive arch;
    arch.add("Info", composeInfo(metadata, nativeFilePath, d->saveVersion).toUtf8());

    if(d->id == Hexen)
    {
        // Translate and separate the serialized world ACS data into a new file.
        Block worldACScriptData;
        Writer writer(worldACScriptData);
        d->translateACScriptState(*from, writer);
        arch.add("ACScriptState", worldACScriptData);
    }

    if(d->id == Hexen)
    {
        // Serialized map states are in similarly named "side car" files.
        int const maxHubMaps = 99;
        for(int i = 0; i < maxHubMaps; ++i)
        {
            // Open the map state file.
            Path mapStatePath =
                    path.toString().fileNamePath() / saveName.fileNameWithoutExtension()
                        + String("%1").arg(i + 1, 2, 10, QChar('0'))
                        + saveName.fileNameExtension();

            d->closeFile();
            try
            {
                d->openFile(mapStatePath);
                // Buffer the file and write it out to a new map state file.
                if(Block *xlatedData = d->bufferFile())
                {
                    // Append the remaining translated data to header, forming the new serialized
                    // map state data file.
                    Block *mapStateData = composeMapStateHeader(d->magic(), d->saveVersion);
                    *mapStateData += *xlatedData;
                    delete xlatedData;

                    arch.add(Path("maps") / composeMapUriPath(0, i) + "State", *mapStateData);
                    delete mapStateData;
                }
            }
            catch(FileOpenError const &)
            {} // Ignore this error.
        }
    }
    else
    {
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
    }

    delete from;
    d->closeFile();

    File &outFile = DENG2_TEXT_APP->homeFolder().replaceFile(saveName.fileNameWithoutExtension() + ".save");
    outFile.setMode(File::Write | File::Truncate);
    Writer(outFile) << arch;
    LOG_MSG("Wrote ") << outFile.path();
}
