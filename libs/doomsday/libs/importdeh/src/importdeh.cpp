/** @file importdeh.cpp  DeHackEd patch reader plugin for Doomsday Engine.
 *
 * @authors Copyright © 2013-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "importdeh.h"

#include <doomsday/filesys/lumpindex.h>
#include <doomsday/res/bundles.h>
#include <doomsday/doomsdayapp.h>
#include <de/app.h>
#include <de/commandline.h>
#include <de/block.h>
#include <de/extension.h>
#include <de/log.h>
#include <de/string.h>

#include "dehreader.h"

using namespace de;
using namespace res;

// Global handle on the engine's definition databases.
ded_t *ded;

// This is the original data before it gets replaced by any patches.
ded_sprid_t origSpriteNames[NUMSPRITES];
String      origActionNames[NUMSTATES];

static void backupData()
{
    for (int i = 0; i < NUMSPRITES && i < ded->sprites.size(); i++)
    {
        strncpy(origSpriteNames[i].id, ded->sprites[i].id, DED_SPRITEID_LEN + 1);
    }

    for (int i = 0; i < NUMSTATES && i < ded->states.size(); i++)
    {
        origActionNames[i] = ded->states[i].gets("action");
    }
}

static void readLump(const LumpIndex &lumpIndex, lumpnum_t lumpNum)
{
    if (0 > lumpNum || lumpNum >= lumpIndex.size())
    {
        LOG_AS("DehRead::readLump");
        LOG_WARNING("Invalid lump index #%i, ignoring.") << lumpNum;
        return;
    }

    File1 &lump = lumpIndex[lumpNum];
    size_t len  = lump.size();
    Block deh   = Block(lump.cache(), len);
    /// @attention Results in a deep-copy of the lump data into the Block
    ///            thus the cached lump can be released after this call.
    ///
    /// @todo Do not use a local buffer - read using QTextStream.
    lump.unlock();

    /// @todo Custom status for contained files is not inherited from the container?
    bool lumpIsCustom = (lump.isContained()? lump.container().hasCustom() : lump.hasCustom());

    LOG_RES_MSG("Applying DeHackEd patch lump #%i \"%s:%s\"%s")
            << lumpNum
            << NativePath(lump.container().composePath()).pretty()
            << lump.name()
            << (lumpIsCustom? " (custom)" : "");

    readDehPatch(deh, lumpIsCustom, NoInclude | IgnoreEOF);
}

#if 0
static void readFile(const String &sourcePath, bool sourceIsCustom = true)
{
    LOG_AS("DehRead::readFile");

    QFile file(sourcePath);
    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        LOG_WARNING("Failed opening \"%s\" for read, aborting...") << QDir::toNativeSeparators(sourcePath);
        return;
    }

    LOG_RES_MSG("Applying DeHackEd patch file \"%s\"%s")
            << NativePath(sourcePath).pretty()
            << (sourceIsCustom? " (custom)" : "");

    readDehPatch(file.readAll(), sourceIsCustom, IgnoreEOF);
}
#endif

static void readFile2(const String &path, bool sourceIsCustom = true)
{
    LOG_AS("DehRead::readFile2");

    if (const File *file = App::rootFolder().tryLocate<File const>(path))
    {
        LOG_RES_MSG("Applying %s%s")
                << file->description()
                << (sourceIsCustom? " (custom)" : "");

        Block deh;
        *file >> deh;
        readDehPatch(deh, sourceIsCustom, IgnoreEOF);
    }
    else
    {
        LOG_RES_WARNING("\"%s\" not found") << path;
    }
}

static void readPatchLumps(const LumpIndex &lumpIndex)
{
    const bool readAll = DE_APP->commandLine().check("-alldehs");
    for (int i = lumpIndex.size() - 1; i >= 0; i--)
    {
        if (lumpIndex[i].name().fileNameExtension().compare(".deh", CaseInsensitive) == 0)
        {
            readLump(lumpIndex, i);
            if (!readAll) return;
        }
    }
}

static void readPatchFiles()
{
    // Patches may be loaded as data bundles.
    for (const DataBundle *bundle : DataBundle::loadedBundles())
    {
        if (bundle->format() == DataBundle::Dehacked)
        {
            const String bundleRoot = bundle->rootPath();
            for (const Value *path : bundle->packageMetadata().geta("dataFiles").elements())
            {
                readFile2(bundleRoot / path->asText());
            }
        }
    }
}

/**
 * This will be called after the engine has loaded all definitions but before
 * the data they contain has been initialized.
 */
static int DefsHook(int /*hook_type*/, int /*parm*/, void *data)
{
    // Grab the DED definition handle supplied by the engine.
    ded = reinterpret_cast<ded_t *>(data);

    backupData();

    // Check for DEHACKED lumps.
    readPatchLumps(*reinterpret_cast<const res::LumpIndex *>(F_LumpIndex()));

    // Process all patch files specified with -deh options on the command line.
    readPatchFiles();

    return true;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
static void DP_Initialize()
{
    Plug_AddHook(HOOK_DEFS, DefsHook);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
static const char *deng_LibraryType()
{
    return "deng-plugin/generic";
}

//DE_DECLARE_API(Base);
//DE_DECLARE_API(Con);
//DE_DECLARE_API(Def);
//DE_DECLARE_API(F);

//DE_API_EXCHANGE(
//    DE_GET_API(DE_API_BASE, Base);
//    DE_GET_API(DE_API_CONSOLE, Con);
//    DE_GET_API(DE_API_DEFINITIONS, Def);
//    DE_GET_API(DE_API_FILE_SYSTEM, F);
//)

DE_ENTRYPOINT void *extension_importdeh_symbol(const char *name)
{
    DE_SYMBOL_PTR(name, deng_LibraryType)
    DE_SYMBOL_PTR(name, DP_Initialize);
    warning("\"%s\" not found in importdeh", name);
    return nullptr;
}
