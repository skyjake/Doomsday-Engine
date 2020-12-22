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

#include <de/directoryfeed.h>
#include <de/textapp.h>
#include <de/commandline.h>
#include <de/logbuffer.h>
#include <de/filesystem.h>
#include <de/time.h>
#include "id1translator.h"
#include "nativetranslator.h"

using namespace de;

String fallbackGameId;

typedef List<PackageFormatter *> FormatTranslators;

static FormatTranslators         translators;
static PackageFormatter *        knownTranslator;

static void initTranslators()
{
    // Order defines save format translator recognition order.

    // Add Doomsday-native formats:
    translators << new NativeTranslator(NativeTranslator::Doom,    {".dsg"}, {"doom", "hacx", "chex"});
    translators << new NativeTranslator(NativeTranslator::Heretic, {".hsg"}, {"heretic"});
    translators << new NativeTranslator(NativeTranslator::Hexen,   {".hxs"}, {"hexen"});

    // Add id Tech1 formats:
    translators << new Id1Translator(Id1Translator::DoomV9,     {".dsg"}, {"doom", "hacx", "chex"});
    translators << new Id1Translator(Id1Translator::HereticV13, {".hsg"}, {"heretic"});
}

static void printUsage()
{
    LOG_INFO(  "Usage: %s [options] savegame-path ..."
             "\nOptions:"
             "\n--help, -h, -?  Show usage information."
             "\n-idKey   Fallback game ID. Used to resolve ambigous savegame formats."
             "\n-output  Redirect .save output to this directory (default is the working directory).")
            << DE_TEXT_APP->commandLine().at(0);
}

static void printDescription()
{
    LOG_VERBOSE("%s is a utility for converting legacy Doomsday Engine, Doom and Heretic savegame"
                " files into a format recognized by Doomsday Engine version 1.14 (or newer).")
            << DE_TEXT_APP->metadata().gets(App::APP_NAME);
}

String versionText()
{
    return DE_TEXT_APP->metadata().gets(App::APP_NAME) + " version " +
           DE_TEXT_APP->metadata().gets(App::APP_VERSION) + " (" __DATE__ " " __TIME__ ")";
}

Path composeMapUriPath(duint32 episode, duint32 map)
{
    if (episode > 0)
    {
        return Stringf("E%iM%i", episode, map > 0? map : 1);
    }
    return Stringf("MAP%02i", map > 0? map : 1);
}

Folder &outputFolder()
{
    return DE_TEXT_APP->rootFolder().locate<Folder>("output");
}

static PackageFormatter *saveFormatForGameId(const String &idKey)
{
    for (PackageFormatter *fmt : translators)
    {
        for (const String &baseId : fmt->baseGameIds)
        {
            if (idKey.beginsWith(baseId)) return fmt;
        }
    }
    return 0; // Not found.
}

static PackageFormatter *guessSaveFormatFromFileName(const Path &path)
{
    String ext = path.lastSegment().toLowercaseString().fileNameExtension();
    for (PackageFormatter *fmt : translators)
    {
        if (String::contains(fmt->knownExtensions, ext, CaseInsensitive))
        {
            return fmt;
        }
    }
    return 0; // Not found.
}

/// Returns the current, known format translator.
static PackageFormatter &translator()
{
    if (knownTranslator)
    {
        return *knownTranslator;
    }
    throw Error("translator", "Current save format is unknown");
}

/// @param inputPath  Path to the game state file [.dsg | .hsg | .hxs] in the vfs.
static void convertSavegame(const Path& inputPath)
{
    for (PackageFormatter *xlator : translators)
    {
        if (xlator->recognize(inputPath))
        {
            LOG_VERBOSE("Recognized \"%s\" as a %s format savegame")
                    << NativePath(inputPath).pretty() << xlator->formatName();
            knownTranslator = xlator;
            break;
        }
    }

    // Still unknown? Try again with "fuzzy" logic.
    if (!knownTranslator)
    {
        // Unknown magic
        if (!fallbackGameId.isEmpty())
        {
            // Use whichever format is applicable for the specified game ID.
            knownTranslator = saveFormatForGameId(fallbackGameId);
        }
        else if (!inputPath.toString().fileNameExtension().isEmpty())
        {
            // We'll try to guess the save format...
            knownTranslator = guessSaveFormatFromFileName(inputPath);
        }
    }

    if (knownTranslator)
    {
        translator().convert(inputPath);
        return;
    }

    /// @throw Error  Ambigous/unknown format.
    throw Error("convertSavegame", "Format of \"" + NativePath(inputPath).pretty() + "\" is unknown");
}

int main(int argc, char **argv)
{
    init_Foundation();
    initTranslators();
    try
    {
        TextApp app(makeList(argc, argv));
        app.setMetadata("Deng Team", "dengine.net", "Savegame Tool", "1.0.1");
        app.initSubsystems(App::DisablePersistentData);

        // Name the log output file appropriately.
        LogBuffer::get().setOutputFile(app.homeFolder().path() / "savegametool.out",
                                       LogBuffer::DontFlush);

        // Default /output to the current working directory.
        app.fileSystem().makeFolderWithFeed("/output",
                new DirectoryFeed(NativePath::workPath(), DirectoryFeed::AllowWrite),
                Folder::PopulateOnlyThisFolder);

        // Print a banner.
        LOG_MSG("") << versionText();

        CommandLine &args = app.commandLine();
        if (args.count() < 2 ||
           args.has("-h") || args.has("-?") || args.has("--help"))
        {
            printUsage();
            printDescription();
        }
        else
        {
            for (int i = 1; i < args.sizei(); ++i)
            {
                if (args.isOption(i))
                {
                    // Was a fallback game ID specified?
                    if (i + 1 < args.sizei() && !args.at(i).compareWithoutCase("-idkey"))
                    {
                        fallbackGameId = args.at(i + 1).strip().lower();
                        i += 1;
                    }
                    // The -output option can be used to redirect .save output.
                    if (i + 1 < args.sizei() && !args.at(i).compareWithoutCase("-output"))
                    {
                        args.makeAbsolutePath(i + 1);
                        app.fileSystem().makeFolderWithFeed("/output",
                                new DirectoryFeed(NativePath(args.at(i + 1)),
                                                  DirectoryFeed::AllowWrite | DirectoryFeed::CreateIfMissing),
                                Folder::PopulateOnlyThisFolder);
                        i += 1;
                    }
                    continue;
                }

                // Process the named savegame on this input path.
                args.makeAbsolutePath(i);
                const Path inputPath = NativePath(args.at(i)).withSeparators('/');

                // A file name is required.
                if (inputPath.fileName().isEmpty())
                {
                    LOG_ERROR("\"%s\" is missing a file name, cannot convert")
                            << NativePath(inputPath).pretty();
                    continue;
                }

                // Ensure we have read access to the input folder on the local fs.
                NativePath nativeInputFolderPath = inputPath.toString().fileNamePath();
                if (!nativeInputFolderPath.exists() || !nativeInputFolderPath.isReadable())
                {
                    LOG_ERROR("\"%s\" is not accessible (insufficient permissions?) and will not be converted")
                            << NativePath(inputPath).pretty();
                    continue;
                }

                // Update the /input folder using the input folder on the local fs.
                app.fileSystem().makeFolderWithFeed(
                            "/input", new DirectoryFeed(nativeInputFolderPath),
                            Folder::PopulateOnlyThisFolder /* no need to go deeper */);

                // Convert the named save game.
                try
                {
                    convertSavegame(Path("/input") / inputPath.fileName());
                }
                catch (const Error &er)
                {
                    LOG_ERROR("\"%s\" failed conversion:\n")
                            << NativePath(inputPath).pretty() << er.asText();
                }
            }
        }
    }
    catch (const Error &err)
    {
        err.warnPlainText();
    }
    deleteAll(translators);
    deinit_Foundation();
    return 0;
}
