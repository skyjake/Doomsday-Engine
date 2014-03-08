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
#include <de/reader.h>            /// @todo remove me
#include <de/TextApp>
#include <de/ArrayValue>
#include <de/game/savedsession.h>
#include <de/NumberValue>
#include <de/RecordValue>
#include <de/Time>
#include <de/Writer>
#include <de/ZipArchive>

#ifdef __BIG_ENDIAN__
static int16_t ShortSwap(int16_t n)
{
    return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
}

static int32_t LongSwap(int32_t n)
{
    return (((n & 0xff) << 24) | ((n & 0xff00) << 8) |
            ((n & 0xff0000) >> 8) | ((n & 0xff000000) >> 24));
}

static float FloatSwap(float f)
{
    int32_t temp = 0;
    float returnValue = 0;

    std::memcpy(&temp, &f, 4); // Must be 4.
    temp = LongSwap(temp);
    std::memcpy(&returnValue, &temp, 4); // Must be 4.
    return returnValue;
}

#  define SHORT(x) ShortSwap(x)
#  define LONG(x)  LongSwap(x)
#  define FLOAT(x) FloatSwap(x)

#else
#  define SHORT(x) (x)
#  define LONG(x)  (x)
#  define FLOAT(x) (x)
#endif

using namespace de;
using de::game::SessionMetadata;

enum SaveFormat
{
    Unknown,
    Doom,
    Heretic,
    Hexen
};

static String fallbackGameIdentityKey;

static LZFILE *saveFile;
static SaveFormat saveFormat;
static int saveVersion;

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
            fallbackGameIdentityKey = idKey;
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

static bool openFile(Path path)
{
    LOG_TRACE("openFile: Opening \"%s\"") << NativePath(path).pretty();
    DENG2_ASSERT(saveFile == 0);
    saveFile = lzOpen(NativePath(path).expand().toUtf8().constData(), "rp");
    return saveFile != 0;
}

static void closeFile()
{
    if(saveFile)
    {
        lzClose(saveFile);
        saveFile = 0;
    }
}

/**
 * Decompress the rest of the open file into a newly allocated memory buffer.
 *
 * @return  The decompressed data buffer; otherwise @c 0 if at the end of the file.
 */
static Block *bufferFile()
{
#define BLOCKSIZE 1024  // Read in 1kb blocks.

    DENG2_ASSERT(saveFile);

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

#undef BSIZE
}

static void srSeek(reader_s *r, uint offset)
{
    if(!r) return;
    lzSeek(saveFile, offset);
}

static char sri8(reader_s *r)
{
    if(!r) return 0;
    return lzGetC(saveFile);
}

static short sri16(reader_s *r)
{
    if(!r) return 0;
    return lzGetW(saveFile);
}

static int sri32(reader_s *r)
{
    if(!r) return 0;
    return lzGetL(saveFile);
}

static float srf(reader_s *r)
{
    if(!r) return 0;
    DENG2_ASSERT(sizeof(float) == 4);
    int32_t val = lzGetL(saveFile);
    float returnValue = 0;
    std::memcpy(&returnValue, &val, 4);
    return returnValue;
}

static void srd(reader_s *r, char *data, int len)
{
    if(!r) return;
    lzRead(data, len, saveFile);
}

static reader_s *newReader()
{
    return Reader_NewWithCallbacks(sri8, sri16, sri32, srf, srd);
}

static String composeMapUriStr(uint episode, uint map)
{
    if(episode > 0)
    {
        return String("E%1M%2").arg(episode).arg(map > 0? map : 1);
    }
    return String("MAP%1").arg(map > 0? map : 1, 2, 10, QChar('0'));
}

static String identityKeyForLegacyGamemode(int gamemode, SaveFormat saveFormat, int saveVersion)
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
            if(fallbackGameIdentityKey.isEmpty())
            {
                /// @throw Error Game identity key could not be determined unambiguously.
                throw Error("identityKeyForLegacyGamemode", "Game identity key is ambiguous");
            }
            return fallbackGameIdentityKey;
        }
        /// kludge end.
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
static void xlatLegacyMetadata(SessionMetadata &metadata, reader_s *reader)
{
#define SM_NOTHINGS     -1
#define SM_BABY         0
#define NUM_SKILL_MODES 5
#define MAXPLAYERS      16

    saveVersion = Reader_ReadInt32(reader);
    DENG2_ASSERT(saveVersion >= 0 && saveVersion <= 13);
    if(saveVersion > 13)
    {
        /// @throw Error Format is from the future.
        throw Error("xlatLegacyMetadata", "Incompatible format version " + String::number(saveVersion));
    }
    metadata.set("version",             int(14));

    // Translate gamemode identifiers from older save versions.
    int oldGamemode = Reader_ReadInt32(reader);
    metadata.set("gameIdentityKey",     identityKeyForLegacyGamemode(oldGamemode, saveFormat, saveVersion));

    // User description. A fixed 24 characters in length in "really old" versions.
    size_t const len = (saveVersion < 10? 24 : (unsigned)Reader_ReadInt32(reader));
    char *descBuf = (char *)malloc(len + 1);
    DENG2_ASSERT(descBuf != 0);
    Reader_Read(reader, descBuf, len);
    metadata.set("userDescription",     String(descBuf, len));
    free(descBuf); descBuf = 0;

    QScopedPointer<Record> rules(new Record);
    if(saveFormat != Hexen && saveVersion < 13)
    {
        // In DOOM the high bit of the skill mode byte is also used for the
        // "fast" game rule dd_bool. There is more confusion in that SM_NOTHINGS
        // will result in 0xff and thus always set the fast bit.
        //
        // Here we decipher this assuming that if the skill mode is invalid then
        // by default this means "spawn no things" and if so then the "fast" game
        // rule is meaningless so it is forced off.
        byte skillModePlusFastBit = Reader_ReadByte(reader);
        int skill = (skillModePlusFastBit & 0x7f);
        if(skill < SM_BABY || skill >= NUM_SKILL_MODES)
        {
            skill = SM_NOTHINGS;
            rules->addBoolean("fast", false);
        }
        else
        {
            rules->addBoolean("fast", CPP_BOOL(skillModePlusFastBit & 0x80));
        }
        rules->set("skill", skill);
    }
    else
    {
        int skill = Reader_ReadByte(reader) & 0x7f;
        // Interpret skill levels outside the normal range as "spawn no things".
        if(skill < SM_BABY || skill >= NUM_SKILL_MODES)
        {
            skill = SM_NOTHINGS;
        }
        rules->set("skill", skill);
    }

    uint episode = Reader_ReadByte(reader) - 1;
    uint map     = Reader_ReadByte(reader) - 1;
    metadata.set("mapUri",              composeMapUriStr(episode, map));

    rules->set("deathmatch", Reader_ReadByte(reader));
    rules->set("noMonsters", Reader_ReadByte(reader));
    if(saveFormat == Hexen)
    {
        rules->set("randomClasses", Reader_ReadByte(reader));
    }
    else
    {
        rules->set("respawnMonsters", Reader_ReadByte(reader));
    }

    metadata.add("gameRules",           rules.take());

    if(saveFormat != Hexen)
    {
        /*skip old junk*/ if(saveVersion < 10) srSeek(reader, 2);

        metadata.set("mapTime",         Reader_ReadInt32(reader));

        ArrayValue *array = new de::ArrayValue;
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            *array << NumberValue(CPP_BOOL(Reader_ReadByte(reader)), NumberValue::Boolean);
        }
        metadata.set("players",         array);
    }

    metadata.set("sessionId",           Reader_ReadInt32(reader));

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

static SaveFormat saveFormatForGameIdentityKey(String const &idKey)
{
    if(idKey.beginsWith("doom") || !idKey.compare("hacx") || !idKey.compare("chex"))
    {
        return Doom;
    }
    if(idKey.beginsWith("heretic"))
    {
        return Heretic;
    }
    if(idKey.beginsWith("hexen"))
    {
        return Hexen;
    }
    return Unknown;
}

static SaveFormat guessSaveFormatFromFileName(Path const &path)
{
    String ext = path.lastSegment().toString().fileNameExtension().toLower();
    if(!ext.compare(".dsg")) return Doom;
    if(!ext.compare(".hsg")) return Heretic;
    if(!ext.compare(".hxs")) return Hexen;
    return Unknown;
}

/// @param oldSavePath  Path to the game state file [.dsg | .hsg | .hxs]
static bool convertSaveGame(Path oldSavePath)
{
    /// @todo try all known extensions at the given path, if not specified.
    String saveName = oldSavePath.lastSegment().toString();

    try
    {
        ZipArchive arch;

        if(!openFile(oldSavePath))
        {
            /// @throw Error Failed to open source file.
            throw Error("convertSaveGame", "Failed to open \"" + NativePath(oldSavePath).pretty() + "\" for read");
        }

        reader_s *reader = newReader();

        int const magic = Reader_ReadInt32(reader);
        if(magic == 0x1DEAD666)
        {
            saveFormat = Doom;
        }
        else if(magic == 0x7D9A12C5)
        {
            saveFormat = Heretic;
        }
        else if(magic == 0x1B17CC00)
        {
            saveFormat = Hexen;
        }
        else // Unknown magic
        {
            if(!fallbackGameIdentityKey.isEmpty())
            {
                // Use whichever format is applicable for the specified identity key.
                saveFormat = saveFormatForGameIdentityKey(fallbackGameIdentityKey);
            }
            else if(!saveName.fileNameExtension().isEmpty())
            {
                // We'll try to guess the save format...
                saveFormat = guessSaveFormatFromFileName(saveName);
            }
        }

        if(saveFormat == Unknown)
        {
            Reader_Delete(reader);
            /// @throw Error Failed to determine the format of the saved game session.
            throw Error("convertSaveGame", "Format of \"" + NativePath(oldSavePath).pretty() + "\" is unknown");
        }

        // Read and translate the game session metadata.
        SessionMetadata metadata;
        xlatLegacyMetadata(metadata, reader);

        arch.add("Info", composeInfo(metadata, oldSavePath, saveVersion).toUtf8());

        String const mapUriStr = metadata["mapUri"].value().asText();
        if(saveFormat == Hexen)
        {
            // Serialized map states are written to similarly named "side car" files.
            int const maxHubMaps = 99;
            for(int i = 0; i < maxHubMaps; ++i)
            {
                // Open the map state file.
                Path oldMapStatePath =
                        oldSavePath.toString().fileNamePath() / saveName.fileNameWithoutExtension()
                            + String("%1").arg(i + 1, 2, 10, QChar('0'))
                            + saveName.fileNameExtension();

                closeFile();
                if(openFile(oldMapStatePath))
                {
                    if(Block *mapStateData = bufferFile())
                    {
                        arch.add(Path("maps") / composeMapUriStr(0, i), *mapStateData);
                        delete mapStateData;
                    }
                }
            }
        }
        else
        {
            // The only serialized map state follows the session metadata in the game state file.
            // Decompress the rest of the file and write it out to a new map state file.
            if(Block *mapStateData = bufferFile())
            {
                arch.add(Path("maps") / mapUriStr, *mapStateData);
                delete mapStateData;
            }
        }

        Reader_Delete(reader); reader = 0;
        closeFile();

        File &saveFile = DENG2_TEXT_APP->homeFolder().replaceFile(saveName.fileNameWithoutExtension() + ".save");
        saveFile.setMode(File::Write | File::Truncate);
        de::Writer(saveFile) << arch;
        LOG_MSG("Wrote ") << saveFile.path();

        return true;
    }
    catch(Error const &er)
    {
        closeFile();
        LOG_ERROR("%s failed conversion:\n") << oldSavePath << er.asText();
    }

    return false;
}

int main(int argc, char **argv)
{
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
                    convertSaveGame(args.at(i));
                }
            }
        }
    }
    catch(Error const &err)
    {
        qWarning() << err.asText();
    }

    return 0;
}
