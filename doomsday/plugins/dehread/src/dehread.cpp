/** @file dehread.cpp  DeHackEd patch reader plugin for Doomsday Engine.
 * @ingroup dehread
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

#include <QDir>
#include <QFile>
#include <de/App>
#include <de/Block>
#include <de/Log>
#include <de/String>

#include "dehread.h"
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

static void readLump(lumpnum_t lumpNum)
{
    if(0 > lumpNum || lumpNum >= DD_GetInteger(DD_NUMLUMPS))
    {
        LOG_AS("DehRead::readLump");
        LOG_WARNING("Invalid lump index #%i, ignoring.") << lumpNum;
        return;
    }

    size_t len = W_LumpLength(lumpNum);
    Block deh  = Block::fromRawData((char const *)W_CacheLump(lumpNum), len);
    /// @attention Results in a deep-copy of the lump data into the Block
    ///            thus the cached lump can be released after this call.
    ///
    /// @todo Do not use a local buffer - read using QTextStream.
    deh.append(QChar(0));
    W_UnlockLump(lumpNum);

    LOG_RES_MSG("Applying DeHackEd patch lump #%i \"%s:%s\"")
            << lumpNum
            << NativePath(Str_Text(W_LumpSourceFile(lumpNum))).pretty()
            << Str_Text(W_LumpName(lumpNum));

    readDehPatch(deh, NoInclude | IgnoreEOF);
}

static void readFile(String const &filePath)
{
    QFile file(filePath);
    if(!file.open(QFile::ReadOnly | QFile::Text))
    {
        LOG_AS("DehRead::readFile");
        LOG_WARNING("Failed opening \"%s\" for read, aborting...") << QDir::toNativeSeparators(filePath);
        return;
    }

    /// @todo Do not use a local buffer.
    Block deh = file.readAll();
    deh.append(QChar(0));

    LOG_RES_MSG("Applying DeHackEd patch file \"%s\"")
            << NativePath(filePath).pretty();

    readDehPatch(deh, IgnoreEOF);
}

static void readPatchLumps()
{
    bool const readAll = DENG2_APP->commandLine().check("-alldehs");

    for(int i = DD_GetInteger(DD_NUMLUMPS) - 1; i >= 0; i--)
    {
        if(String(Str_Text(W_LumpName(i))).fileNameExtension().toLower() == ".deh")
        {
            readLump(i);
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
    readPatchLumps();

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
DENG_DECLARE_API(W);

DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_BASE, Base);
    DENG_GET_API(DE_API_CONSOLE, Con);
    DENG_GET_API(DE_API_DEFINITIONS, Def);
    DENG_GET_API(DE_API_FILE_SYSTEM, F);
    DENG_GET_API(DE_API_PLUGIN, Plug);
    DENG_GET_API(DE_API_WAD, W);
)
