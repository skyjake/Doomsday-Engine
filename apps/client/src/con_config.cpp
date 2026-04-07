/** @file con_config.cpp  Config file IO.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "ui/inputsystem.h"

#include <de/app.h>
#include <de/directoryfeed.h>
#include <de/filesystem.h>
#include <de/log.h>
#include <de/nativefile.h>
#include <de/path.h>
#include <de/writer.h>
#include <de/c_wrapper.h>
#include <cctype>

#include <doomsday/help.h>
#include <doomsday/console/exec.h>
#include <doomsday/console/var.h>
#include <doomsday/console/alias.h>
#include <doomsday/console/knownword.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>
#include <doomsday/games.h>

#include "dd_main.h"
#include "dd_def.h"

#ifdef __CLIENT__
#  include "clientapp.h"
#  include "world/p_players.h"
#  include "ui/bindcontext.h"
#  include "ui/commandbinding.h"
#  include "ui/impulsebinding.h"
#endif

using namespace de;

static Path cfgFile;
static int flagsAllow;

static void writeHeaderComment(de::Writer &out)
{
    if (!App_GameLoaded())
    {
        out.writeText("# " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT "\n");
    }
    else
    {
        out.writeText(Stringf("# %s %s / " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT "\n",
                (const char *) gx.GetPointer(DD_PLUGIN_NAME),
                (const char *) gx.GetPointer(DD_PLUGIN_VERSION_SHORT)));
    }

    out.writeText("# This configuration file is generated automatically. Each line is a\n"
                  "# console command. Lines beginning with # are comments. Use autoexec.cfg\n"
                  "# for your own startup commands.\n\n");
}

static int writeVariableToFileWorker(const knownword_t *word, void *context)
{
    de::Writer *out = reinterpret_cast<de::Writer *>(context);
    DE_ASSERT(out != 0);

    cvar_t *var = (cvar_t *)word->data;
    DE_ASSERT(var != 0);

    // Don't archive this cvar?
    if (var->flags & CVF_NO_ARCHIVE)
        return 0;

    const AutoStr *path = CVar_ComposePath(var);

    // First print the comment (help text).
    if (const char *str = DH_GetString(DH_Find(Str_Text(path)), HST_DESCRIPTION))
    {
        out->writeText(String(str).addLinePrefix("# ") + "\n");
    }

    out->writeText(Stringf("%s %s", Str_Text(path),
                                  var->flags & CVF_PROTECTED? "force " : ""));

    switch (var->type)
    {
    case CVT_BYTE:  out->writeText(Stringf("%d", *(byte *) var->ptr)); break;
    case CVT_INT:   out->writeText(Stringf("%d", *(int *) var->ptr)); break;
    case CVT_FLOAT: out->writeText(Stringf("%s", M_TrimmedFloat(*(float *) var->ptr))); break;

    case CVT_CHARPTR:
        out->writeText("\"");
        if (CV_CHARPTR(var))
        {
            out->writeText(String(CV_CHARPTR(var)).escaped());
        }
        out->writeText("\"");
        break;

    case CVT_URIPTR:
        out->writeText("\"");
        if (CV_URIPTR(var))
        {
            out->writeText(CV_URIPTR(var)->compose().escaped());
        }
        out->writeText("\"");
        break;

    default:
        break;
    }
    out->writeText("\n\n");

    return 0; // Continue iteration.
}

static void writeVariablesToFile(de::Writer &out)
{
    Con_IterateKnownWords(0, WT_CVAR, writeVariableToFileWorker, &out);
}

static int writeAliasToFileWorker(const knownword_t *word, void *context)
{
    de::Writer *out = reinterpret_cast<de::Writer *>(context);
    DE_ASSERT(out != 0);

    calias_t *cal = (calias_t *) word->data;
    DE_ASSERT(cal != 0);

    out->writeText(Stringf("alias \"%s\" \"%s\"\n",
                                  String(cal->name).escaped().c_str(),
                                  String(cal->command).escaped().c_str()));

    return 0; // Continue iteration.
}

static void writeAliasesToFile(de::Writer &out)
{
    Con_IterateKnownWords(0, WT_CALIAS, writeAliasToFileWorker, &out);
}

static bool writeConsoleState(const Path &filePath)
{
    if (filePath.isEmpty()) return false;

    // Ensure the destination directory exists.
    String fileDir = filePath.toString().fileNamePath();
    if (!fileDir.isEmpty())
    {
        F_MakePath(fileDir);
    }

    try
    {
        File &file = App::rootFolder().replaceFile(filePath);
        de::Writer out(file);

        LOG_SCR_VERBOSE("Writing console state to %s...") << file.description();

        writeHeaderComment(out);
        out.writeText("#\n# CONSOLE VARIABLES\n#\n\n");
        writeVariablesToFile(out);

        out.writeText("\n#\n# ALIASES\n#\n\n");
        writeAliasesToFile(out);
    }
    catch (const Error &er)
    {
        LOG_SCR_WARNING("Failed to open \"%s\" for writing: %s")
                << filePath << er.asText();
        return false;
    }
    return true;
}

#ifdef __CLIENT__
static bool writeBindingsState(const Path &filePath)
{
    if (filePath.isEmpty()) return false;

    // Ensure the destination directory exists.
    String fileDir = filePath.toString().fileNamePath();
    if (!fileDir.isEmpty())
    {
        F_MakePath(fileDir);
    }

    try
    {
        File &file = App::rootFolder().replaceFile(filePath);
        de::Writer out(file);

        InputSystem &isys = ClientApp::input();

        LOG_SCR_VERBOSE("Writing input bindings to %s...") << file.description();

        writeHeaderComment(out);

        // Start with a clean slate when restoring the bindings.
        out.writeText("clearbindings\n\n");

        isys.forAllContexts([&out] (BindContext &context)
        {
            // Commands.
            context.forAllCommandBindings([&out, &context] (Record &rec)
            {
                CommandBinding bind(rec);
                out.writeText(Stringf("bindevent \"%s:%s\" \"",
                                             context.name().c_str(),
                                             bind.composeDescriptor().c_str()) +
                              bind.gets("command").escaped() + "\"\n");
                return LoopContinue;
            });

            // Impulses.
            context.forAllImpulseBindings([&out] (CompiledImpulseBindingRecord &rec)
            {
                ImpulseBinding bind(rec);
                const PlayerImpulse *impulse = P_PlayerImpulsePtr(rec.compiled().impulseId);
                DE_ASSERT(impulse);

                out.writeText(Stringf("bindcontrol local%i-%s \"%s\"\n",
                              bind.geti("localPlayer") + 1,
                              impulse->name.c_str(),
                              bind.composeDescriptor().c_str()));
                return LoopContinue;
            });

            return LoopContinue;
        });
        return true;
    }
    catch (const Error &er)
    {
        LOG_SCR_WARNING("Failed opening \"%s\" for writing: %s")
                << filePath << er.asText();
    }
    return false;
}
#endif // __CLIENT__

static bool writeState(const Path &filePath, const Path &bindingsFileName = "")
{
    if (!filePath.isEmpty() && (flagsAllow & CPCF_ALLOW_SAVE_STATE))
    {
        writeConsoleState(filePath);
    }
#ifdef __CLIENT__
    if (!bindingsFileName.isEmpty() && (flagsAllow & CPCF_ALLOW_SAVE_BINDINGS))
    {
        // Bindings go into a separate file.
        writeBindingsState(bindingsFileName);
    }
#else
    DE_UNUSED(bindingsFileName);
#endif
    return true;
}

void Con_SetAllowed(int flags)
{
    if (flags != 0)
    {
        flagsAllow |= flags & (CPCF_ALLOW_SAVE_STATE | CPCF_ALLOW_SAVE_BINDINGS);
    }
    else
    {
        flagsAllow = 0;
    }
}

bool Con_ParseCommands(const File &file, int flags)
{
    LOG_SCR_MSG("Parsing console commands in %s...") << file.description();

    return Con_Parse(file, (flags & CPCF_SILENT) != 0);
}

bool Con_ParseCommands(const NativePath &nativePath, int flags)
{
    if (nativePath.exists())
    {
        std::unique_ptr<File> file(NativeFile::newStandalone(nativePath));
        return Con_ParseCommands(*file, flags);
    }
    return false;
}

void Con_SetDefaultPath(const Path &path)
{
    cfgFile = path;
}

void Con_SaveDefaults()
{
    Path path;

    if (CommandLine_CheckWith("-config", 1))
    {
        path = FS::accessNativeLocation(CommandLine_NextAsPath(), File::Write);
    }
    else
    {
        path = cfgFile;
    }

    writeState(path, (!isDedicated && App_GameLoaded()?
                             App_CurrentGame().bindingConfig() : ""));
    Con_MarkAsChanged(false);
}

void Con_SaveDefaultsIfChanged()
{
    if (DoomsdayApp::isGameLoaded() && Con_IsChanged())
    {
        Con_SaveDefaults();
    }
}

D_CMD(WriteConsole)
{
    DE_UNUSED(src, argc);

    Path filePath(argv[1]);
    LOG_SCR_MSG("Writing to \"%s\"...") << filePath;
    return !writeState(filePath);
}
