/**\file con_main.cpp  Console subsystem.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#define DENG_NO_API_MACROS_CONSOLE

#include "de_platform.h"

#ifdef WIN32
#   include <process.h>
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_graphics.h"
#include "de_play.h"
#include "de_render.h"
#include "de_edit.h"
#include "de_ui.h"
#include "de_misc.h"
#include "de_infine.h"
#include "de_defs.h"
#include "de_filesys.h"

#include "Game"
#include <de/LogBuffer>
#include <de/charsymbols.h>

#ifdef __CLIENT__
#  include <de/DisplayMode>
#  include "clientapp.h"
#  include "ui/clientwindowsystem.h"
#  include "ui/clientwindow.h"
#  include "ui/widgets/consolewidget.h"
#  include "ui/widgets/taskbarwidget.h"
#  include "ui/busyvisual.h"
#  include "updater/downloaddialog.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cmath>
#include <cctype>

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

#define CMDTYPESTR(src) (src == CMDS_DDAY? "a direct call" \
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
D_CMD(Clear);
D_CMD(Echo);
#ifdef __CLIENT__
D_CMD(OpenClose);
D_CMD(TaskBar);
D_CMD(Tutorial);
#endif
D_CMD(Help);
D_CMD(If);
D_CMD(Parse);
D_CMD(Quit);
D_CMD(Repeat);
D_CMD(Toggle);
D_CMD(Version);
D_CMD(Wait);
D_CMD(InspectMobj);
D_CMD(DebugCrash);
D_CMD(DebugError);

void CVar_PrintReadOnlyWarning(cvar_t const *var); // in con_data.cpp

static int executeSubCmd(const char *subCmd, byte src, dd_bool isNetCmd);
static void Con_SplitIntoSubCommands(char const *command,
                                     timespan_t markerOffset, byte src,
                                     dd_bool isNetCmd);

static void Con_ClearExecBuffer(void);

int     CmdReturnValue = 0;
byte    ConsoleSilent = false;
char    *prbuff = NULL; // Print buffer, used by conPrintf.

static dd_bool      ConsoleInited;   // Has Con_Init() been called?
static execbuff_t * exBuff;
static int          exBuffSize;
static execbuff_t * curExec;

void Con_Register(void)
{
    C_CMD("add",            NULL,   AddSub);
    C_CMD("after",          "is",   Wait);
    C_CMD("alias",          NULL,   Alias);
    C_CMD("clear",          "",     Clear);
#ifdef __CLIENT__
    C_CMD_FLAGS("conclose",       "",     OpenClose,    CMDF_NO_DEDICATED);
    C_CMD_FLAGS("conopen",        "",     OpenClose,    CMDF_NO_DEDICATED);
    C_CMD_FLAGS("contoggle",      "",     OpenClose,    CMDF_NO_DEDICATED);
    C_CMD("taskbar",        "",     TaskBar);
    C_CMD("tutorial",       "",     Tutorial);
#endif
    C_CMD("dec",            NULL,   IncDec);
    C_CMD("echo",           "s*",   Echo);
    C_CMD("print",          "s*",   Echo);
    C_CMD("exec",           "s*",   Parse);
    C_CMD("help",           "",     Help);
    C_CMD("if",             NULL,   If);
    C_CMD("inc",            NULL,   IncDec);
    C_CMD("listmobjtypes",  "",     ListMobjs);
    C_CMD("load",           "s*",   Load);
    C_CMD("quit",           "",     Quit);
    C_CMD("inspectmobj",    "i",    InspectMobj);
    C_CMD("quit!",          "",     Quit);
    C_CMD("repeat",         "ifs",  Repeat);
    C_CMD("reset",          "",     Reset);
    C_CMD("reload",         "",     ReloadGame);
    C_CMD("sub",            NULL,   AddSub);
    C_CMD("toggle",         "s",    Toggle);
    C_CMD("unload",         "*",    Unload);
    C_CMD("version",        "",     Version);
    C_CMD("write",          "s",    WriteConsole);
#ifdef _DEBUG
    C_CMD("crash",          NULL,   DebugCrash);
    C_CMD("fatalerror",     NULL,   DebugError);
#endif

    C_VAR_CHARPTR("file-startup", &startupFiles, 0, 0, 0);

    Con_DataRegister();

#ifdef __CLIENT__
    /// @todo Move to UI module.
    Con_TransitionRegister();
#endif
}

static void PrepareCmdArgs(cmdargs_t *cargs, const char *lpCmdLine)
{
#define IS_ESC_CHAR(x)  ((x) == '"' || (x) == '\\' || (x) == '{' || (x) == '}')

    size_t              i, len = strlen(lpCmdLine);

    // Prepare the data.
    memset(cargs, 0, sizeof(cmdargs_t));
    strcpy(cargs->cmdLine, lpCmdLine);

    // Prepare.
    for(i = 0; i < len; ++i)
    {
        // Whitespaces are separators.
        if(DENG_ISSPACE(cargs->cmdLine[i]))
            cargs->cmdLine[i] = 0;

        if(cargs->cmdLine[i] == '\\' && IS_ESC_CHAR(cargs->cmdLine[i + 1]))
        {   // Escape sequence.
            memmove(cargs->cmdLine + i, cargs->cmdLine + i + 1,
                    sizeof(cargs->cmdLine) - i - 1);
            len--;
            continue;
        }

        if(cargs->cmdLine[i] == '"')
        {   // Find the end.
            size_t              start = i;

            cargs->cmdLine[i] = 0;
            for(++i; i < len && cargs->cmdLine[i] != '"'; ++i)
            {
                if(cargs->cmdLine[i] == '\\' && IS_ESC_CHAR(cargs->cmdLine[i + 1])) // Escape sequence?
                {
                    memmove(cargs->cmdLine + i, cargs->cmdLine + i + 1,
                            sizeof(cargs->cmdLine) - i - 1);
                    len--;
                    continue;
                }
            }

            // Quote not terminated?
            if(i == len)
                break;

            // An empty set of quotes?
            if(i == start + 1)
                cargs->cmdLine[i] = SC_EMPTY_QUOTE;
            else
                cargs->cmdLine[i] = 0;
        }

        if(cargs->cmdLine[i] == '{')
        {   // Find matching end, braces are another notation for quotes.
            int             level = 0;
            size_t          start = i;

            cargs->cmdLine[i] = 0;
            for(++i; i < len; ++i)
            {
                if(cargs->cmdLine[i] == '\\' && IS_ESC_CHAR(cargs->cmdLine[i + 1])) // Escape sequence?
                {
                    memmove(cargs->cmdLine + i, cargs->cmdLine + i + 1,
                            sizeof(cargs->cmdLine) - i - 1);
                    len--;
                    i++;
                    continue;
                }

                if(cargs->cmdLine[i] == '}')
                {
                    if(!level)
                        break;

                    level--;
                }

                if(cargs->cmdLine[i] == '{')
                    level++;
            }

            // Quote not terminated?
            if(i == len)
                break;

            // An empty set of braces?
            if(i == start + 1)
                cargs->cmdLine[i] = SC_EMPTY_QUOTE;
            else
                cargs->cmdLine[i] = 0;
        }
    }

    // Scan through the cmdLine and get the beginning of each token.
    cargs->argc = 0;
    for(i = 0; i < len; ++i)
    {
        if(!cargs->cmdLine[i])
            continue;

        // Is this an empty quote?
        if(cargs->cmdLine[i] == SC_EMPTY_QUOTE)
            cargs->cmdLine[i] = 0;  // Just an empty string.

        cargs->argv[cargs->argc++] = cargs->cmdLine + i;
        i += strlen(cargs->cmdLine + i);
    }

#undef IS_ESC_CHAR
}

dd_bool Con_Init(void)
{
    if(ConsoleInited) return true;

    LOG_SCR_VERBOSE("Initializing the console...");

    exBuff = NULL;
    exBuffSize = 0;

    ConsoleInited = true;

    return true;
}

void Con_Shutdown(void)
{
    if(!ConsoleInited) return;

    LOG_SCR_VERBOSE("Shutting down the console...");

    Con_ClearExecBuffer();
    Con_ShutdownDatabases();

    if(prbuff)
    {
        M_Free(prbuff); prbuff = 0;
    }

    ConsoleInited = false;
}

#ifdef __CLIENT__
/**
 * Send a console command to the server.
 * This shouldn't be called unless we're logged in with the right password.
 */
static void Con_Send(const char *command, byte src, int silent)
{
    ushort len = (ushort) strlen(command);

    LOG_AS("Con_Send");
    if(len >= 0x8000)
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
#endif // __CLIENT__

static void Con_QueueCmd(const char *singleCmd, timespan_t atSecond,
                         byte source, dd_bool isNetCmd)
{
    execbuff_t *ptr = NULL;
    int     i;

    // Look for an empty spot.
    for(i = 0; i < exBuffSize; ++i)
        if(!exBuff[i].used)
        {
            ptr = exBuff + i;
            break;
        }

    if(ptr == NULL)
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
#define BUFFSIZE 256
    dd_bool allDone;
    dd_bool ret = true;
    int     i, count = 0;
    char    storage[BUFFSIZE];

    storage[255] = 0;

    do                          // We'll keep checking until all is done.
    {
        allDone = true;

        // Execute the commands marked for this or a previous tic.
        for(i = 0; i < exBuffSize; ++i)
        {
            execbuff_t *ptr = exBuff + i;

            if(!ptr->used || ptr->when > sysTime)
                continue;
            // We'll now execute this command.
            curExec = ptr;
            ptr->used = false;
            strncpy(storage, ptr->subCmd, BUFFSIZE-1);

            if(!executeSubCmd(storage, ptr->source, ptr->isNetCmd))
                ret = false;
            allDone = false;
        }

        if(count++ > 100)
        {            
            DENG_ASSERT(!"Execution buffer overflow");
            LOG_SCR_ERROR("Console execution buffer overflow! Everything canceled!");
            Con_ClearExecBuffer();
            break;
        }
    } while(!allDone);

    return ret;
#undef BUFFSIZE
}

void Con_Ticker(timespan_t time)
{
    Con_CheckExecBuffer();
    if(tickFrame)
    {
        Con_TransitionTicker(time);
    }

    /*
#ifdef __CLIENT__
    Rend_ConsoleTicker(time);
#endif

    if(!ConsoleActive)
        return;                 // We have nothing further to do here.

    ConsoleTime += time;        // Increase the ticker.
    */
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

    for(i = 0; text[i]; ++i)
    {
        if(text[i] == '%' && (text[i + 1] >= '1' && text[i + 1] <= '9'))
        {
            char           *substr;
            int             aidx = text[i + 1] - '1' + 1;

            // Expand! (or delete)
            if(aidx > args->argc - 1)
                substr = (char *) "";
            else
                substr = args->argv[aidx];

            // First get rid of the %n.
            memmove(text + i, text + i + 2, size - i - 2);
            // Reallocate.
            off = strlen(substr);
            text = *expCommand = (char *) M_Realloc(*expCommand, size += off - 2);
            if(off)
            {
                // Make room for the insert.
                memmove(text + i + off, text + i, size - i - off);
                memcpy(text + i, substr, off);
            }
            i += off - 1;
        }
        else if(text[i] == '%' && text[i + 1] == '0')
        {
            // First get rid of the %0.
            memmove(text + i, text + i + 2, size - i - 2);
            text = *expCommand = (char *) M_Realloc(*expCommand, size -= 2);
            for(p = args->argc - 1; p > 0; p--)
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
    cmdargs_t   args;
    ccmd_t   *ccmd;
    cvar_t   *cvar;
    calias_t   *cal;

    PrepareCmdArgs(&args, subCmd);
    if(!args.argc)
        return true;

    /*
    if(args.argc == 1)  // Possibly a control command?
    {
        if(P_ControlExecute(args.argv[0]))
        {
            // It was a control command.  No further processing is
            // necessary.
            return true;
        }
    }
     */

#ifdef __CLIENT__
    // If logged in, send command to server at this point.
    if(!isServer && netLoggedIn)
    {
        // We have logged in on the server. Send the command there.
        Con_Send(subCmd, src, ConsoleSilent);
        return true;
    }
#endif

    // Try to find a matching console command.
    ccmd = Con_FindCommandMatchArgs(&args);
    if(ccmd != NULL)
    {
        // Found a match. Are we allowed to execute?
        dd_bool canExecute = true;

        // Trying to issue a command requiring a loaded game?
        // dj: This should be considered a short-term solution. Ideally we want some namespacing mechanics.
        if((ccmd->flags & CMDF_NO_NULLGAME) && !App_GameLoaded())
        {
            LOG_SCR_ERROR("Execution of command '%s' is only allowed when a game is loaded") << ccmd->name;
            return true;
        }

        // A dedicated server, trying to execute a ccmd not available to us?
        if(isDedicated && (ccmd->flags & CMDF_NO_DEDICATED))
        {
            LOG_SCR_ERROR("Execution of command '%s' not possible in dedicated mode") << ccmd->name;
            return true;
        }

        // Net commands sent to servers have extra protection.
        if(isServer && isNetCmd)
        {
            // Is the command permitted for use by clients?
            if(ccmd->flags & CMDF_CLIENT)
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
            switch(src)
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

        // Is the src permitted for this command?
        switch(src)
        {
        case CMDS_UNKNOWN:
            canExecute = false;
            break;

        case CMDS_DDAY:
            if(ccmd->flags & CMDF_DDAY)
                canExecute = false;
            break;

        case CMDS_GAME:
            if(ccmd->flags & CMDF_GAME)
                canExecute = false;
            break;

        case CMDS_CONSOLE:
            if(ccmd->flags & CMDF_CONSOLE)
                canExecute = false;
            break;

        case CMDS_BIND:
            if(ccmd->flags & CMDF_BIND)
                canExecute = false;
            break;

        case CMDS_CONFIG:
            if(ccmd->flags & CMDF_CONFIG)
                canExecute = false;
            break;

        case CMDS_PROFILE:
            if(ccmd->flags & CMDF_PROFILE)
                canExecute = false;
            break;

        case CMDS_CMDLINE:
            if(ccmd->flags & CMDF_CMDLINE)
                canExecute = false;
            break;

        case CMDS_SCRIPT:
            if(ccmd->flags & CMDF_DED)
                canExecute = false;
            break;

        default:
            return true;
        }

        if(!canExecute)
        {
            LOG_SCR_ERROR("'%s' cannot be executed via %s") << ccmd->name << CMDTYPESTR(src);
            return true;
        }

        if(canExecute)
        {
            /**
             * Execute the command!
             * \note Console command execution may invoke a full update of the
             * console databases; thus the @c ccmd pointer may be invalid after
             * this call.
             */
            int result;
            if((result = ccmd->execFunc(src, args.argc, args.argv)) == false)
            {
                LOG_SCR_ERROR("'%s' failed") << args.argv[0];
            }
            return result;
        }
        return true;
    }

    // Then try the cvars?
    cvar = Con_FindVariable(args.argv[0]);
    if(cvar != NULL)
    {
        dd_bool outOfRange = false, setting = false, hasCallback;

        /**
         * \note Change notification callback execution may invoke
         * a full update of the console databases; thus the @c cvar
         * pointer may be invalid once a callback executes.
         */
        hasCallback = (cvar->notifyChanged != 0);

        if(args.argc == 2 ||
           (args.argc == 3 && !stricmp(args.argv[1], "force")))
        {
            char* argptr = args.argv[args.argc - 1];
            dd_bool forced = args.argc == 3;

            setting = true;
            if(cvar->flags & CVF_READ_ONLY)
            {
                CVar_PrintReadOnlyWarning(cvar);
            }
            else if((cvar->flags & CVF_PROTECTED) && !forced)
            {
                AutoStr* name = CVar_ComposePath(cvar);
                LOG_SCR_NOTE("%s is protected; you shouldn't change its value -- "
                             "use the command: " _E(b) "'%s force %s'" _E(.) " to modify it anyway")
                        << Str_Text(name) << Str_Text(name) << argptr;
            }
            else
            {
                switch(cvar->type)
                {
                case CVT_BYTE: {
                    byte val = (byte) strtol(argptr, NULL, 0);
                    if(!forced &&
                       ((!(cvar->flags & CVF_NO_MIN) && val < cvar->min) ||
                        (!(cvar->flags & CVF_NO_MAX) && val > cvar->max)))
                        outOfRange = true;
                    else
                        CVar_SetInteger(cvar, val);
                    break;
                  }
                case CVT_INT: {
                    int val = strtol(argptr, NULL, 0);
                    if(!forced &&
                       ((!(cvar->flags & CVF_NO_MIN) && val < cvar->min) ||
                        (!(cvar->flags & CVF_NO_MAX) && val > cvar->max)))
                        outOfRange = true;
                    else
                        CVar_SetInteger(cvar, val);
                    break;
                  }
                case CVT_FLOAT: {
                    float val = strtod(argptr, NULL);
                    if(!forced &&
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
                case CVT_URIPTR: {
                    /// @todo Sanitize and validate against known schemas.
                    de::Uri uri(argptr, RC_NULL);
                    CVar_SetUri(cvar, reinterpret_cast<uri_s *>(&uri));
                    break; }
                default: break;
                }
            }
        }

        if(outOfRange)
        {
            AutoStr* name = CVar_ComposePath(cvar);
            if(!(cvar->flags & (CVF_NO_MIN | CVF_NO_MAX)))
            {
                char temp[20];
                strcpy(temp, M_TrimmedFloat(cvar->min));
                LOG_SCR_ERROR("%s <= %s <= %s") << temp << Str_Text(name) << M_TrimmedFloat(cvar->max);
            }
            else if(cvar->flags & CVF_NO_MAX)
            {
                LOG_SCR_ERROR("%s >= %s") << Str_Text(name) << M_TrimmedFloat(cvar->min);
            }
            else
            {
                LOG_SCR_ERROR("%s <= %s") << Str_Text(name) << M_TrimmedFloat(cvar->max);
            }
        }
        else if(!setting) // Show the value.
        {
            if(setting && hasCallback)
            {
                // Lookup the cvar again - our pointer may have been invalidated.
                cvar = Con_FindVariable(args.argv[0]);
            }

            if(cvar)
            {
                // It still exists.
                Con_PrintCVar(cvar, "");
            }
        }
        return true;
    }

    // How about an alias then?
    cal = Con_FindAlias(args.argv[0]);
    if(cal != NULL)
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
    if(Con_FindCommand(args.argv[0]))
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
    if(!command || command[0] == 0)
        return;

    // Jump over initial semicolons.
    len = strlen(command);
    while(gPos < len && command[gPos] == ';' && command[gPos] != 0)
        gPos++;

    subCmd[0] = subCmd[BUFFSIZE-1] = 0;

    // The command may actually contain many commands, separated
    // with semicolons. This isn't a very clear algorithm...
    for(; command[gPos];)
    {
        escape = false;
        if(inQuotes && command[gPos] == '\\')   // Escape sequence?
        {
            subCmd[scPos++] = command[gPos++];
            escape = true;
        }
        if(command[gPos] == '"' && !escape)
            inQuotes = !inQuotes;

        // Collect characters.
        subCmd[scPos++] = command[gPos++];
        if(subCmd[0] == ' ')
            scPos = 0;          // No spaces in the beginning.

        if((command[gPos] == ';' && !inQuotes) || command[gPos] == 0)
        {
            while(gPos < len && command[gPos] == ';' && command[gPos] != 0)
                gPos++;
            // The subcommand ends.
            subCmd[scPos] = 0;
        }
        else
        {
            continue;
        }

        // Queue it.
        Con_QueueCmd(subCmd, sysTime + markerOffset, src, isNetCmd);

        scPos = 0;
    }

#undef BUFFSIZE
}

struct AnnotationWork
{
    QSet<QString> terms;
    de::String result;
};

static int annotateMatchedWordCallback(knownword_t const *word, void *parameters)
{
    AnnotationWork *work = reinterpret_cast<AnnotationWork *>(parameters);
    AutoStr *name = Con_KnownWordToString(word);
    de::String found;

    if(!work->terms.contains(Str_Text(name)))
        return false; // keep going

    switch(word->type)
    {
    case WT_CVAR:
        if(!(((cvar_t *)word->data)->flags & CVF_HIDE))
        {
            found = Con_VarAsStyledText((cvar_t *) word->data, "");
        }
        break;

    case WT_CCMD:
        if(!((ccmd_t *)word->data)->prevOverload)
        {
            found = Con_CmdAsStyledText((ccmd_t *) word->data);
        }
        break;

    case WT_CALIAS:
        found = Con_AliasAsStyledText((calias_t *) word->data);
        break;

    case WT_GAME:
        found = Con_GameAsStyledText((Game *) word->data);
        break;

    default:
        break;
    }

    if(!found.isEmpty())
    {
        if(!work->result.isEmpty()) work->result.append("\n");
        work->result.append(found);
    }

    return false; // don't stop
}

de::String Con_AnnotatedConsoleTerms(QStringList terms)
{
    AnnotationWork work;
    foreach(QString term, terms)
    {
        work.terms.insert(term);
    }
    Con_IterateKnownWords(NULL, WT_ANY, annotateMatchedWordCallback, &work);
    return work.result;
}

/**
 * Wrapper for Con_Execute
 * Public method for plugins to execute console commands.
 */
int DD_Execute(int silent, const char* command)
{
    return Con_Execute(CMDS_GAME, command, silent, false);
}

int Con_Execute(byte src, const char *command, int silent, dd_bool netCmd)
{
    int             ret;

    if(silent)
        ConsoleSilent = true;

    Con_SplitIntoSubCommands(command, 0, src, netCmd);
    ret = Con_CheckExecBuffer();

    if(silent)
        ConsoleSilent = false;

    return ret;
}

/**
 * Exported version of Con_Executef
 */
int DD_Executef(int silent, const char *command, ...)
{
    va_list         argptr;
    char            buffer[4096];

    va_start(argptr, command);
    vsprintf(buffer, command, argptr);
    va_end(argptr);
    return Con_Execute(CMDS_GAME, buffer, silent, false);
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

void Con_Open(int yes)
{
#ifdef __CLIENT__
    if(yes)
    {
        ClientWindow &win = ClientWindow::main();
        win.taskBar().open();
        win.root().setFocus(&win.console().commandLine());
    }
    else
    {
        ClientWindow::main().console().closeLog();
    }
#endif

#ifdef __SERVER__
    DENG_UNUSED(yes);
#endif  
}

/*
/// @param flags  @ref consolePrintFlags
static void conPrintf(int flags, const char* format, va_list args)
{
    DENG_UNUSED(flags);

#ifdef __SERVER__
    const char* text = 0;
#endif

    if(format && format[0])
    {
        if(!prbuff)
        {
            prbuff = (char *) M_Malloc(PRBUFF_SIZE);
        }

        // Format the message to prbuff.
        dd_vsnprintf(prbuff, PRBUFF_SIZE, format, args);

#ifdef __SERVER__
        text = prbuff;
#endif

        LOG_MSG("") << prbuff;
    }

#ifdef __SERVER__
    // Servers might have to send the text to a number of clients.
    if(isServer)
    {
        if(flags & CPF_TRANSMIT)
            Sv_SendText(NSP_BROADCAST, flags, text);
        else if(netRemoteUser) // Is somebody logged in?
            Sv_SendText(netRemoteUser, flags | SV_CONSOLE_PRINT_FLAGS, text);
    }
#endif
}
*/

void Con_Error(char const *error, ...)
{
    LogBuffer_Flush();

    static dd_bool errorInProgress = false;

    //int i, numBufLines;
    char buff[2048], err[256];
    va_list argptr;

#ifdef __CLIENT__
    ClientWindow::main().canvas().trapMouse(false);
#endif

    // Already in an error?
    if(!ConsoleInited || errorInProgress)
    {
#ifdef __CLIENT__
        DisplayMode_Shutdown();
#endif

        va_start(argptr, error);
        dd_vsnprintf(buff, sizeof(buff), error, argptr);
        va_end(argptr);

        if(!BusyMode_InWorkerThread())
        {
            Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, buff, 0);
        }

        // Exit immediately, lest we go into an infinite loop.
        exit(1);
    }

    // We've experienced a fatal error; program will be shut down.
    errorInProgress = true;

    // Get back to the directory we started from.
    Dir_SetCurrent(ddRuntimePath);

    va_start(argptr, error);
    dd_vsnprintf(err, sizeof(err), error, argptr);
    va_end(argptr);

    LOG_CRITICAL("") << err;
    LogBuffer_Flush();

    strcpy(buff, "");
    strcat(buff, "\n");
    strcat(buff, err);

    if(BusyMode_Active())
    {
        BusyMode_WorkerError(buff);
        if(BusyMode_InWorkerThread())
        {
            // We should not continue to execute the worker any more.
            for(;;) Thread_Sleep(10000);
        }
    }
    else
    {
        Con_AbnormalShutdown(buff);
    }
}

void Con_AbnormalShutdown(char const *message)
{   
    // This is a crash landing, better be safe than sorry.
    BusyMode_SetAllowed(false);

    Sys_Shutdown();

#ifdef __CLIENT__
    DisplayMode_Shutdown();
    DENG2_GUI_APP->loop().pause();

    // This is an abnormal shutdown, we cannot continue drawing any of the
    // windows. (Alternatively could hide/disable drawing of the windows.) Note
    // that the app's event loop is running normally while we show the native
    // message box below -- if the app windows are not hidden/closed, they might
    // receive draw events.
    ClientApp::windowSystem().closeAll();
#endif

    if(message) // Only show if a message given.
    {
        // Make sure all the buffered stuff goes into the file.
        LogBuffer_Flush();

        /// @todo Get the actual output filename (might be a custom one).
        Sys_MessageBoxWithDetailsFromFile(MBT_ERROR, DOOMSDAY_NICENAME, message,
                                          "See Details for complete message log contents.",
                                          de::LogBuffer::appBuffer().outputFile().toUtf8());
    }

    //Sys_Shutdown();
    DD_Shutdown();

    // Get outta here.
    exit(1);
}

/**
 * Create an alias.
 */
static void Con_Alias(char *aName, char *command)
{
    calias_t *cal = Con_FindAlias(aName);
    dd_bool remove = false;

    // Will we remove this alias?
    if(command == NULL)
        remove = true;
    else if(command[0] == 0)
        remove = true;

    if(cal && remove) // This alias will be removed.
    {
        Con_DeleteAlias(cal);
        return; // We're done.
    }

    // Does the alias already exist?
    if(cal)
    {
        cal->command = (char *) M_Realloc(cal->command, strlen(command) + 1);
        strcpy(cal->command, command);
        return;
    }

    // We need to create a new alias.
    Con_AddAlias(aName, command);
}

static int addToTerms(knownword_t const *word, void *parameters)
{
    shell::Lexicon *lexi = reinterpret_cast<shell::Lexicon *>(parameters);
    lexi->addTerm(Str_Text(Con_KnownWordToString(word)));
    return 0;
}

shell::Lexicon Con_Lexicon()
{
    shell::Lexicon lexi;
    Con_IterateKnownWords(0, WT_ANY, addToTerms, &lexi);
    lexi.setAdditionalWordChars("-_.");
    return lexi;
}

D_CMD(Help)
{
    DENG2_UNUSED3(src, argc, argv);

    /*
#ifdef __CLIENT__
    char actKeyName[40];   
    strcpy(actKeyName, B_ShortNameForKey(consoleActiveKey));
    actKeyName[0] = toupper(actKeyName[0]);
#endif
*/

    LOG_SCR_NOTE(_E(b) DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT " Console");

#define TABBED(A, B) "\n" _E(Ta) _E(b) "  " << A << " " _E(.) _E(Tb) << B

#ifdef __CLIENT__
    LOG_SCR_MSG(_E(D) "Keys:" _E(.))
            << TABBED(DENG2_CHAR_SHIFT_KEY "Esc", "Open the taskbar and console")
            << TABBED("Tab", "Autocomplete the word at the cursor")
            << TABBED(DENG2_CHAR_UP_DOWN_ARROW, "Move backwards/forwards through the input command history, or up/down one line inside a multi-line command")
            << TABBED("PgUp/Dn", "Scroll up/down in the history, or expand the history to full height")
            << TABBED(DENG2_CHAR_SHIFT_KEY "PgUp/Dn", "Jump to the top/bottom of the history")
            << TABBED("Home", "Move the cursor to the start of the command line")
            << TABBED("End", "Move the cursor to the end of the command line")
            << TABBED(DENG2_CHAR_CONTROL_KEY "K", "Clear everything on the line right of the cursor position")
            << TABBED("F5", "Clear the console message history");
#endif
    LOG_SCR_MSG(_E(D) "Getting started:");
    LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "help (what)" _E(.) " for information about " _E(l) "(what)");
    LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "listcmds" _E(.) " to list available commands");
    LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "listgames" _E(.) " to list installed games and their status");
    LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "listvars" _E(.) " to list available variables");

#undef TABBED

    return true;
}

D_CMD(Clear)
{
    DENG2_UNUSED3(src, argc, argv);

#ifdef __CLIENT__
    ClientWindow::main().console().clearLog();
#endif
    return true;
}

D_CMD(Version)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_NOTE(_E(D) DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_FULLTEXT);
    LOG_MSG(_E(l) "Homepage: " _E(.) _E(i) DOOMSDAY_HOMEURL _E(.)
            "\n" _E(l) "Project: " _E(.) _E(i) DENGPROJECT_HOMEURL);

    // Print the version info of the current game if loaded.
    if(App_GameLoaded())
    {
        LOG_MSG(_E(l) "Game: " _E(.) "%s") << (char const *) gx.GetVariable(DD_PLUGIN_VERSION_LONG);
    }
    return true;
}

D_CMD(Quit)
{
    DENG2_UNUSED2(src, argc);

#ifdef __CLIENT__
    if(DownloadDialog::isDownloadInProgress())
    {
        LOG_WARNING("Cannot quit while downloading an update");
        ClientWindow::main().taskBar().openAndPauseGame();
        DownloadDialog::currentDownload().open();
        return false;
    }
#endif

    if(argv[0][4] == '!' || isDedicated || !App_GameLoaded() ||
       gx.TryShutdown == 0)
    {
        // No questions asked.
        Sys_Quit();
        return true; // Never reached.
    }

#ifdef __CLIENT__
    // Dismiss the taskbar if it happens to be open, we are expecting
    // the game to handle this from now on.
    ClientWindow::main().taskBar().close();
#endif

    // Defer this decision to the loaded game.
    return gx.TryShutdown();
}

D_CMD(Alias)
{
    DENG2_UNUSED(src);

    if(argc != 3 && argc != 2)
    {
        LOG_SCR_NOTE("Usage: %s (alias) (cmd)") << argv[0];
        LOG_SCR_MSG("Example: alias bigfont \"font size 3\"");
        LOG_SCR_MSG("Use %%1-%%9 to pass the alias arguments to the command.");
        return true;
    }

    Con_Alias(argv[1], argc == 3 ? argv[2] : NULL);
    if(argc != 3)
    {
        LOG_SCR_MSG("Alias '%s' deleted") << argv[1];
    }
    return true;
}

D_CMD(Parse)
{
    DENG2_UNUSED(src);

    int     i;

    for(i = 1; i < argc; ++i)
    {
        LOG_SCR_MSG("Parsing \"%s\"") << argv[i];
        Con_ParseCommands(argv[i]);
    }
    return true;
}

D_CMD(Wait)
{
    DENG2_UNUSED2(src, argc);

    timespan_t offset;

    offset = strtod(argv[1], NULL) / 35;    // Offset in seconds.
    if(offset < 0)
        offset = 0;
    Con_SplitIntoSubCommands(argv[2], offset, CMDS_CONSOLE, false);
    return true;
}

D_CMD(Repeat)
{
    DENG2_UNUSED2(src, argc);

    int     count;
    timespan_t interval, offset;

    count = atoi(argv[1]);
    interval = strtod(argv[2], NULL) / 35;  // In seconds.
    offset = 0;
    while(count-- > 0)
    {
        offset += interval;
        Con_SplitIntoSubCommands(argv[3], offset, CMDS_CONSOLE, false);
    }
    return true;
}

D_CMD(Echo)
{
    DENG2_UNUSED(src);

    int     i;

    for(i = 1; i < argc; ++i)
    {
        LOG_MSG("%s") << argv[i];
    }
    return true;
}

static dd_bool cvarAddSub(const char* name, float delta, dd_bool force)
{
    cvar_t* cvar = Con_FindVariable(name);
    float val;

    if(!cvar)
    {
        if(name && name[0])
            LOG_SCR_ERROR("%s is not a known cvar") << name;
        return false;
    }

    if(cvar->flags & CVF_READ_ONLY)
    {
        CVar_PrintReadOnlyWarning(cvar);
        return false;
    }

    val = Con_GetFloat(name) + delta;
    if(!force)
    {
        if(!(cvar->flags & CVF_NO_MAX) && val > cvar->max)
            val = cvar->max;
        if(!(cvar->flags & CVF_NO_MIN) && val < cvar->min)
            val = cvar->min;
    }
    Con_SetFloat(name, val);
    return true;
}

/**
 * Rather messy, wouldn't you say?
 */
D_CMD(AddSub)
{
    DENG2_UNUSED(src);

    dd_bool             force = false;
    float               delta = 0;

    if(argc <= 2)
    {
        LOG_SCR_NOTE("Usage: %s (cvar) (val) (force)") << argv[0];
        LOG_SCR_MSG("Use force to make cvars go off limits.");
        return true;
    }
    if(argc >= 4)
    {
        force = !stricmp(argv[3], "force");
    }

    delta = strtod(argv[2], NULL);
    if(!stricmp(argv[0], "sub"))
        delta = -delta;

    return cvarAddSub(argv[1], delta, force);
}

/**
 * Rather messy, wouldn't you say?
 */
D_CMD(IncDec)
{
    DENG2_UNUSED(src);

    dd_bool force = false;
    cvar_t* cvar;
    float val;

    if(argc == 1)
    {
        LOG_SCR_NOTE("Usage: %s (cvar) (force)") << argv[0];
        LOG_SCR_MSG("Use force to make cvars go off limits.");
        return true;
    }
    if(argc >= 3)
    {
        force = !stricmp(argv[2], "force");
    }
    cvar = Con_FindVariable(argv[1]);
    if(!cvar)
        return false;

    if(cvar->flags & CVF_READ_ONLY)
    {
        LOG_SCR_ERROR("%s (cvar) is read-only, it cannot be changed (even with force)") << argv[1];
        return false;
    }

    val = Con_GetFloat(argv[1]);
    val += !stricmp(argv[0], "inc")? 1 : -1;

    if(!force)
    {
        if(!(cvar->flags & CVF_NO_MAX) && val > cvar->max)
            val = cvar->max;
        if(!(cvar->flags & CVF_NO_MIN) && val < cvar->min)
            val = cvar->min;
    }

    Con_SetFloat(argv[1], val);
    return true;
}

/**
 * Toggle the value of a variable between zero and nonzero.
 */
D_CMD(Toggle)
{
    DENG2_UNUSED2(src, argc);

    Con_SetInteger(argv[1], Con_GetInteger(argv[1]) ? 0 : 1);
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
    uint        i, oper;
    cvar_t     *var;
    dd_bool     isTrue = false;

    if(argc != 5 && argc != 6)
    {
        LOG_SCR_NOTE("Usage: %s (cvar) (operator) (value) (cmd) (else-cmd)") << argv[0];
        LOG_SCR_MSG("Operator must be one of: not, =, >, <, >=, <=");
        LOG_SCR_MSG("The (else-cmd) can be omitted.");
        return true;
    }

    var = Con_FindVariable(argv[1]);
    if(!var)
        return false;

    // Which operator?
    for(i = 0; operators[i].opstr; ++i)
        if(!stricmp(operators[i].opstr, argv[2]))
        {
            oper = operators[i].op;
            break;
        }
    if(!operators[i].opstr)
        return false;           // Bad operator.

    // Value comparison depends on the type of the variable.
    switch(var->type)
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
        int comp = stricmp(CV_CHARPTR(var), argv[3]);

        isTrue = (oper == IF_EQUAL     ? comp == 0 :
                  oper == IF_NOT_EQUAL ? comp != 0 :
                  oper == IF_GREATER   ? comp >  0 :
                  oper == IF_LESS      ? comp <  0 :
                  oper == IF_GEQUAL    ? comp >= 0 :
                                         comp <= 0);
        }
        break;
    default:
        DENG_ASSERT(!"CCmdIf: Invalid cvar type");
        return false;
    }

    // Should the command be executed?
    if(isTrue)
    {
        Con_Execute(src, argv[4], ConsoleSilent, false);
    }
    else if(argc == 6)
    {
        Con_Execute(src, argv[5], ConsoleSilent, false);
    }
    CmdReturnValue = isTrue;
    return true;
}

#ifdef __CLIENT__

/**
 * Console command to open/close the console prompt.
 */
D_CMD(OpenClose)
{
    DENG2_UNUSED2(src, argc);

    if(!stricmp(argv[0], "conopen"))
    {
        Con_Open(true);
    }
    else if(!stricmp(argv[0], "conclose"))
    {
        Con_Open(false);
    }
    else
    {
        Con_Open(!ClientWindow::main().console().isLogOpen());
    }
    return true;
}

D_CMD(TaskBar)
{
    DENG2_UNUSED3(src, argc, argv);

    ClientWindow &win = ClientWindow::main();
    if(!win.taskBar().isOpen() || !win.console().commandLine().hasFocus())
    {
        win.taskBar().open();
        win.console().focusOnCommandLine();
    }
    else
    {
        win.taskBar().close();
    }
    return true;
}

D_CMD(Tutorial)
{
    DENG2_UNUSED3(src, argc, argv);
    ClientWindow::main().taskBar().showTutorial();
    return true;
}

#endif // __CLIENT__

D_CMD(DebugCrash)
{
    DENG2_UNUSED3(src, argv, argc);

    int* ptr = (int*) 0x123;

    // Goodbye cruel world.
    *ptr = 0;
    return true;
}

D_CMD(DebugError)
{
    DENG2_UNUSED3(src, argv, argc);

    Con_Error("Fatal error!\n");
    return true;
}

DENG_DECLARE_API(Con) =
{
    { DE_API_CONSOLE },

    Con_Open,
    Con_AddCommand,
    Con_AddVariable,
    Con_AddCommandList,
    Con_AddVariableList,

    Con_GetVariableType,

    Con_GetByte,
    Con_GetInteger,
    Con_GetFloat,
    Con_GetString,
    Con_GetUri,

    Con_SetInteger2,
    Con_SetInteger,

    Con_SetFloat2,
    Con_SetFloat,

    Con_SetString2,
    Con_SetString,

    Con_SetUri2,
    Con_SetUri,

    Con_Error,

    DD_Execute,
    DD_Executef,
};
