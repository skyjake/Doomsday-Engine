/** @file con_config.cpp  Config file IO.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "con_config.h"

#include <cctype>
#include <de/Log>
#include <de/Path>
#include <doomsday/help.h>
#include <doomsday/console/exec.h>
#include <doomsday/console/var.h>
#include <doomsday/console/alias.h>
#include <doomsday/console/knownword.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>

#include "dd_main.h"
#include "dd_def.h"
#include "m_misc.h"

#include "Games"

#ifdef __CLIENT__
#  include "clientapp.h"
#  include "BindContext"
#  include "ui/playerimpulse.h"
#endif

using namespace de;

static Path cfgFile;
static int flagsAllow;

static void writeHeaderComment(FILE *file)
{
    if(!App_GameLoaded())
    {
        fprintf(file, "# " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT "\n");
    }
    else
    {
        fprintf(file, "# %s %s / " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT "\n",
                (char *) gx.GetVariable(DD_PLUGIN_NAME),
                (char *) gx.GetVariable(DD_PLUGIN_VERSION_SHORT));
    }

    fprintf(file, "# This configuration file is generated automatically. Each line is a\n");
    fprintf(file, "# console command. Lines beginning with # are comments. Use autoexec.cfg\n");
    fprintf(file, "# for your own startup commands.\n\n");
}

static int writeVariableToFileWorker(knownword_t const *word, void *context)
{
    FILE *file = (FILE *)context;
    DENG_ASSERT(file != 0);

    cvar_t *var = (cvar_t *)word->data;
    DENG2_ASSERT(var != 0);

    // Don't archive this cvar?
    if(var->flags & CVF_NO_ARCHIVE)
        return 0;

    AutoStr const *path = CVar_ComposePath(var);

    // First print the comment (help text).
    if(char const *str = DH_GetString(DH_Find(Str_Text(path)), HST_DESCRIPTION))
    {
        M_WriteCommented(file, str);
    }

    fprintf(file, "%s ", Str_Text(path));
    if(var->flags & CVF_PROTECTED)
        fprintf(file, "force ");

    switch(var->type)
    {
    case CVT_BYTE:  fprintf(file, "%d", *(byte *) var->ptr); break;
    case CVT_INT:   fprintf(file, "%d", *(int *) var->ptr); break;
    case CVT_FLOAT: fprintf(file, "%s", M_TrimmedFloat(*(float *) var->ptr)); break;

    case CVT_CHARPTR:
        fprintf(file, "\"");
        if(CV_CHARPTR(var))
        {
            M_WriteTextEsc(file, CV_CHARPTR(var));
        }
        fprintf(file, "\"");
        break;

    case CVT_URIPTR:
        fprintf(file, "\"");
        if(CV_URIPTR(var))
        {
            fprintf(file, "%s", CV_URIPTR(var)->compose().toUtf8().constData());
        }
        fprintf(file, "\"");
        break;

    default: break;
    }
    fprintf(file, "\n\n");

    return 0; // Continue iteration.
}

static void writeVariablesToFile(FILE *file)
{
    Con_IterateKnownWords(0, WT_CVAR, writeVariableToFileWorker, file);
}

static int writeAliasToFileWorker(knownword_t const *word, void *context)
{
    FILE *file = (FILE *) context;
    DENG2_ASSERT(file != 0);

    calias_t *cal = (calias_t *) word->data;
    DENG2_ASSERT(cal != 0);

    fprintf(file, "alias \"");
    M_WriteTextEsc(file, cal->name);
    fprintf(file, "\" \"");
    M_WriteTextEsc(file, cal->command);
    fprintf(file, "\"\n");

    return 0; // Continue iteration.
}

static void writeAliasesToFile(FILE *file)
{
    Con_IterateKnownWords(0, WT_CALIAS, writeAliasToFileWorker, file);
}

static bool writeConsoleState(Path const &filePath)
{
    if(filePath.isEmpty()) return false;

    // Ensure the destination directory exists.
    String fileDir = filePath.toString().fileNamePath();
    if(!fileDir.isEmpty())
    {
        F_MakePath(fileDir.toUtf8().constData());
    }

    if(FILE *file = fopen(filePath.toUtf8().constData(), "wt"))
    {
        LOG_SCR_VERBOSE("Writing state to \"%s\"...")
                << NativePath(filePath).pretty();

        writeHeaderComment(file);
        fprintf(file, "#\n# CONSOLE VARIABLES\n#\n\n");
        writeVariablesToFile(file);

        fprintf(file, "\n#\n# ALIASES\n#\n\n");
        writeAliasesToFile(file);
        fclose(file);
        return true;
    }

    LOG_SCR_WARNING("Failed opening \"%s\" for writing")
            << NativePath(filePath).pretty();

    return false;
}

#ifdef __CLIENT__
static bool writeBindingsState(Path const &filePath)
{
    if(filePath.isEmpty()) return false;

    // Ensure the destination directory exists.
    String fileDir = filePath.toString().fileNamePath();
    if(!fileDir.isEmpty())
    {
        F_MakePath(fileDir.toUtf8().constData());
    }

    if(FILE *file = fopen(filePath.toUtf8().constData(), "wt"))
    {
        InputSystem &isys = ClientApp::inputSystem();

        LOG_SCR_VERBOSE("Writing bindings to \"%s\"...")
                << NativePath(filePath).pretty();

        writeHeaderComment(file);

        // Start with a clean slate when restoring the bindings.
        fprintf(file, "clearbindings\n\n");

        isys.forAllContexts([&isys, &file] (BindContext &context)
        {
            // Commands.
            context.forAllCommandBindings([&isys, &file, &context] (CommandBinding &bind)
            {
                fprintf(file, "bindevent \"%s:%s\" \"", context.name().toUtf8().constData(),
                               isys.composeBindsFor(bind).toUtf8().constData());
                M_WriteTextEsc(file, bind.command.toUtf8().constData());
                fprintf(file, "\"\n");
                return LoopContinue;
            });

            // Impulses.
            context.forAllImpulseBindings([&isys, &file, &context] (ImpulseBinding &bind)
            {
                PlayerImpulse const *impulse = P_ImpulsePtr(bind.impulseId);
                DENG2_ASSERT(impulse);

                fprintf(file, "bindcontrol local%i-%s \"%s\"\n",
                              bind.localPlayer + 1, impulse->name().toUtf8().constData(),
                              isys.composeBindsFor(bind).toUtf8().constData());
                return LoopContinue;
            });

            return LoopContinue;
        });

        fclose(file);
        return true;
    }

    LOG_SCR_WARNING("Failed opening \"%s\" for writing")
            << NativePath(filePath).pretty();

    return false;
}
#endif // __CLIENT__

static bool writeState(Path const &filePath, Path const &bindingsFileName = "")
{
    if(!filePath.isEmpty() && (flagsAllow & CPCF_ALLOW_SAVE_STATE))
    {
        writeConsoleState(filePath);
    }
#ifdef __CLIENT__
    if(!bindingsFileName.isEmpty() && (flagsAllow & CPCF_ALLOW_SAVE_BINDINGS))
    {
        // Bindings go into a separate file.
        writeBindingsState(bindingsFileName);
    }
#else
    DENG2_UNUSED(bindingsFileName);
#endif
    return true;
}

bool Con_ParseCommands(Path const &fileName, int flags)
{
    bool const setDefault = (flags & CPCF_SET_DEFAULT) != 0;

    // Is this supposed to be the default?
    if(setDefault)
    {
        cfgFile = fileName;
    }

    // Update the allowed operations.
    flagsAllow |= flags & (CPCF_ALLOW_SAVE_STATE | CPCF_ALLOW_SAVE_BINDINGS);

    LOG_SCR_VERBOSE("Parsing \"%s\" (setdef:%b)")
            << NativePath(fileName).pretty()
            << setDefault;

    return Con_Parse(fileName, setDefault /* => silently */);
}

void Con_SaveDefaults()
{
    writeState(cfgFile, (!isDedicated && App_GameLoaded()?
                             App_CurrentGame().bindingConfig() : ""));
}

D_CMD(WriteConsole)
{
    DENG2_UNUSED2(src, argc);
    Path filePath = Path(NativePath(argv[1]).expand().withSeparators('/'));

    LOG_SCR_MSG("Writing to \"%s\"...") << filePath;
    return !writeState(filePath);
}
