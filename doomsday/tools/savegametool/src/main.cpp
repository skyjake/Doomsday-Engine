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

#include <QDebug>
#include <QList>
#include <QMutableListIterator>
#include <QtAlgorithms>
#include <de/TextApp>
#include <de/Time>
#include "id1translator.h"
#include "nativetranslator.h"

using namespace de;

String fallbackGameId;

typedef QList<PackageFormatter *> FormatTranslators;
static FormatTranslators translators;

static void initTranslators()
{
    // Add Doomsday-native formats:
    translators << new NativeTranslator(NativeTranslator::Doom,    "Doom",    0x1DEAD666, QStringList(".dsg"), QStringList() << "doom" << "hacx" << "chex");
    translators << new NativeTranslator(NativeTranslator::Heretic, "Heretic", 0x7D9A12C5, QStringList(".hsg"), QStringList() << "heretic");
    translators << new NativeTranslator(NativeTranslator::Hexen,   "Hexen",   0x1B17CC00, QStringList(".hxs"), QStringList() << "hexen");

    // Add IdTech1 formats:
    translators << new Id1Translator(Id1Translator::DoomV9,     "Doom (id Tech 1)",    0x1DEAD666, QStringList(".dsg"), QStringList() << "doom" << "hacx" << "chex");
    translators << new Id1Translator(Id1Translator::HereticV13, "Heretic (id Tech 1)", 0x7D9A12C5, QStringList(".hsg"), QStringList() << "heretic");
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

String versionText()
{
    return String("%1 version %2 (%3)")
               .arg(DENG2_TEXT_APP->applicationName())
               .arg(DENG2_TEXT_APP->applicationVersion())
               .arg(Time::fromText(__DATE__ " " __TIME__, Time::CompilerDateTime)
                    .asDateTime().toString(Qt::SystemLocaleShortDate));
}

Path composeMapUriPath(duint32 episode, duint32 map)
{
    if(episode > 0)
    {
        return String("E%1M%2").arg(episode).arg(map > 0? map : 1);
    }
    return String("MAP%1").arg(map > 0? map : 1, 2, 10, QChar('0'));
}

static PackageFormatter *saveFormatForGameIdentityKey(String const &idKey)
{
    foreach(PackageFormatter *fmt, translators)
    foreach(QString const &baseIdentityKey, fmt->baseGameIdKeys)
    {
        if(idKey.beginsWith(baseIdentityKey)) return fmt;
    }
    return 0; // Not found.
}

static PackageFormatter *guessSaveFormatFromFileName(Path const &path)
{
    String ext = path.lastSegment().toString().fileNameExtension().toLower();
    foreach(PackageFormatter *fmt, translators)
    foreach(QString const &knownExtension, fmt->knownExtensions)
    {
        if(!knownExtension.compare(ext, Qt::CaseInsensitive)) return fmt;
    }
    return 0; // Not found.
}

static PackageFormatter *knownTranslator;

/// Returns the current, known format translator.
static PackageFormatter &translator()
{
    if(knownTranslator)
    {
        return *knownTranslator;
    }
    throw Error("translator", "Current save format is unknown");
}

/// @param oldSavePath  Path to the game state file [.dsg | .hsg | .hxs]
static bool convertSavegame(Path oldSavePath)
{
    /// @todo try all known extensions at the given path, if not specified.
    String saveName = oldSavePath.lastSegment().toString();

    try
    {
        foreach(PackageFormatter *fmt, translators)
        {
            if(fmt->recognize(oldSavePath))
            {
                LOG_VERBOSE("Recognized \"%s\" as a %s format savegame")
                        << NativePath(oldSavePath).pretty() << fmt->textualId;
                knownTranslator = fmt;
                break;
            }
        }

        // Still unknown? Try again with "fuzzy" logic.
        if(!knownTranslator)
        {
            // Unknown magic
            if(!fallbackGameId.isEmpty())
            {
                // Use whichever format is applicable for the specified identity key.
                knownTranslator = saveFormatForGameIdentityKey(fallbackGameId);
            }
            else if(!saveName.fileNameExtension().isEmpty())
            {
                // We'll try to guess the save format...
                knownTranslator = guessSaveFormatFromFileName(saveName);
            }
        }

        if(knownTranslator)
        {
            translator().convert(oldSavePath);
            return true;
        }
        /// @throw Error  Ambigous/unknown format.
        throw Error("convertSavegame", "Format of \"" + NativePath(oldSavePath).pretty() + "\" is unknown");
    }
    catch(Error const &er)
    {
        LOG_ERROR("\"%s\" failed conversion:\n")
                << NativePath(oldSavePath).pretty() << er.asText();
    }

    return false;
}

int main(int argc, char **argv)
{
    initTranslators();

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
                fallbackGameId = args.at(arg + 1).strip().toLower();
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

    qDeleteAll(translators);
    return 0;
}
