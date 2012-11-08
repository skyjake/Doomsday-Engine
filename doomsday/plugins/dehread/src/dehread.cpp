/**
 * @file dehread.cpp
 * DeHackEd patch reader plugin for Doomsday Engine. @ingroup dehread
 *
 * @author Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

static bool processAllPatchLumps; /// @c true= all lumps should be processed.

// This is the original data before it gets replaced by any patches.
static bool backedUpData;
ded_sprid_t  origSpriteNames[NUMSPRITES];
ded_funcid_t origActionNames[NUMSTATES];

// Global handle on the engine's definition databases.
ded_t* ded;

static bool recognisePatchLumpByName(lumpnum_t lumpNum)
{
    AutoStr* lumpName = W_LumpName(lumpNum);
    if(Str_IsEmpty(lumpName)) return false;

    const char* ext = F_FindFileExtension(Str_Text(lumpName));
    return (ext && !qstricmp(ext, "deh"));
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
    Block deh = Block::fromRawData((const char*)W_CacheLump(lumpNum), len);
    /// @attention Results in a deep-copy of the lump data into the Block
    ///            thus the cached lump can be released after this call.
    ///
    /// @todo Do not use a local buffer - read using QTextStream.
    deh.append(QChar(0));
    W_UnlockLump(lumpNum);

    Con_Message("Applying DeHackEd patch lump #%i \"%s:%s\"...\n", lumpNum,
                F_PrettyPath(Str_Text(W_LumpSourceFile(lumpNum))), Str_Text(W_LumpName(lumpNum)));

    readDehPatch(deh, NoInclude | IgnoreEOF);
}

static void readFile(const String& filePath)
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

    Con_Message("Applying DeHackEd patch file \"%s\"...\n", F_PrettyPath(filePath.toUtf8().constData()));

    readDehPatch(deh, IgnoreEOF);
}

static void processPatchLumps()
{
    for(int i = DD_GetInteger(DD_NUMLUMPS) - 1; i >= 0; i--)
    {
        if(!recognisePatchLumpByName(i)) continue;

        readLump(i);

        // Are we processing the first patch only?
        if(!processAllPatchLumps) break;
    }
}

static void processPatchFiles()
{
    CommandLine& cmdLine = DENG2_APP->commandLine();

    for(int p = 0; p < cmdLine.count(); ++p)
    {
        const char* arg = *(cmdLine.argv() + p);
        if(!cmdLine.matches("-deh", arg)) continue;

        while(++p != cmdLine.count() && !cmdLine.isOption(p))
        {
            cmdLine.makeAbsolutePath(p);
            const String filePath = String::fromNativePath(*(cmdLine.argv() + p));

            readFile(filePath);
        }

        p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }
}

static void backupData(void)
{
    // Already been here?
    if(backedUpData) return;

    for(int i = 0; i < NUMSPRITES && i < ded->count.sprites.num; i++)
        qstrncpy(origSpriteNames[i].id, ded->sprites[i].id, DED_SPRITEID_LEN + 1);

    for(int i = 0; i < NUMSTATES && i < ded->count.states.num; i++)
        qstrncpy(origActionNames[i], ded->states[i].action, DED_STRINGID_LEN + 1);

    backedUpData = true;
}

/**
 * This will be called after the engine has loaded all definitions but before
 * the data they contain has been initialized.
 */
int DefsHook(int /*hook_type*/, int /*parm*/, void* data)
{
    // Grab the DED definition handle supplied by the engine.
    ded = reinterpret_cast<ded_t*>(data);

    // Are we processing all lump patches?
    processAllPatchLumps = DENG2_APP->commandLine().check("-alldehs");

    backupData();

    // Check for DEHACKED lumps.
    processPatchLumps();

    // Process all patch files specified with -deh options on the command line.
    processPatchFiles();

    return true;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize(void)
{
    Plug_AddHook(HOOK_DEFS, DefsHook);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
extern "C" const char* deng_LibraryType(void)
{
    return "deng-plugin/generic";
}
