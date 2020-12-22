/** @file console.cpp  Console subsystem.
 *
 * @todo The Console subsystem should be rewritten to be a de::System and it
 * should use Doomsday Script as the underlying engine; everything should be
 * mapped to Doomsday Script processes, functions, variables, etc., making the
 * Console a mere convenience layer. -jk
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/console/exec.h"

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include <de/legacy/memory.h>
#include <de/charsymbols.h>
#include <de/app.h>
#include <de/dscript.h>
#include <de/log.h>
#include <de/logbuffer.h>
#include <de/nativefile.h>
#include <de/time.h>

#include "doomsday/doomsdayapp.h"
#include "doomsday/game.h"
#include "doomsday/console/knownword.h"
#include "doomsday/console/cmd.h"
#include "doomsday/console/var.h"
#include "doomsday/console/alias.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/fs_util.h"
#include "doomsday/uri.h"
#include "dd_share.h"

using namespace de;

#define SC_EMPTY_QUOTE  -1

// Length of the print buffer. Used in conPrintf and in Console_Message.
// If console messages are longer than this, an error will occur.
// Needed because we can't sizeof a malloc'd block.
#define PRBUFF_SIZE 0x2000

// Operators for the "if" command.
enum {
    IF_EQUAL,
    IF_NOT_EQUAL,
    IF_GREATER,
    IF_LESS,
    IF_GEQUAL,
    IF_LEQUAL
};

#define CMDTYPESTR(src) \
         (src == CMDS_DDAY? "a direct call" \
        : src == CMDS_GAME? "a game library call" \
        : src == CMDS_CONSOLE? "the console" \
        : src == CMDS_BIND? "a binding" \
        : src == CMDS_CONFIG? "a cfg file" \
        : src == CMDS_PROFILE? "a player profile" \
        : src == CMDS_CMDLINE? "the command line" \
        : src == CMDS_SCRIPT? "an action command" : "???")

typedef struct execbuff_s {
    dd_bool used;               // Is this in use?
    timespan_t when;            // System time when to execute the command.
    byte    source;             // Where the command came from
                                // (console input, a cfg file etc..)
    dd_bool isNetCmd;           // Command was sent over the net to us.
    char    subCmd[1024];       // A single command w/args.
} execbuff_t;

D_CMD(AddSub);
D_CMD(IncDec);
D_CMD(Alias);
D_CMD(Echo);
D_CMD(Help);
D_CMD(If);
D_CMD(Parse);
D_CMD(Quit);
D_CMD(Repeat);
D_CMD(Toggle);
D_CMD(Wait);
D_CMD(InspectMobj);
D_CMD(DebugCrash);
D_CMD(DebugError);
D_CMD(DoomsdayScript);

void initVariableBindings(Binder &);

static int executeSubCmd(const char *subCmd, byte src, dd_bool isNetCmd);
static void Con_SplitIntoSubCommands(const char *command,
                                     timespan_t markerOffset, byte src,
                                     dd_bool isNetCmd);

static void Con_ClearExecBuffer(void);

byte    ConsoleSilent = false;

static dd_bool      ConsoleInited;   // Has Con_Init() been called?
static Binder       consoleBinder;
static execbuff_t * exBuff;
static int          exBuffSize;
static execbuff_t * curExec;
static bool         consoleChanged = false;

void Con_Register(void)
{
    C_CMD("add",            NULL,   AddSub);
    C_CMD("after",          "is",   Wait);
    C_CMD("alias",          NULL,   Alias);
    C_CMD("dec",            NULL,   IncDec);
    C_CMD("echo",           "s*",   Echo);
    C_CMD("print",          "s*",   Echo);
    C_CMD("exec",           "s*",   Parse);
    C_CMD("if",             NULL,   If);
    C_CMD("inc",            NULL,   IncDec);
    C_CMD("repeat",         "ifs",  Repeat);
    C_CMD("sub",            NULL,   AddSub);
    C_CMD("toggle",         "s",    Toggle);
#ifdef _DEBUG
    C_CMD("crash",          NULL,   DebugCrash);
#endif
    C_CMD("ds",             "s*",   DoomsdayScript);

    Con_DataRegister();
}

static void PrepareCmdArgs(cmdargs_t *cargs, const char *lpCmdLine)
{
#define IS_ESC_CHAR(x)  ((x) == '"' || (x) == '\\' || (x) == '{' || (x) == '}')

    size_t              i, len = strlen(lpCmdLine);

    // Prepare the data.
    memset(cargs, 0, sizeof(cmdargs_t));
    strcpy(cargs->cmdLine, lpCmdLine);

    // Prepare.
    for (i = 0; i < len; ++i)
    {
        // Whitespaces are separators.
        if (DE_ISSPACE(cargs->cmdLine[i]))
            cargs->cmdLine[i] = 0;

        if (cargs->cmdLine[i] == '\\' && IS_ESC_CHAR(cargs->cmdLine[i + 1]))
        {   // Escape sequence.
            memmove(cargs->cmdLine + i, cargs->cmdLine + i + 1,
                    sizeof(cargs->cmdLine) - i - 1);
            len--;
            continue;
        }

        if (cargs->cmdLine[i] == '"')
        {   // Find the end.
            size_t              start = i;

            cargs->cmdLine[i] = 0;
            for (++i; i < len && cargs->cmdLine[i] != '"'; ++i)
            {
                if (cargs->cmdLine[i] == '\\' && IS_ESC_CHAR(cargs->cmdLine[i + 1])) // Escape sequence?
                {
                    memmove(cargs->cmdLine + i, cargs->cmdLine + i + 1,
                            sizeof(cargs->cmdLine) - i - 1);
                    len--;
                    continue;
                }
            }

            // Quote not terminated?
            if (i == len)
                break;

            // An empty set of quotes?
            if (i == start + 1)
                cargs->cmdLine[i] = SC_EMPTY_QUOTE;
            else
                cargs->cmdLine[i] = 0;
        }

        if (cargs->cmdLine[i] == '{')
        {   // Find matching end, braces are another notation for quotes.
            int             level = 0;
            size_t          start = i;

            cargs->cmdLine[i] = 0;
            for (++i; i < len; ++i)
            {
                if (cargs->cmdLine[i] == '\\' && IS_ESC_CHAR(cargs->cmdLine[i + 1])) // Escape sequence?
                {
                    memmove(cargs->cmdLine + i, cargs->cmdLine + i + 1,
                            sizeof(cargs->cmdLine) - i - 1);
                    len--;
                    i++;
                    continue;
                }

                if (cargs->cmdLine[i] == '}')
                {
                    if (!level)
                        break;

                    level--;
                }

                if (cargs->cmdLine[i] == '{')
                    level++;
            }

            // Quote not terminated?
            if (i == len)
                break;

            // An empty set of braces?
            if (i == start + 1)
                cargs->cmdLine[i] = SC_EMPTY_QUOTE;
            else
                cargs->cmdLine[i] = 0;
        }
    }

    // Scan through the cmdLine and get the beginning of each token.
    cargs->argc = 0;
    for (i = 0; i < len; ++i)
    {
        if (!cargs->cmdLine[i])
            continue;

        // Is this an empty quote?
        if (cargs->cmdLine[i] == char(SC_EMPTY_QUOTE))
            cargs->cmdLine[i] = 0;  // Just an empty string.

        cargs->argv[cargs->argc++] = cargs->cmdLine + i;
        i += strlen(cargs->cmdLine + i);
    }

#undef IS_ESC_CHAR
}

static Value *Function_Console_ListVars(Context &, const Function::ArgumentValues &args)
{
    StringList vars;
    Con_TermsRegex(vars, args.at(0)->asText(), WT_CVAR);

    std::unique_ptr<ArrayValue> result(new ArrayValue);
    for (String v : vars)
    {
        *result << new TextValue(v);
    }
    return result.release();
}

dd_bool Con_Init(void)
{
    if (ConsoleInited) return true;

    LOG_SCR_VERBOSE("Initializing the console...");

    // Doomsday Script bindings to access console features.
    /// @todo Some of these should become obsolete once cvars/cmds are moved to
    /// DS records.
    consoleBinder.initNew();
    initVariableBindings(consoleBinder);
    consoleBinder << DE_FUNC(Console_ListVars, "listVars", "pattern");
    App::scriptSystem().addNativeModule("Console", consoleBinder.module());

    exBuff = NULL;
    exBuffSize = 0;

    ConsoleInited = true;

    return true;
}

void Con_Shutdown(void)
{
    if (!ConsoleInited) return;

    LOG_SCR_VERBOSE("Shutting down the console...");

    Con_ClearExecBuffer();
    Con_ShutdownDatabases();
    consoleBinder.deinit();

    ConsoleInited = false;
}

void Con_MarkAsChanged(dd_bool changed)
{
    consoleChanged = changed;
}

dd_bool Con_IsChanged()
{
    return consoleChanged;
}

#if 0 // should use libshell!
/**
 * Send a console command to the server.
 * This shouldn't be called unless we're logged in with the right password.
 */
static void Con_Send(const char *command, byte src, int silent)
{
    ushort len = (ushort) strlen(command);

    LOG_AS("Con_Send");
    if (len >= 0x8000)
    {
        LOGDEV_NET_ERROR("Command is too long, length=%i") << len;
        return;
    }

    Msg_Begin(PKT_COMMAND2);
    // Mark high bit for silent commands.
    Writer_WriteUInt16(msgWriter, len | (silent ? 0x8000 : 0));
    Writer_WriteUInt16(msgWriter, 0); // flags. Unused at present.
    Writer_WriteByte(msgWriter, src);
    Writer_Write(msgWriter, command, len);
    Msg_End();
    Net_SendBuffer(0, 0);
}
#endif

static void Con_QueueCmd(const char *singleCmd, timespan_t atSecond,
                         byte source, dd_bool isNetCmd)
{
    execbuff_t *ptr = NULL;
    int     i;

    // Look for an empty spot.
    for (i = 0; i < exBuffSize; ++i)
        if (!exBuff[i].used)
        {
            ptr = exBuff + i;
            break;
        }

    if (ptr == NULL)
    {
        // No empty places, allocate a new one.
        exBuff = (execbuff_t *) M_Realloc(exBuff, sizeof(execbuff_t) * ++exBuffSize);
        ptr = exBuff + exBuffSize - 1;
    }
    ptr->used = true;
    strcpy(ptr->subCmd, singleCmd);
    ptr->when = atSecond;
    ptr->source = source;
    ptr->isNetCmd = isNetCmd;
}

static void Con_ClearExecBuffer(void)
{
    M_Free(exBuff);
    exBuff = NULL;
    exBuffSize = 0;
}

/**
 * The execbuffer is used to schedule commands for later.
 *
 * @return          @c false, if an executed command fails.
 */
static dd_bool Con_CheckExecBuffer(void)
{
#define BUFFSIZE 1024 /// @todo Rewrite all of this; use de::String. -jk

    dd_bool ret = true;
    int     i;
    char    storage[BUFFSIZE];

    storage[255] = 0;

    const TimeSpan now = TimeSpan::sinceStartOfProcess();

    // Execute the commands whose time has come.
    for (i = 0; i < exBuffSize; ++i)
    {
        execbuff_t *ptr = exBuff + i;

        if (!ptr->used || ptr->when > now)
            continue;

        // We'll now execute this command.
        curExec = ptr;
        ptr->used = false;
        strncpy(storage, ptr->subCmd, BUFFSIZE - 1);

        const bool isInteractive =
                (ptr->source == CMDS_CONSOLE ||
                 ptr->source == CMDS_CMDLINE);
        if (isInteractive)
        {
            LOG().beginInteractive();
        }

        if (!executeSubCmd(storage, ptr->source, ptr->isNetCmd))
        {
            ret = false;
        }

        if (isInteractive)
        {
            LOG().endInteractive();
        }
    }

    return ret;
#undef BUFFSIZE
}

void Con_Ticker(timespan_t /*time*/)
{
    Con_CheckExecBuffer();
}

/**
 * expCommand gets reallocated in the expansion process.
 * This could be bit more clever.
 */
static void expandWithArguments(char **expCommand, cmdargs_t *args)
{
    int             p;
    char           *text = *expCommand;
    size_t          i, off, size = strlen(text) + 1;

    for (i = 0; text[i]; ++i)
    {
        if (text[i] == '%' && (text[i + 1] >= '1' && text[i + 1] <= '9'))
        {
            char           *substr;
            int             aidx = text[i + 1] - '1' + 1;

            // Expand! (or delete)
            if (aidx > args->argc - 1)
                substr = (char *) "";
            else
                substr = args->argv[aidx];

            // First get rid of the %n.
            memmove(text + i, text + i + 2, size - i - 2);
            // Reallocate.
            off = strlen(substr);
            text = *expCommand = (char *) M_Realloc(*expCommand, size += off - 2);
            if (off)
            {
                // Make room for the insert.
                memmove(text + i + off, text + i, size - i - off);
                memcpy(text + i, substr, off);
            }
            i += off - 1;
        }
        else if (text[i] == '%' && text[i + 1] == '0')
        {
            // First get rid of the %0.
            memmove(text + i, text + i + 2, size - i - 2);
            text = *expCommand = (char *) M_Realloc(*expCommand, size -= 2);
            for (p = args->argc - 1; p > 0; p--)
            {
                off = strlen(args->argv[p]) + 1;
                text = *expCommand = (char *) M_Realloc(*expCommand, size += off);
                memmove(text + i + off, text + i, size - i - off);
                text[i] = ' ';
                memcpy(text + i + 1, args->argv[p], off - 1);
            }
        }
    }
}

/**
 * The command is executed forthwith!!
 */
static int executeSubCmd(const char *subCmd, byte src, dd_bool isNetCmd)
{
    cmdargs_t args;
    ccmd_t *ccmd;
    cvar_t *cvar;
    calias_t *cal;

    PrepareCmdArgs(&args, subCmd);
    if (!args.argc)
        return true;

    // Try to find a matching console command.
    ccmd = Con_FindCommandMatchArgs(&args);
    if (ccmd != NULL)
    {
        // Found a match. Are we allowed to execute?
        dd_bool canExecute = true;

        // Trying to issue a command requiring a loaded game?
        // dj: This should be considered a short-term solution. Ideally we want some namespacing mechanics.
        if ((ccmd->flags & CMDF_NO_NULLGAME) && DoomsdayApp::game().isNull())
        {
            LOG_SCR_ERROR("Execution of command '%s' is only allowed when a game is loaded") << ccmd->name;
            return true;
        }

        /// @todo Access control needs revising. -jk
#if 0
        // A dedicated server, trying to execute a ccmd not available to us?
        if (isDedicated && (ccmd->flags & CMDF_NO_DEDICATED))
        {
            LOG_SCR_ERROR("Execution of command '%s' not possible in dedicated mode") << ccmd->name;
            return true;
        }

        // Net commands sent to servers have extra protection.
        if (isServer && isNetCmd)
        {
            // Is the command permitted for use by clients?
            if (ccmd->flags & CMDF_CLIENT)
            {
                LOG_NET_ERROR("Execution of command '%s' blocked (client attempted invocation);"
                              "this command is not permitted for use by clients") << ccmd->name;
                return true;
            }

            // Are ANY commands from this (remote) src permitted for use
            // by our clients?

            // NOTE:
            // This is an interim measure to protect against abuse of the
            // most vulnerable invocation methods.
            // Once all console commands are updated with the correct usage
            // flags we can then remove these restrictions or make them
            // optional for servers.
            //
            // The next step will then be allowing select console commands
            // to be executed by non-logged in clients.
            switch (src)
            {
            case CMDS_UNKNOWN:
            case CMDS_CONFIG:
            case CMDS_PROFILE:
            case CMDS_CMDLINE:
            case CMDS_SCRIPT:
                LOG_NET_ERROR("Execution of command '%s' blocked (client attempted invocation via %s); "
                              "this method is not permitted by clients") << ccmd->name << CMDTYPESTR(src);
                return true;

            default:
                break;
            }
        }
#endif

        // Is the src permitted for this command?
        switch (src)
        {
        case CMDS_UNKNOWN:
            canExecute = false;
            break;

        case CMDS_DDAY:
            if (ccmd->flags & CMDF_DDAY)
                canExecute = false;
            break;

        case CMDS_GAME:
            if (ccmd->flags & CMDF_GAME)
                canExecute = false;
            break;

        case CMDS_CONSOLE:
            if (ccmd->flags & CMDF_CONSOLE)
                canExecute = false;
            break;

        case CMDS_BIND:
            if (ccmd->flags & CMDF_BIND)
                canExecute = false;
            break;

        case CMDS_CONFIG:
            if (ccmd->flags & CMDF_CONFIG)
                canExecute = false;
            break;

        case CMDS_PROFILE:
            if (ccmd->flags & CMDF_PROFILE)
                canExecute = false;
            break;

        case CMDS_CMDLINE:
            if (ccmd->flags & CMDF_CMDLINE)
                canExecute = false;
            break;

        case CMDS_SCRIPT:
            if (ccmd->flags & CMDF_DED)
                canExecute = false;
            break;

        default:
            return true;
        }

        if (!canExecute)
        {
            LOG_SCR_ERROR("'%s' cannot be executed via %s") << ccmd->name << CMDTYPESTR(src);
            return true;
        }

        if (canExecute)
        {
            /**
             * Execute the command!
             * \note Console command execution may invoke a full update of the
             * console databases; thus the @c ccmd pointer may be invalid after
             * this call.
             */
            int result;
            if ((result = ccmd->execFunc(src, args.argc, args.argv)) == false)
            {
                LOG_SCR_ERROR("'%s' failed") << args.argv[0];
            }
            return result;
        }
        return true;
    }

    // Then try the cvars?
    cvar = Con_FindVariable(args.argv[0]);
    if (cvar != NULL)
    {
        dd_bool outOfRange = false, setting = false, hasCallback;

        /**
         * \note Change notification callback execution may invoke
         * a full update of the console databases; thus the @c cvar
         * pointer may be invalid once a callback executes.
         */
        hasCallback = (cvar->notifyChanged != 0);

        if (args.argc == 2 ||
           (args.argc == 3 && !iCmpStrCase(args.argv[1], "force")))
        {
            char* argptr = args.argv[args.argc - 1];
            dd_bool forced = args.argc == 3;

            setting = true;
            if (cvar->flags & CVF_READ_ONLY)
            {
                CVar_PrintReadOnlyWarning(cvar);
            }
            else if ((cvar->flags & CVF_PROTECTED) && !forced)
            {
                AutoStr* name = CVar_ComposePath(cvar);
                LOG_SCR_NOTE("%s is protected; you shouldn't change its value -- "
                             "use the command: " _E(b) "'%s force %s'" _E(.) " to modify it anyway")
                        << Str_Text(name) << Str_Text(name) << argptr;
            }
            else
            {
                Con_MarkAsChanged(true);

                switch (cvar->type)
                {
                case CVT_BYTE: {
                    byte val = (byte) strtol(argptr, NULL, 0);
                    if (!forced &&
                       ((!(cvar->flags & CVF_NO_MIN) && val < cvar->min) ||
                        (!(cvar->flags & CVF_NO_MAX) && val > cvar->max)))
                        outOfRange = true;
                    else
                        CVar_SetInteger(cvar, val);
                    break;
                  }
                case CVT_INT: {
                    int val = strtol(argptr, NULL, 0);
                    if (!forced &&
                       ((!(cvar->flags & CVF_NO_MIN) && val < cvar->min) ||
                        (!(cvar->flags & CVF_NO_MAX) && val > cvar->max)))
                        outOfRange = true;
                    else
                        CVar_SetInteger(cvar, val);
                    break;
                  }
                case CVT_FLOAT: {
                    float val = strtod(argptr, NULL);
                    if (!forced &&
                       ((!(cvar->flags & CVF_NO_MIN) && val < cvar->min) ||
                        (!(cvar->flags & CVF_NO_MAX) && val > cvar->max)))
                        outOfRange = true;
                    else
                        CVar_SetFloat(cvar, val);
                    break;
                  }
                case CVT_CHARPTR:
                    CVar_SetString(cvar, argptr);
                    break;
                case CVT_URIPTR:
                    /// @todo Sanitize and validate against known schemas.
                    CVar_SetUri(cvar, res::makeUri(argptr));
                    break;
                default: break;
                }
            }
        }

        if (outOfRange)
        {
            AutoStr* name = CVar_ComposePath(cvar);
            if (!(cvar->flags & (CVF_NO_MIN | CVF_NO_MAX)))
            {
                char temp[20];
                strcpy(temp, M_TrimmedFloat(cvar->min));
                LOG_SCR_ERROR("%s <= %s <= %s") << temp << Str_Text(name) << M_TrimmedFloat(cvar->max);
            }
            else if (cvar->flags & CVF_NO_MAX)
            {
                LOG_SCR_ERROR("%s >= %s") << Str_Text(name) << M_TrimmedFloat(cvar->min);
            }
            else
            {
                LOG_SCR_ERROR("%s <= %s") << Str_Text(name) << M_TrimmedFloat(cvar->max);
            }
        }
        else if (!setting) // Show the value.
        {
            if (setting && hasCallback)
            {
                // Lookup the cvar again - our pointer may have been invalidated.
                cvar = Con_FindVariable(args.argv[0]);
            }

            if (cvar)
            {
                // It still exists.
                Con_PrintCVar(cvar, "");
            }
        }
        return true;
    }

    // How about an alias then?
    cal = Con_FindAlias(args.argv[0]);
    if (cal != NULL)
    {
        char* expCommand;

        // Expand the command with arguments.
        expCommand = (char *) M_Malloc(strlen(cal->command) + 1);
        strcpy(expCommand, cal->command);
        expandWithArguments(&expCommand, &args);
        // Do it, man!
        Con_SplitIntoSubCommands(expCommand, 0, src, isNetCmd);
        M_Free(expCommand);
        return true;
    }

    // What *is* that?
    if (Con_FindCommand(args.argv[0]))
    {
        LOG_SCR_WARNING("%s: command arguments invalid") << args.argv[0];
        Con_Executef(CMDS_DDAY, false, "help %s", args.argv[0]);
    }
    else
    {
        LOG_SCR_MSG("%s: unknown identifier") << args.argv[0];
    }
    return false;
}

/**
 * Splits the command into subcommands and queues them into the
 * execution buffer.
 */
static void Con_SplitIntoSubCommands(const char *command,
                                     timespan_t markerOffset, byte src,
                                     dd_bool isNetCmd)
{
#define BUFFSIZE        2048

    char            subCmd[BUFFSIZE];
    int             inQuotes = false, escape = false;
    size_t          gPos = 0, scPos = 0, len;

    // Is there a command to execute?
    if (!command || command[0] == 0)
        return;

    // Jump over initial semicolons.
    len = strlen(command);
    while (gPos < len && command[gPos] == ';' && command[gPos] != 0)
        gPos++;

    subCmd[0] = subCmd[BUFFSIZE-1] = 0;

    // The command may actually contain many commands, separated
    // with semicolons. This isn't a very clear algorithm...
    for (; command[gPos];)
    {
        escape = false;
        if (inQuotes && command[gPos] == '\\')   // Escape sequence?
        {
            subCmd[scPos++] = command[gPos++];
            escape = true;
        }
        if (command[gPos] == '"' && !escape)
            inQuotes = !inQuotes;

        // Collect characters.
        subCmd[scPos++] = command[gPos++];
        if (subCmd[0] == ' ')
            scPos = 0;          // No spaces in the beginning.

        if ((command[gPos] == ';' && !inQuotes) || command[gPos] == 0)
        {
            while (gPos < len && command[gPos] == ';' && command[gPos] != 0)
                gPos++;
            // The subcommand ends.
            subCmd[scPos] = 0;
        }
        else
        {
            continue;
        }

        // Queue it.
        Con_QueueCmd(subCmd, TimeSpan::sinceStartOfProcess() + markerOffset, src, isNetCmd);

        scPos = 0;
    }

#undef BUFFSIZE
}

int Con_Execute(byte src, const char *command, int silent, dd_bool netCmd)
{
    if (silent)
    {
        ConsoleSilent = true;
    }

    Con_SplitIntoSubCommands(command, 0, src, netCmd);
    int ret = Con_CheckExecBuffer();

    if (silent)
    {
        ConsoleSilent = false;
    }
    return ret;
}

int Con_Executef(byte src, int silent, const char *command, ...)
{
    va_list         argptr;
    char            buffer[4096];

    va_start(argptr, command);
    dd_vsnprintf(buffer, sizeof(buffer), command, argptr);
    va_end(argptr);
    return Con_Execute(src, buffer, silent, false);
}

bool Con_Parse(const File &file, bool silently)
{
    Block utf8;
    file >> utf8;

    String contents = String::fromUtf8(utf8);

    // This file is filled with console commands.
    int currentLine = 1;
    for (const auto &lineRef : contents.splitRef("\n"))
    {
        // Each line is a command.
        const String line = String(lineRef).leftStrip();
        if (!line.isEmpty() && line.first() != '#')
        {
            // Execute the commands silently.
            if (!Con_Execute(CMDS_CONFIG, line, silently, false))
            {
                if (!silently)
                {
                    LOG_SCR_WARNING("%s (line %i): error executing command \"%s\"")
                            << file.description()
                            << currentLine
                            << line;
                }
            }
        }
        currentLine += 1;
    }

    Con_MarkAsChanged(false);
    return true;
}

/**
 * Create an alias.
 */
static void makeAlias(char *aName, char *command)
{
    calias_t *cal = Con_FindAlias(aName);
    dd_bool remove = false;

    // Will we remove this alias?
    if (command == NULL)
        remove = true;
    else if (command[0] == 0)
        remove = true;

    if (cal && remove) // This alias will be removed.
    {
        Con_DeleteAlias(cal);
        return; // We're done.
    }

    // Does the alias already exist?
    if (cal)
    {
        cal->command = (char *) M_Realloc(cal->command, strlen(command) + 1);
        strcpy(cal->command, command);
        return;
    }

    // We need to create a new alias.
    Con_AddAlias(aName, command);
}

D_CMD(Alias)
{
    DE_UNUSED(src);

    if (argc != 3 && argc != 2)
    {
        LOG_SCR_NOTE("Usage: %s (alias) (cmd)") << argv[0];
        LOG_SCR_MSG("Example: alias bigfont \"font size 3\"");
        LOG_SCR_MSG("Use %%1-%%9 to pass the alias arguments to the command.");
        return true;
    }

    makeAlias(argv[1], argc == 3 ? argv[2] : NULL);
    if (argc != 3)
    {
        LOG_SCR_MSG("Alias '%s' deleted") << argv[1];
    }
    return true;
}

D_CMD(Parse)
{
    DE_UNUSED(src);

    int     i;

    for (i = 1; i < argc; ++i)
    {
        try
        {
            LOG_SCR_MSG("Parsing \"%s\"") << argv[i];
            std::unique_ptr<NativeFile> file(
                        NativeFile::newStandalone(App::app().nativeHomePath() / NativePath(argv[i])));
            Con_Parse(*file, false /*not silent*/);
        }
        catch (const Error &er)
        {
            LOG_SCR_ERROR("Failed to parse \"%s\": %s")
                    << argv[i] << er.asText();
        }
    }
    return true;
}

D_CMD(Wait)
{
    DE_UNUSED(src, argc);

    timespan_t offset;

    offset = strtod(argv[1], NULL) / 35;    // Offset in seconds.
    if (offset < 0)
        offset = 0;
    Con_SplitIntoSubCommands(argv[2], offset, CMDS_CONSOLE, false);
    return true;
}

D_CMD(Repeat)
{
    DE_UNUSED(src, argc);

    int     count;
    timespan_t interval, offset;

    count = atoi(argv[1]);
    interval = strtod(argv[2], NULL) / 35;  // In seconds.
    offset = 0;
    while (count-- > 0)
    {
        offset += interval;
        Con_SplitIntoSubCommands(argv[3], offset, CMDS_CONSOLE, false);
    }
    return true;
}

D_CMD(Echo)
{
    DE_UNUSED(src);

    int     i;

    for (i = 1; i < argc; ++i)
    {
        LOG_MSG("%s") << argv[i];
    }
    return true;
}

static dd_bool cvarAddSub(const char* name, float delta, dd_bool force)
{
    cvar_t* cvar = Con_FindVariable(name);
    float val;

    if (!cvar)
    {
        if (name && name[0])
            LOG_SCR_ERROR("%s is not a known cvar") << name;
        return false;
    }

    if (cvar->flags & CVF_READ_ONLY)
    {
        CVar_PrintReadOnlyWarning(cvar);
        return false;
    }

    val = CVar_Float(cvar) + delta;
    if (!force)
    {
        if (!(cvar->flags & CVF_NO_MAX) && val > cvar->max)
            val = cvar->max;
        if (!(cvar->flags & CVF_NO_MIN) && val < cvar->min)
            val = cvar->min;
    }
    CVar_SetFloat(cvar, val);
    return true;
}

/**
 * Rather messy, wouldn't you say?
 */
D_CMD(AddSub)
{
    DE_UNUSED(src);

    dd_bool             force = false;
    float               delta = 0;

    if (argc <= 2)
    {
        LOG_SCR_NOTE("Usage: %s (cvar) (val) (force)") << argv[0];
        LOG_SCR_MSG("Use force to make cvars go off limits.");
        return true;
    }
    if (argc >= 4)
    {
        force = !iCmpStrCase(argv[3], "force");
    }

    delta = strtod(argv[2], NULL);
    if (!iCmpStrCase(argv[0], "sub"))
        delta = -delta;

    return cvarAddSub(argv[1], delta, force);
}

/**
 * Rather messy, wouldn't you say?
 */
D_CMD(IncDec)
{
    DE_UNUSED(src);

    dd_bool force = false;
    cvar_t* cvar;
    float val;

    if (argc == 1)
    {
        LOG_SCR_NOTE("Usage: %s (cvar) (force)") << argv[0];
        LOG_SCR_MSG("Use force to make cvars go off limits.");
        return true;
    }
    if (argc >= 3)
    {
        force = !iCmpStrCase(argv[2], "force");
    }
    cvar = Con_FindVariable(argv[1]);
    if (!cvar)
        return false;

    if (cvar->flags & CVF_READ_ONLY)
    {
        LOG_SCR_ERROR("%s (cvar) is read-only, it cannot be changed (even with force)") << argv[1];
        return false;
    }

    val = CVar_Float(cvar);
    val += !iCmpStrCase(argv[0], "inc")? 1 : -1;

    if (!force)
    {
        if (!(cvar->flags & CVF_NO_MAX) && val > cvar->max)
            val = cvar->max;
        if (!(cvar->flags & CVF_NO_MIN) && val < cvar->min)
            val = cvar->min;
    }

    CVar_SetFloat(cvar, val);
    return true;
}

/**
 * Toggle the value of a variable between zero and nonzero.
 */
D_CMD(Toggle)
{
    DE_UNUSED(src, argc);

    cvar_t *cvar = Con_FindVariable(argv[1]);
    if (!cvar) return false;

    CVar_SetInteger(cvar, CVar_Integer(cvar)? 0 : 1);
    return true;
}

/**
 * Execute a command if the condition passes.
 */
D_CMD(If)
{
    struct {
        const char *opstr;
        uint    op;
    } operators[] =
    {
        {"not", IF_NOT_EQUAL},
        {"=",   IF_EQUAL},
        {">",   IF_GREATER},
        {"<",   IF_LESS},
        {">=",  IF_GEQUAL},
        {"<=",  IF_LEQUAL},
        {NULL,  0}
    };
    uint        i, oper = IF_EQUAL;
    cvar_t     *var;
    dd_bool     isTrue = false;

    if (argc != 5 && argc != 6)
    {
        LOG_SCR_NOTE("Usage: %s (cvar) (operator) (value) (cmd) (else-cmd)") << argv[0];
        LOG_SCR_MSG("Operator must be one of: not, =, >, <, >=, <=");
        LOG_SCR_MSG("The (else-cmd) can be omitted.");
        return true;
    }

    var = Con_FindVariable(argv[1]);
    if (!var)
        return false;

    // Which operator?
    for (i = 0; operators[i].opstr; ++i)
        if (!iCmpStrCase(operators[i].opstr, argv[2]))
        {
            oper = operators[i].op;
            break;
        }
    if (!operators[i].opstr)
        return false;           // Bad operator.

    // Value comparison depends on the type of the variable.
    switch (var->type)
    {
    case CVT_BYTE:
    case CVT_INT:
        {
        int value = (var->type == CVT_INT ? CV_INT(var) : CV_BYTE(var));
        int test = strtol(argv[3], 0, 0);

        isTrue = (oper == IF_EQUAL     ? value == test :
                  oper == IF_NOT_EQUAL ? value != test :
                  oper == IF_GREATER   ? value >  test :
                  oper == IF_LESS      ? value <  test :
                  oper == IF_GEQUAL    ? value >= test :
                                         value <= test);
        break;
        }
    case CVT_FLOAT:
        {
        float value = CV_FLOAT(var);
        float test = strtod(argv[3], 0);

        isTrue = (oper == IF_EQUAL     ? value == test :
                  oper == IF_NOT_EQUAL ? value != test :
                  oper == IF_GREATER   ? value >  test :
                  oper == IF_LESS      ? value <  test :
                  oper == IF_GEQUAL    ? value >= test :
                                         value <= test);
        break;
        }
    case CVT_CHARPTR:
        {
        int comp = iCmpStrCase(CV_CHARPTR(var), argv[3]);

        isTrue = (oper == IF_EQUAL     ? comp == 0 :
                  oper == IF_NOT_EQUAL ? comp != 0 :
                  oper == IF_GREATER   ? comp >  0 :
                  oper == IF_LESS      ? comp <  0 :
                  oper == IF_GEQUAL    ? comp >= 0 :
                                         comp <= 0);
        }
        break;
    default:
        DE_ASSERT_FAIL("CCmdIf: Invalid cvar type");
        return false;
    }

    // Should the command be executed?
    if (isTrue)
    {
        Con_Execute(src, argv[4], ConsoleSilent, false);
    }
    else if (argc == 6)
    {
        Con_Execute(src, argv[5], ConsoleSilent, false);
    }
    return true;
}

D_CMD(DebugCrash)
{
    DE_UNUSED(src, argv, argc);

    int* ptr = (int*) 0x123;

    // Goodbye cruel world.
    *ptr = 0;
    return true;
}

D_CMD(HelpWhat);
D_CMD(HelpApropos);
D_CMD(ListAliases);
D_CMD(ListCmds);
D_CMD(ListVars);
#if _DEBUG
D_CMD(PrintVarStats);
#endif

static bool inited;

D_CMD(ListCmds);
D_CMD(HelpApropos);

void Con_DataRegister()
{
    C_CMD("apropos",        "s",    HelpApropos);
    C_CMD("listaliases",    NULL,   ListAliases);
    C_CMD("listcmds",       NULL,   ListCmds);
    C_CMD("listvars",       NULL,   ListVars);
#ifdef DE_DEBUG
    C_CMD("varstats",       NULL,   PrintVarStats);
#endif
}

void Con_InitDatabases(void)
{
    if (inited) return;

    Con_InitVariableDirectory();
    Con_InitCommands();
    Con_InitAliases();
    Con_ClearKnownWords();

    inited = true;
}

void Con_ClearDatabases(void)
{
    if (!inited) return;
    Con_ClearKnownWords();
    Con_ClearAliases();
    Con_ClearCommands();
    Con_ClearVariables();
}

void Con_ShutdownDatabases(void)
{
    if (!inited) return;

    Con_ClearDatabases();
    Con_DeinitVariableDirectory();

    inited = false;
}

String Con_GameAsStyledText(const Game *game)
{
    DE_ASSERT(game != 0);
    return String(_E(1)) + game->id() + _E(.);
}

static int printKnownWordWorker(const knownword_t *word, void *parameters)
{
    DE_ASSERT(word);
    uint *numPrinted = (uint *) parameters;

    switch (word->type)
    {
    case WT_CCMD: {
        ccmd_t *ccmd = (ccmd_t *) word->data;
        if (ccmd->prevOverload)
        {
            return 0; // Skip overloaded variants.
        }
        LOG_SCR_MSG("%s") << Con_CmdAsStyledText(ccmd);
        break; }

    case WT_CVAR: {
        cvar_t *cvar = (cvar_t *) word->data;
        if (cvar->flags & CVF_HIDE)
        {
            return 0; // Skip hidden variables.
        }
        Con_PrintCVar(cvar, "");
        break; }

    case WT_CALIAS:
        LOG_SCR_MSG("%s") << Con_AliasAsStyledText((calias_t *) word->data);
        break;

    case WT_GAME:
        LOG_SCR_MSG("%s") << Con_GameAsStyledText((const Game *) word->data);
        break;

    default:
        DE_ASSERT(false);
        break;
    }

    if (numPrinted) ++(*numPrinted);
    return 0; // Continue iteration.
}

D_CMD(ListVars)
{
    DE_UNUSED(src);

    uint numPrinted = 0;
    LOG_SCR_MSG(_E(b) "Console variables:");
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CVAR, printKnownWordWorker, &numPrinted);
    LOG_SCR_MSG("Found %i console variables") << numPrinted;
    return true;
}

D_CMD(ListCmds)
{
    DE_UNUSED(src);

    LOG_SCR_MSG(_E(b) "Console commands:");
    uint numPrinted = 0;
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CCMD, printKnownWordWorker, &numPrinted);
    LOG_SCR_MSG("Found %i console commands") << numPrinted;
    return true;
}

D_CMD(ListAliases)
{
    DE_UNUSED(src);

    LOG_SCR_MSG(_E(b) "Aliases:");
    uint numPrinted = 0;
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CALIAS, printKnownWordWorker, &numPrinted);
    LOG_SCR_MSG("Found %i aliases") << numPrinted;
    return true;
}

D_CMD(DoomsdayScript)
{
    DE_UNUSED(src);
    DE_UNUSED(argc);
    String source;
    for (int i = 1; i < argc; ++i)
    {
        if (source) source += " ";
        source += String(argv[i]);
    }
    Script script(source);
    Process proc(script);
    proc.execute();
    return true;
}
