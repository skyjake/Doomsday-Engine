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
#include <de/Writer>
#include <de/App>
#include <de/NativeFile>
#include <de/DirectoryFeed>
#include <de/c_wrapper.h>

#include <doomsday/help.h>
#include <doomsday/console/exec.h>
#include <doomsday/console/var.h>
#include <doomsday/console/alias.h>
#include <doomsday/console/knownword.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>
#include <doomsday/Games>

#include "dd_main.h"
#include "dd_def.h"
#include "m_misc.h"

#ifdef __CLIENT__
#  include "clientapp.h"

#  include "world/p_players.h"

#  include "BindContext"
#  include "CommandBinding"
#  include "ImpulseBinding"
#endif

using namespace de;

static Path cfgFile;
static int flagsAllow;

static String const STR_COMMENT = "# ";

static void writeHeaderComment(de::Writer &out)
{
    if(!App_GameLoaded())
    {
        out.writeText("# " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT "\n");
    }
    else
    {
        out.writeText(String::format("# %s %s / " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT "\n",
                (char const *) gx.GetVariable(DD_PLUGIN_NAME),
                (char const *) gx.GetVariable(DD_PLUGIN_VERSION_SHORT)));
    }

    out.writeText("# This configuration file is generated automatically. Each line is a\n"
                  "# console command. Lines beginning with # are comments. Use autoexec.cfg\n"
                  "# for your own startup commands.\n\n");
}

static int writeVariableToFileWorker(knownword_t const *word, void *context)
{
    de::Writer *out = reinterpret_cast<de::Writer *>(context);
    DENG_ASSERT(out != 0);

    cvar_t *var = (cvar_t *)word->data;
    DENG2_ASSERT(var != 0);

    // Don't archive this cvar?
    if(var->flags & CVF_NO_ARCHIVE)
        return 0;

    AutoStr const *path = CVar_ComposePath(var);

    // First print the comment (help text).
    if(char const *str = DH_GetString(DH_Find(Str_Text(path)), HST_DESCRIPTION))
    {
        out->writeText(String(str).addLinePrefix(STR_COMMENT) + "\n");
    }

    out->writeText(String::format("%s %s", Str_Text(path),
                                  var->flags & CVF_PROTECTED? "force " : ""));

    switch(var->type)
    {
    case CVT_BYTE:  out->writeText(String::format("%d", *(byte *) var->ptr)); break;
    case CVT_INT:   out->writeText(String::format("%d", *(int *) var->ptr)); break;
    case CVT_FLOAT: out->writeText(String::format("%s", M_TrimmedFloat(*(float *) var->ptr))); break;

    case CVT_CHARPTR:
        out->writeText("\"");
        if(CV_CHARPTR(var))
        {
            out->writeText(String(CV_CHARPTR(var)).escaped());
        }
        out->writeText("\"");
        break;

    case CVT_URIPTR:
        out->writeText("\"");
        if(CV_URIPTR(var))
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

static int writeAliasToFileWorker(knownword_t const *word, void *context)
{
    de::Writer *out = reinterpret_cast<de::Writer *>(context);
    DENG2_ASSERT(out != 0);

    calias_t *cal = (calias_t *) word->data;
    DENG2_ASSERT(cal != 0);

    out->writeText(String::format("alias \"%s\" \"%s\"\n",
                                  String(cal->name).escaped().toUtf8().constData(),
                                  String(cal->command).escaped().toUtf8().constData()));

    return 0; // Continue iteration.
}

static void writeAliasesToFile(de::Writer &out)
{
    Con_IterateKnownWords(0, WT_CALIAS, writeAliasToFileWorker, &out);
}

static bool writeConsoleState(Path const &filePath)
{
    if(filePath.isEmpty()) return false;

    // Ensure the destination directory exists.
    String fileDir = filePath.toString().fileNamePath();
    if(!fileDir.isEmpty())
    {
        F_MakePath(fileDir.toUtf8());
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

        file.flush();
    }
    catch(Error const &er)
    {
        LOG_SCR_WARNING("Failed to open \"%s\" for writing: %s")
                << filePath << er.asText();
        return false;
    }
    return true;
}

#ifdef __CLIENT__
static bool writeBindingsState(Path const &filePath)
{
    if(filePath.isEmpty()) return false;

    // Ensure the destination directory exists.
    String fileDir = filePath.toString().fileNamePath();
    if(!fileDir.isEmpty())
    {
        F_MakePath(fileDir.toUtf8());
    }

    try
    {
        File &file = App::rootFolder().replaceFile(filePath);
        de::Writer out(file);

        InputSystem &isys = ClientApp::inputSystem();

        LOG_SCR_VERBOSE("Writing input bindings to %s...") << file.description();

        writeHeaderComment(out);

        // Start with a clean slate when restoring the bindings.
        out.writeText("clearbindings\n\n");

        isys.forAllContexts([&isys, &out] (BindContext &context)
        {
            // Commands.
            context.forAllCommandBindings([&out, &context] (Record &rec)
            {
                CommandBinding bind(rec);
                out.writeText(String::format("bindevent \"%s:%s\" \"",
                                             context.name().toUtf8().constData(),
                                             bind.composeDescriptor().toUtf8().constData()) +
                              bind.gets("command").escaped() + "\"\n");
                return LoopContinue;
            });

            // Impulses.
            context.forAllImpulseBindings([&out, &context] (Record &rec)
            {
                ImpulseBinding bind(rec);
                PlayerImpulse const *impulse = P_PlayerImpulsePtr(bind.geti("impulseId"));
                DENG2_ASSERT(impulse);

                out.writeText(String::format("bindcontrol local%i-%s \"%s\"\n",
                              bind.geti("localPlayer") + 1,
                              impulse->name.toUtf8().constData(),
                              bind.composeDescriptor().toUtf8().constData()));
                return LoopContinue;
            });

            return LoopContinue;
        });

        file.flush();
        return true;
    }
    catch(Error const &er)
    {
        LOG_SCR_WARNING("Failed opening \"%s\" for writing: %s")
                << filePath << er.asText();
    }
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

void Con_SetAllowed(int flags)
{
    flagsAllow |= flags & (CPCF_ALLOW_SAVE_STATE | CPCF_ALLOW_SAVE_BINDINGS);
}

bool Con_ParseCommands(File const &file, int flags)
{
    LOG_SCR_MSG("Parsing console commands in %s...") << file.description();

    return Con_Parse(file, (flags & CPCF_SILENT) != 0);
}

bool Con_ParseCommands(NativePath const &nativePath, int flags)
{
    if(nativePath.exists())
    {
        std::unique_ptr<File> file(NativeFile::newStandalone(nativePath));
        return Con_ParseCommands(*file, flags);
    }
    return false;
}

void Con_SetDefaultPath(Path const &path)
{
    cfgFile = path;
}

void Con_SaveDefaults()
{
    Path path;

    if(CommandLine_CheckWith("-config", 1))
    {
        path = App::fileSystem().accessNativeLocation(CommandLine_NextAsPath(), File::Write);
    }
    else
    {
        path = cfgFile;
    }

    writeState(path, (!isDedicated && App_GameLoaded()?
                             App_CurrentGame().bindingConfig() : ""));
}

D_CMD(WriteConsole)
{
    DENG2_UNUSED2(src, argc);

    Path filePath(argv[1]);
    LOG_SCR_MSG("Writing to \"%s\"...") << filePath;
    return !writeState(filePath);
}
