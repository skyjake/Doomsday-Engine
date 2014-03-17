/** @file main.cpp  Savegame tool.
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
#include "lzss.h"
#include <QDebug>
#include <QList>
#include <QMutableListIterator>
#include <QStringList>
#include <QtAlgorithms>
#include <de/TextApp>
#include <de/ArrayValue>
#include <de/game/savedsession.h>
#include <de/NumberValue>
#include <de/RecordValue>
#include <de/Time>
#include <de/Writer>
#include <de/ZipArchive>

using namespace de;
using de::game::SessionMetadata;

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

static LZFILE *saveFile;

enum SaveFormatId
{
    // Doomsday-native formats:
    Doom,
    Heretic,
    Hexen
};

/**
 * Base class for known save formats.
 */
class SaveFormat
{
public:
    /// An error occured when attempting to open the source file. @ingroup errors
    DENG2_ERROR(FileOpenError);

    /**
     * Base class for save format file readers.
     */
    class Reader
    {
    public:
        virtual ~Reader() {}

        virtual void seek(uint offset) = 0;
        virtual char readInt8() = 0;
        virtual short readInt16() = 0;
        virtual int readInt32() = 0;
        virtual float readFloat() = 0;
        virtual void read(char *data, int len) = 0;
    };

    SaveFormatId id;
    String textualId;
    int magic;
    QStringList knownExtensions;
    QStringList baseGameIdKeys;

public:
    /**
     * @param id               Unique identifier for the format.
     * @param textualId        Textual identifier for the format, used for log/error messages.
     * @param magic            Native "magic" idenifier, used for format recognition.
     * @param knownExtensions  List of known file extensions for the format.
     * @param baseGameIdKeys   List of supported base game identity keys for the format.
     */
    SaveFormat(SaveFormatId id, String textualId, int magic, QStringList knownExtensions,
               QStringList baseGameIdKeys)
        : id             (id)
        , textualId      (textualId)
        , magic          (magic)
        , knownExtensions(knownExtensions)
        , baseGameIdKeys (baseGameIdKeys)
    {}

    /**
     * Attempt to recognize the format of the given file.
     *
     * @param path  Path to the file to be recognized.
     */
    virtual bool recognize(Path path) = 0;

    /**
     * Try to open the given file.
     *
     * @param path  Path to the file to be opened.
     */
    virtual void openFile(Path path) = 0;

    /**
     * Close the already open file.
     */
    virtual void closeFile() = 0;

    /**
     * Instantiate a new reader for the already open file.
     */
    virtual Reader *newReader() const = 0;

    /**
     * Buffer the rest of the open file into a newly allocated memory buffer.
     *
     * @return  The read data buffer; otherwise @c 0 if at the end of the file.
     */
     virtual Block *bufferFile() const = 0;
};

/**
 * Specialized SaveFormat suitable for translating (old) Doomsday-native save formats.
 */
class NativeSaveFormat : public SaveFormat
{
public:
    /**
     * Reader for the old native save format, which is compressed with LZSS.
     */
    class Reader : public SaveFormat::Reader
    {
    public:
        void seek(uint offset)
        {
            lzSeek(saveFile, offset);
        }

        char readInt8()
        {
            return lzGetC(saveFile);
        }

        short readInt16()
        {
            return lzGetW(saveFile);
        }

        int readInt32()
        {
            return lzGetL(saveFile);
        }

        float readFloat()
        {
            DENG2_ASSERT(sizeof(float) == 4);
            int32_t val = lzGetL(saveFile);
            float returnValue = 0;
            std::memcpy(&returnValue, &val, 4);
            return returnValue;
        }

        void read(char *data, int len)
        {
            lzRead(data, len, saveFile);
        }
    };

public:
    NativeSaveFormat(SaveFormatId id, String textualId, int magic, QStringList knownExtensions,
                     QStringList baseGameIdKeys)
        : SaveFormat(id, textualId, magic, knownExtensions, baseGameIdKeys)
    {}
    virtual ~NativeSaveFormat() {}

    bool recognize(Path path)
    {
        bool result = false;
        try
        {
            openFile(path);
            Reader *reader = newReader();
            // Native save formats can be recognized by their "magic" byte identifier.
            result = (reader->readInt32() == magic);
            delete reader;
        }
        catch(...)
        {}
        closeFile();
        return result;
    }

    void openFile(Path path)
    {
        LOG_TRACE("NativeSaveFormat::openFile: Opening \"%s\"") << NativePath(path).pretty();
        DENG2_ASSERT(saveFile == 0);
        saveFile = lzOpen(NativePath(path).expand().toUtf8().constData(), "rp");
        if(!saveFile)
        {
            throw FileOpenError("NativeSaveFormat", "Failed opening \"" + NativePath(path).pretty() + "\"");
        }
    }

    void closeFile()
    {
        if(saveFile)
        {
            lzClose(saveFile);
            saveFile = 0;
        }
    }

    Reader *newReader() const
    {
        return new Reader;
    }

    Block *bufferFile() const
    {
#define BLOCKSIZE 1024  // Read in 1kb blocks.

        DENG2_ASSERT(saveFile != 0);

        Block *buffer = 0;
        dint8 readBuf[BLOCKSIZE];

        while(!lzEOF(saveFile))
        {
            dsize bytesRead = lzRead(readBuf, BLOCKSIZE, saveFile);
            if(!buffer)
            {
                buffer = new Block;
            }
            buffer->append((char *)readBuf, bytesRead);
        }

        return buffer;

#undef BLOCKSIZE
    }
};

typedef QList<SaveFormat *> SaveFormats;

static SaveFormats saveFormats;

static void initSaveFormats()
{
    // Add Doomsday-native formats:
    saveFormats << new NativeSaveFormat(Doom,    "Doom",    0x1DEAD666, QStringList(".dsg"), QStringList() << "doom" << "hacx" << "chex");
    saveFormats << new NativeSaveFormat(Heretic, "Heretic", 0x7D9A12C5, QStringList(".hsg"), QStringList() << "heretic");
    saveFormats << new NativeSaveFormat(Hexen,   "Hexen",   0x1B17CC00, QStringList(".hxs"), QStringList() << "hexen");
}

static SaveFormat *saveFormatForGameIdentityKey(String const &idKey)
{
    foreach(SaveFormat *fmt, saveFormats)
    foreach(QString const &baseIdentityKey, fmt->baseGameIdKeys)
    {
        if(idKey.beginsWith(baseIdentityKey)) return fmt;
    }
    return 0; // Not found.
}

static SaveFormat *guessSaveFormatFromFileName(Path const &path)
{
    String ext = path.lastSegment().toString().fileNameExtension().toLower();
    foreach(SaveFormat *fmt, saveFormats)
    foreach(QString const &knownExtension, fmt->knownExtensions)
    {
        if(!knownExtension.compare(ext, Qt::CaseInsensitive)) return fmt;
    }
    return 0; // Not found.
}

static String fallbackGameId;

static SaveFormat *knownSaveFormat;
static int saveVersion;

/// Returns the current, known save format.
static SaveFormat &saveFormat()
{
    if(knownSaveFormat)
    {
        return *knownSaveFormat;
    }
    throw Error("saveFormat", "Current save format is unknown");
}

static String versionText()
{
    return String("%1 version %2 (%3)")
               .arg(DENG2_TEXT_APP->applicationName())
               .arg(DENG2_TEXT_APP->applicationVersion())
               .arg(Time::fromText(__DATE__ " " __TIME__, Time::CompilerDateTime)
                    .asDateTime().toString(Qt::SystemLocaleShortDate));
}

static void printUsage()
{
    LOG_INFO("Usage: %s [options] savegame-path ...\n"
             "Options:\n"
             "--help, -h, -?  Show usage information."
             "-idKey  Fallback game identity key. Used to resolve ambigous savegame formats.")
            << DENG2_TEXT_APP->commandLine().at(0);
}

static void printDescription()
{
    LOG_VERBOSE("%s is a utility for converting legacy Doomsday Engine, Doom and Heretic savegame"
                " files into a format recognized by Doomsday Engine version 1.14 (or newer).")
            << DENG2_TEXT_APP->applicationName();
}

static void setFallbackGameIdentityKey(String const &idKey)
{
    static char const *knownIdKeys[] = {
        "chex",
        "doom1-ultimate",
        "doom1-share"
        "doom1",
        "doom2-plut",
        "doom2-tnt",
        "doom2",
        "hacx",
        "heretic-ext",
        "heretic-share",
        "heretic",
        "hexen-betademo",
        "hexen-demo",
        "hexen-dk",
        "hexen-v10"
        "hexen",
    };
    int const knownIdKeyCount = sizeof(knownIdKeys) / sizeof(knownIdKeys[0]);

    // Sanity check expected keys.
    int i = 0;
    for(; i < knownIdKeyCount; ++i)
    {
        if(!idKey.compare(knownIdKeys[i]))
        {
            fallbackGameId = idKey;
            break;
        }
    }
    if(i == knownIdKeyCount)
    {
        LOG_WARNING("Fallback game identity key '%s' specified with -idkey is unknown."
                    " Any converted savegame which relies on this may not be valid")
                << idKey;
    }
}

static Path composeMapUriPath(uint episode, uint map)
{
    if(episode > 0)
    {
        return String("E%1M%2").arg(episode).arg(map > 0? map : 1);
    }
    return String("MAP%1").arg(map > 0? map : 1, 2, 10, QChar('0'));
}

static String identityKeyForLegacyGamemode(int gamemode, SaveFormatId saveFormat, int saveVersion)
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
    if(saveFormat == Doom && saveVersion < 9)
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
                /// @throw Error Game identity key could not be determined unambiguously.
                throw Error("identityKeyForLegacyGamemode", "Game identity key is ambiguous");
            }
            return fallbackGameId;
        }
    }
    if(saveFormat == Heretic && saveVersion < 8)
    {
        static int const oldGamemodes[] = {
            /*heretic_shareware*/   0,
            /*heretic*/             1,
            /*heretic_extended*/    2
        };
        DENG2_ASSERT(gamemode >= 0 && (unsigned)gamemode < sizeof(oldGamemodes) / sizeof(oldGamemodes[0]));
        gamemode = oldGamemodes[gamemode];
    }

    switch(saveFormat)
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

/**
 * Supports native Doomsday savegame formats up to and including version 13.
 */
static void xlatLegacyMetadata(SessionMetadata &metadata, SaveFormat::Reader &reader)
{
#define SM_NOTHINGS     -1
#define SM_BABY         0
#define NUM_SKILL_MODES 5
#define MAXPLAYERS      16

    /*magic*/ reader.readInt32();

    saveVersion = reader.readInt32();
    DENG2_ASSERT(saveVersion >= 0 && saveVersion <= 13);
    if(saveVersion > 13)
    {
        /// @throw Error Format is from the future.
        throw Error("xlatLegacyMetadata", "Incompatible format version " + String::number(saveVersion));
    }
    // We are incompatible with v3 saves due to an invalid test used to determine present
    // sides (ver3 format's sides contain chunks of junk data).
    if(saveFormat().id == Hexen && saveVersion == 3)
    {
        /// @throw Error Map state is in an unsupported format.
        throw Error("xlatLegacyMetadata", "Unsupported format version " + String::number(saveVersion));
    }
    metadata.set("version",             int(14));

    // Translate gamemode identifiers from older save versions.
    int oldGamemode = reader.readInt32();
    metadata.set("gameIdentityKey",     identityKeyForLegacyGamemode(oldGamemode, saveFormat().id, saveVersion));

    // User description. A fixed 24 characters in length in "really old" versions.
    size_t const len = (saveVersion < 10? 24 : (unsigned)reader.readInt32());
    char *descBuf = (char *)malloc(len + 1);
    DENG2_ASSERT(descBuf != 0);
    reader.read(descBuf, len);
    metadata.set("userDescription",     String(descBuf, len));
    free(descBuf); descBuf = 0;

    QScopedPointer<Record> rules(new Record);
    if(saveFormat().id != Hexen && saveVersion < 13)
    {
        // In DOOM the high bit of the skill mode byte is also used for the
        // "fast" game rule dd_bool. There is more confusion in that SM_NOTHINGS
        // will result in 0xff and thus always set the fast bit.
        //
        // Here we decipher this assuming that if the skill mode is invalid then
        // by default this means "spawn no things" and if so then the "fast" game
        // rule is meaningless so it is forced off.
        dbyte skillModePlusFastBit = reader.readInt8();
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
        int skill = reader.readInt8() & 0x7f;
        // Interpret skill levels outside the normal range as "spawn no things".
        if(skill < SM_BABY || skill >= NUM_SKILL_MODES)
        {
            skill = SM_NOTHINGS;
        }
        rules->set("skill", skill);
    }

    uint episode = reader.readInt8();
    uint map     = reader.readInt8() - 1;
    metadata.set("mapUri",              composeMapUriPath(episode, map).asText());

    rules->set("deathmatch", reader.readInt8());
    if(saveFormat().id != Hexen && saveVersion == 13)
    {
        rules->set("fast", reader.readInt8());
    }
    rules->set("noMonsters", reader.readInt8());
    if(saveFormat().id == Hexen)
    {
        rules->set("randomClasses", reader.readInt8());
    }
    else
    {
        rules->set("respawnMonsters", reader.readInt8());
    }

    metadata.add("gameRules",           rules.take());

    if(saveFormat().id != Hexen)
    {
        /*skip junk*/ if(saveVersion < 10) reader.seek(2);

        metadata.set("mapTime",         reader.readInt32());

        ArrayValue *array = new de::ArrayValue;
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            *array << NumberValue(reader.readInt8() != 0, NumberValue::Boolean);
        }
        metadata.set("players",         array);
    }

    metadata.set("sessionId",           reader.readInt32());

#undef MAXPLAYERS
#undef NUM_SKILL_MODES
#undef SM_BABY
#undef SM_NOTHINGS
}

static String composeInfo(SessionMetadata const &metadata, Path const &sourceFile,
    int oldSaveVersion)
{
    String info;
    QTextStream os(&info);
    os.setCodec("UTF-8");

    // Write header and misc info.
    Time now;
    os << "# Doomsday Engine saved game session package.\n#"
       << "\n# Generator: " + versionText()
       << "\n# Generation Date: " + now.asDateTime().toString(Qt::SystemLocaleShortDate)
       << "\n# Source file: \"" + NativePath(sourceFile).pretty() + "\""
       << "\n# Source version: " + String::number(oldSaveVersion);

    // Write metadata.
    os << "\n\n" + metadata.asTextWithInfoSyntax() + "\n";

    return info;
}

/**
 * (Deferred) ACScript translator utility.
 */
struct ACScriptTask : public IWritable
{
    duint32 mapNumber;
    dint32 scriptNumber;
    dbyte args[4];

    static ACScriptTask *ACScriptTask::fromReader(SaveFormat::Reader &reader)
    {
        ACScriptTask *task = new ACScriptTask;
        task->read(reader);
        return task;
    }

    void read(SaveFormat::Reader &reader)
    {
        mapNumber    = duint32(reader.readInt32());
        scriptNumber = reader.readInt32();
        reader.read((char *)args, 4);
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

static void xlatWorldACScriptData(SaveFormat::Reader &reader, Writer &writer)
{
#define MAX_ACS_WORLD_VARS 64

    if(saveVersion >= 7)
    {
        if(reader.readInt32() != ASEG_WORLDSCRIPTDATA)
        {
            /// @throw Error Failed alignment check.
            throw Error("xlatWorldACScriptData", "Corrupt save game, segment #" + String::number(ASEG_WORLDSCRIPTDATA) + " failed alignment check");
        }
    }

    int const ver = (saveVersion >= 7)? reader.readInt8() : 1;
    if(ver < 1 || ver > 3)
    {
        /// @throw Error Format is from the future.
        throw Error("xlatWorldACScriptData", "Incompatible data segment version " + String::number(ver));
    }

    dint32 worldVars[MAX_ACS_WORLD_VARS];
    for(int i = 0; i < MAX_ACS_WORLD_VARS; ++i)
    {
        worldVars[i] = reader.readInt32();
    }

    // Read the deferred tasks into a temporary buffer for translation.
    int const oldStoreSize = reader.readInt32();
    ACScriptTasks tasks;

    if(oldStoreSize > 0)
    {
        tasks.reserve(oldStoreSize);
        for(int i = 0; i < oldStoreSize; ++i)
        {
            tasks.append(ACScriptTask::fromReader(reader));
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

    /* skip junk */ if(saveVersion < 7) reader.seek(12);

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

static Block *mapStateHeader()
{
    Block *hdr = new Block;
    Writer(*hdr) << saveFormat().magic << saveVersion;
    return hdr;
}

/// @param oldSavePath  Path to the game state file [.dsg | .hsg | .hxs]
static bool convertSavegame(Path oldSavePath)
{
    /// @todo try all known extensions at the given path, if not specified.
    String saveName = oldSavePath.lastSegment().toString();

    try
    {
        foreach(SaveFormat *fmt, saveFormats)
        {
            if(fmt->recognize(oldSavePath))
            {
                LOG_VERBOSE("Recognized \"%s\" as a %s format savegame")
                    << NativePath(oldSavePath).pretty() << fmt->textualId;
                knownSaveFormat = fmt;
            }
        }

        // Still unknown? Try again with "fuzzy" logic.
        if(!knownSaveFormat)
        {
            // Unknown magic
            if(!fallbackGameId.isEmpty())
            {
                // Use whichever format is applicable for the specified identity key.
                knownSaveFormat = saveFormatForGameIdentityKey(fallbackGameId);
            }
            else if(!saveName.fileNameExtension().isEmpty())
            {
                // We'll try to guess the save format...
                knownSaveFormat = guessSaveFormatFromFileName(saveName);
            }
        }

        // Still unknown!?
        if(!knownSaveFormat)
        {
            /// @throw Error Failed to determine the format of the saved game session.
            throw Error("convertSavegame", "Format of \"" + NativePath(oldSavePath).pretty() + "\" is unknown");
        }

        saveFormat().openFile(oldSavePath);
        SaveFormat::Reader *reader = saveFormat().newReader();

        // Read and translate the game session metadata.
        SessionMetadata metadata;
        xlatLegacyMetadata(metadata, *reader);

        ZipArchive arch;
        arch.add("Info", composeInfo(metadata, oldSavePath, saveVersion).toUtf8());

        if(saveFormat().id == Hexen)
        {
            // Translate and separate the serialized world ACS data into a new file.
            Block worldACScriptData;
            Writer writer(worldACScriptData);
            xlatWorldACScriptData(*reader, writer);
            arch.add("ACScriptState", worldACScriptData);
        }

        if(saveFormat().id == Hexen)
        {
            // Serialized map states are in similarly named "side car" files.
            int const maxHubMaps = 99;
            for(int i = 0; i < maxHubMaps; ++i)
            {
                // Open the map state file.
                Path oldMapStatePath =
                        oldSavePath.toString().fileNamePath() / saveName.fileNameWithoutExtension()
                            + String("%1").arg(i + 1, 2, 10, QChar('0'))
                            + saveName.fileNameExtension();

                saveFormat().closeFile();
                try
                {
                    saveFormat().openFile(oldMapStatePath);
                    // Buffer the file and write it out to a new map state file.
                    if(Block *xlatedData = saveFormat().bufferFile())
                    {
                        // Append the remaining translated data to header, forming the new serialized
                        // map state data file.
                        Block *mapStateData = mapStateHeader();
                        *mapStateData += *xlatedData;
                        delete xlatedData;

                        arch.add(Path("maps") / composeMapUriPath(0, i) + "State", *mapStateData);
                        delete mapStateData;
                    }
                }
                catch(SaveFormat::FileOpenError const &)
                {} // Ignore this error.
            }
        }
        else
        {
            // The only serialized map state follows the session metadata in the game state file.
            // Buffer the rest of the file and write it out to a new map state file.
            if(Block *xlatedData = saveFormat().bufferFile())
            {
                String const mapUriStr = metadata["mapUri"].value().asText();

                // Append the remaining translated data to header, forming the new serialized
                // map state data file.
                Block *mapStateData = mapStateHeader();
                *mapStateData += *xlatedData;
                delete xlatedData;

                arch.add(Path("maps") / mapUriStr + "State", *mapStateData);
                delete mapStateData;
            }
        }

        delete reader;
        saveFormat().closeFile();

        File &saveFile = DENG2_TEXT_APP->homeFolder().replaceFile(saveName.fileNameWithoutExtension() + ".save");
        saveFile.setMode(File::Write | File::Truncate);
        Writer(saveFile) << arch;
        LOG_MSG("Wrote ") << saveFile.path();

        return true;
    }
    catch(Error const &er)
    {
        if(knownSaveFormat) saveFormat().closeFile();
        LOG_ERROR("%s failed conversion:\n") << oldSavePath << er.asText();
    }

    return false;
}

int main(int argc, char **argv)
{
    initSaveFormats();

    try
    {
        TextApp app(argc, argv);
        app.setMetadata("Deng Team", "dengine.net", "Savegame Tool", "1.0.0");
        app.initSubsystems(App::DisablePlugins | App::DisablePersistentData);

        // Name the log output file appropriately.
        LogBuffer::appBuffer().setOutputFile(app.homeFolder().path() / "savegametool.out",
                                             LogBuffer::DontFlush);

        // Print a banner.
        LOG_MSG("") << versionText();

        CommandLine &args = app.commandLine();
        if(args.count() < 2 ||
           args.has("-h") || args.has("-?") || args.has("--help"))
        {
            printUsage();
            printDescription();
        }
        else
        {
            // Was a fallback game identity key specified?
            if(int arg = args.check("-idkey", 1))
            {
                setFallbackGameIdentityKey(args.at(arg + 1).strip().toLower());
            }

            // Scan the command line arguments looking for savegame names/paths.
            for(int i = 1; i < args.count(); ++i)
            {
                if(args.at(i).first() != '-') // Not an option?
                {
                    // Process this savegame.
                    convertSavegame(args.at(i));
                }
            }
        }
    }
    catch(Error const &err)
    {
        qWarning() << err.asText();
    }

    qDeleteAll(saveFormats);
    return 0;
}
