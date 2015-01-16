/** @file dehread.cpp  DeHackEd patch reader plugin for Doomsday Engine.
 *
 * @authors Copyright © 2013-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2012-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "dehread.h"

#include <QDir>
#include <QFile>
#include <doomsday/filesys/lumpindex.h>
#include <de/App>
#include <de/Block>
#include <de/Log>
#include <de/String>

#include "dehreader.h"

using namespace de;

// Global handle on the engine's definition databases.
ded_t *ded;

// This is the original data before it gets replaced by any patches.
ded_sprid_t  origSpriteNames[NUMSPRITES];
ded_funcid_t origActionNames[NUMSTATES];

static void backupData()
{
    for(int i = 0; i < NUMSPRITES && i < ded->sprites.size(); i++)
    {
        qstrncpy(origSpriteNames[i].id, ded->sprites[i].id, DED_SPRITEID_LEN + 1);
    }

    for(int i = 0; i < NUMSTATES && i < ded->states.size(); i++)
    {
        qstrncpy(origActionNames[i], ded->states[i].action, DED_STRINGID_LEN + 1);
    }
}

static void readLump(LumpIndex const &lumpIndex, lumpnum_t lumpNum)
{
    if(0 > lumpNum || lumpNum >= lumpIndex.size())
    {
        LOG_AS("DehRead::readLump");
        LOG_WARNING("Invalid lump index #%i, ignoring.") << lumpNum;
        return;
    }

    File1 &lump = lumpIndex[lumpNum];
    size_t len  = lump.size();
    Block deh   = Block::fromRawData((char const *)lump.cache(), len);
    /// @attention Results in a deep-copy of the lump data into the Block
    ///            thus the cached lump can be released after this call.
    ///
    /// @todo Do not use a local buffer - read using QTextStream.
    deh.append(QChar(0));
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

static void readFile(String const &sourcePath, bool sourceIsCustom = true)
{
    QFile file(sourcePath);
    if(!file.open(QFile::ReadOnly | QFile::Text))
    {
        LOG_AS("DehRead::readFile");
        LOG_WARNING("Failed opening \"%s\" for read, aborting...") << QDir::toNativeSeparators(sourcePath);
        return;
    }

    /// @todo Do not use a local buffer.
    Block deh = file.readAll();
    deh.append(QChar(0));

    LOG_RES_MSG("Applying DeHackEd patch file \"%s\"%s")
            << NativePath(sourcePath).pretty()
            << (sourceIsCustom? " (custom)" : "");

    readDehPatch(deh, sourceIsCustom, IgnoreEOF);
}

static void readPatchLumps(LumpIndex const &lumpIndex)
{
    bool const readAll = DENG2_APP->commandLine().check("-alldehs");
    for(int i = lumpIndex.size() - 1; i >= 0; i--)
    {
        if(lumpIndex[i].name().fileNameExtension().toLower() == ".deh")
        {
            readLump(lumpIndex, i);
            if(!readAll) return;
        }
    }
}

static void readPatchFiles()
{
    CommandLine &cmdLine = DENG2_APP->commandLine();

    for(int p = 0; p < cmdLine.count(); ++p)
    {
        char const *arg = *(cmdLine.argv() + p);
        if(!cmdLine.matches("-deh", arg)) continue;

        while(++p != cmdLine.count() && !cmdLine.isOption(p))
        {
            cmdLine.makeAbsolutePath(p);
            readFile(NativePath(*(cmdLine.argv() + p)));
        }

        p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }
}

/**
 * This will be called after the engine has loaded all definitions but before
 * the data they contain has been initialized.
 */
int DefsHook(int /*hook_type*/, int /*parm*/, void *data)
{
    // Grab the DED definition handle supplied by the engine.
    ded = reinterpret_cast<ded_t *>(data);

    backupData();

    // Check for DEHACKED lumps.
    readPatchLumps(*reinterpret_cast<de::LumpIndex const *>(F_LumpIndex()));

    // Process all patch files specified with -deh options on the command line.
    readPatchFiles();

    return true;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize()
{
    Plug_AddHook(HOOK_DEFS, DefsHook);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
extern "C" char const *deng_LibraryType()
{
    return "deng-plugin/generic";
}

DENG_DECLARE_API(Base);
DENG_DECLARE_API(Con);
DENG_DECLARE_API(Def);
DENG_DECLARE_API(F);
DENG_DECLARE_API(Plug);

DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_BASE, Base);
    DENG_GET_API(DE_API_CONSOLE, Con);
    DENG_GET_API(DE_API_DEFINITIONS, Def);
    DENG_GET_API(DE_API_FILE_SYSTEM, F);
    DENG_GET_API(DE_API_PLUGIN, Plug);
)
