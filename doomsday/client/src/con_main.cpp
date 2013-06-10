/**\file con_main.cpp
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

/**
 * Console Subsystem
 *
 * Should be completely redesigned.
 */

// HEADER FILES ------------------------------------------------------------

#define DENG_NO_API_MACROS_CONSOLE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>

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

#include "resource/fonts.h"
//#include "cbuffer.h"
#include "Game"
#include <de/LogBuffer>

#ifdef __CLIENT__
#  include <de/DisplayMode>
#  include "clientapp.h"
#  include "ui/windowsystem.h"
#  include "ui/clientwindow.h"
#  include "ui/widgets/consolewidget.h"
#  include "ui/widgets/taskbarwidget.h"
#  include "ui/busyvisual.h"
#  include "updater/downloaddialog.h"
#endif

using namespace de;

#define SC_EMPTY_QUOTE  -1

// Length of the print buffer. Used in conPrintf and in Console_Message.
// If console messages are longer than this, an error will occur.
// Needed because we can't sizeof a malloc'd block.
#define PRBUFF_SIZE 655365

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
    boolean used;               // Is this in use?
    timespan_t when;            // System time when to execute the command.
    byte    source;             // Where the command came from
                                // (console input, a cfg file etc..)
    boolean isNetCmd;           // Command was sent over the net to us.
    char    subCmd[1024];       // A single command w/args.
} execbuff_t;

D_CMD(AddSub);
D_CMD(IncDec);
D_CMD(Alias);
D_CMD(Clear);
D_CMD(Echo);
#ifdef __CLIENT__
D_CMD(Font);
D_CMD(OpenClose);
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

static int executeSubCmd(const char *subCmd, byte src, boolean isNetCmd);
static void Con_SplitIntoSubCommands(const char *command,
                                     timespan_t markerOffset, byte src,
                                     boolean isNetCmd);

static void Con_ClearExecBuffer(void);

int     CmdReturnValue = 0;

byte    ConsoleSilent = false;

//int     conCompMode = 0;        // Completion mode.
byte    conSilentCVars = 1;
byte    consoleDump = true;
//int     consoleActiveKey = '`'; // Tilde.
//byte    consoleSnapBackOnPrint = false;

char* prbuff = NULL; // Print buffer, used by conPrintf.

//static CBuffer* histBuf = NULL; // The console history buffer (log).
//static uint bLineOff; // How many lines from the last in the histBuf?

//static char** oldCmds = NULL; // The old commands buffer.
//static uint oldCmdsSize = 0, oldCmdsMax = 0;
//static uint ocPos; // How many cmds from the last in the oldCmds buffer.

static boolean ConsoleInited;   // Has Con_Init() been called?

//static boolean ConsoleActive;   // Is the console active?
//static timespan_t ConsoleTime;  // How many seconds has the console been open?

//static char cmdLine[CMDLINE_SIZE+1]; // The command line.
//static uint cmdCursor;          // Position of the cursor on the command line.
//static boolean cmdInsMode;      // Are we in insert input mode.
//static boolean conInputLock;    // While locked, most user input is disabled.

static execbuff_t* exBuff;
static int exBuffSize;
static execbuff_t* curExec;

// The console font.
//static fontid_t consoleFont;
//static int consoleFontTracking;
//static float consoleFontLeading;
//static float consoleFontScale[2];

//static void (*consolePrintFilter) (char* text); // Maybe alters text.

//static uint complPos; // Where is the completion cursor?
//static uint lastCompletion; // The last completed known word match (1-based index).

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
#endif
    C_CMD("dec",            NULL,   IncDec);
    C_CMD("echo",           "s*",   Echo);
    C_CMD("print",          "s*",   Echo);
    C_CMD("exec",           "s*",   Parse);
/*#ifdef __CLIENT__
    C_CMD("font",           NULL,   Font);
#endif*/
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

    /*
    // Console
    C_VAR_INT("con-completion", &conCompMode, 0, 0, 1);
    C_VAR_BYTE("con-dump", &consoleDump, 0, 0, 1);
    C_VAR_INT("con-key-activate", &consoleActiveKey, 0, 0, 255);
    C_VAR_BYTE("con-var-silent", &conSilentCVars, 0, 0, 1);
    C_VAR_BYTE("con-snapback", &consoleSnapBackOnPrint, 0, 0, 1);
    */

    // Games
    C_CMD("listgames",      "",     ListGames);

    // File
    C_VAR_CHARPTR("file-startup", &startupFiles, 0, 0, 0);

    Con_DataRegister();

    Con_TransitionRegister();
}

#if 0
void Con_ResizeHistoryBuffer(void)
{
    int maxLength = 70;

    if(!ConsoleInited)
    {
        Con_Error("Con_ResizeHistoryBuffer: Console is not yet initialised.");
        exit(1); // Unreachable.
    }

#ifdef __CLIENT__
    if(!novideo && !isDedicated)
    {
        float cw;

        FR_SetFont(consoleFont);
        FR_LoadDefaultAttrib();
        FR_SetTracking(consoleFontTracking);
        FR_SetLeading(consoleFontLeading);

        cw = (FR_TextWidth("AA") * consoleFontScale[0]) / 2;
        if(0 != cw)
        {
            maxLength = MIN_OF(DENG_WINDOW->width() / cw - 2, 250);
        }
    }
#endif

    CBuffer_SetMaxLineLength(Con_HistoryBuffer(), maxLength);
}
#endif

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

#if 0
static void clearCommandHistory(void)
{
    if(!oldCmds) return;

    { uint i;
    for(i = 0; i < oldCmdsSize; ++i)
    {
        free(oldCmds[i]);
    }}
    free(oldCmds), oldCmds = NULL;
    oldCmdsSize = 0, oldCmdsMax = 0;
}
#endif

boolean Con_Init(void)
{
    if(ConsoleInited)
    {
#if _DEBUG
        Con_Error("Con_Init: Console already initialized!");
#endif
        return true;
    }

    Con_Message("Initializing the console...");

    exBuff = NULL;
    exBuffSize = 0;

#if 0
    histBuf = CBuffer_New(512, 70, 0);
    bLineOff = 0;

    oldCmds = NULL;
    oldCmdsSize = 0, oldCmdsMax = 0;
    ocPos = 0; // No commands yet.

    complPos = 0;
    lastCompletion = 0;

    cmdCursor = 0;

    consoleFont = 0;
    consoleFontTracking = 0;
    consoleFontLeading = 1.f;
    consoleFontScale[0] = 1.f;
    consoleFontScale[1] = 1.f;

    consolePrintFilter = 0;

    ConsoleTime = 0;
    ConsoleActive = false;

    //Rend_ConsoleInit();
#endif

    ConsoleInited = true;

    return true;
}

void Con_Shutdown(void)
{
    if(!ConsoleInited) return;

    Con_Message("Shutting down the console...");

    Con_ClearExecBuffer();
    Con_ShutdownDatabases();

    if(prbuff)
    {
        M_Free(prbuff); prbuff = 0;
    }

#if 0
    if(histBuf)
    {
        CBuffer_Delete(histBuf); histBuf = 0;
    }

    clearCommandHistory();
#endif

    ConsoleInited = false;
}

#if 0
boolean Con_IsActive(void)
{
    return ConsoleActive;
}

boolean Con_IsLocked(void)
{
    return conInputLock;
}

boolean Con_InputMode(void)
{
    return cmdInsMode;
}

char* Con_CommandLine(void)
{
    return cmdLine;
}

CBuffer* Con_HistoryBuffer(void)
{
    return histBuf;
}

uint Con_HistoryOffset(void)
{
    return bLineOff;
}

uint Con_CommandLineCursorPosition(void)
{
    return cmdCursor;
}

fontid_t Con_Font(void)
{
    if(!ConsoleInited)
        Con_Error("Con_Font: Console is not yet initialised.");
    return consoleFont;
}

void Con_SetFont(fontid_t font)
{
    if(!ConsoleInited)
        Con_Error("Con_SetFont: Console is not yet initialised.");
    if(consoleFont == font) return;
    consoleFont = font;
    Con_ResizeHistoryBuffer();
    Rend_ConsoleResize(true/*force*/);
}

con_textfilter_t Con_PrintFilter(void)
{
    if(!ConsoleInited)
        Con_Error("Con_PrintFilter: Console is not yet initialised.");
    return consolePrintFilter;
}
#endif

void Con_SetPrintFilter(con_textfilter_t /*printFilter*/)
{
    /*
    if(!ConsoleInited)
        Con_Error("Con_SetPrintFilter: Console is not yet initialised.");
    consolePrintFilter = printFilter;
    */
}

#if 0
void Con_FontScale(float* scaleX, float* scaleY)
{
    if(!ConsoleInited)
        Con_Error("Con_FontScale: Console is not yet initialised.");
    if(scaleX)
        *scaleX = consoleFontScale[0];
    if(scaleY)
        *scaleY = consoleFontScale[1];
}

void Con_SetFontScale(float scaleX, float scaleY)
{
    if(!ConsoleInited)
        Con_Error("Con_SetFont: Console is not yet initialised.");
    if(scaleX > 0.0001f)
        consoleFontScale[0] = MAX_OF(.5f, scaleX);
    if(scaleY > 0.0001f)
        consoleFontScale[1] = MAX_OF(.5f, scaleY);
    Con_ResizeHistoryBuffer();
    Rend_ConsoleResize(true/*force*/);
}

float Con_FontLeading(void)
{
    if(!ConsoleInited)
        Con_Error("Con_FontLeading: Console is not yet initialised.");
    return consoleFontLeading;
}

void Con_SetFontLeading(float value)
{
    if(!ConsoleInited)
        Con_Error("Con_SetFontLeading: Console is not yet initialised.");
    consoleFontLeading = MAX_OF(.1f, value);
    Con_ResizeHistoryBuffer();
    Rend_ConsoleResize(true/*force*/);
}

int Con_FontTracking(void)
{
    if(!ConsoleInited)
        Con_Error("Con_FontTracking: Console is not yet initialised.");
    return consoleFontTracking;
}

void Con_SetFontTracking(int value)
{
    if(!ConsoleInited)
        Con_Error("Con_SetFontTracking: Console is not yet initialised.");
    consoleFontTracking = MAX_OF(0, value);
    Con_ResizeHistoryBuffer();
    Rend_ConsoleResize(true/*force*/);
}
#endif

#ifdef __CLIENT__
/**
 * Send a console command to the server.
 * This shouldn't be called unless we're logged in with the right password.
 */
static void Con_Send(const char *command, byte src, int silent)
{
    ushort len = (ushort) strlen(command);

    if(len >= 0x8000)
    {
        Con_Message("Con_Send: Command is too long, length=%i.", len);
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
                         byte source, boolean isNetCmd)
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
static boolean Con_CheckExecBuffer(void)
{
#define BUFFSIZE 256
    boolean allDone;
    boolean ret = true;
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
            Con_Message("Console execution buffer overflow! Everything canceled.");
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
static int executeSubCmd(const char *subCmd, byte src, boolean isNetCmd)
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
        boolean canExecute = true;

        // Trying to issue a command requiring a loaded game?
        // dj: This should be considered a short-term solution. Ideally we want some namespacing mechanics.
        if((ccmd->flags & CMDF_NO_NULLGAME) && !App_GameLoaded())
        {
            Con_Printf("Execution of command '%s' not possible with no game loaded.\n", ccmd->name);
            return true;
        }

        // A dedicated server, trying to execute a ccmd not available to us?
        if(isDedicated && (ccmd->flags & CMDF_NO_DEDICATED))
        {
            Con_Printf("Execution of command '%s' not possible in dedicated mode.\n", ccmd->name);
            return true;
        }

        // Net commands sent to servers have extra protection.
        if(isServer && isNetCmd)
        {
            // Is the command permitted for use by clients?
            if(ccmd->flags & CMDF_CLIENT)
            {
                Con_Printf("Execution of command '%s' blocked (client attempted invocation).\n"
                           "This command is not permitted for use by clients\n", ccmd->name);
                // \todo Tell the client!
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
                Con_Printf("Execution of command '%s' blocked (client attempted invocation via %s).\n"
                           "This method is not permitted by clients.\n", ccmd->name, CMDTYPESTR(src));
                // \todo Tell the client!
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
            Con_Printf("Error: '%s' cannot be executed via %s.\n", ccmd->name, CMDTYPESTR(src));
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
                Con_Printf("Error: '%s' failed.\n", args.argv[0]);
            }
            return result;
        }
        return true;
    }

    // Then try the cvars?
    cvar = Con_FindVariable(args.argv[0]);
    if(cvar != NULL)
    {
        boolean out_of_range = false, setting = false, hasCallback;

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
            boolean forced = args.argc == 3;

            setting = true;
            if(cvar->flags & CVF_READ_ONLY)
            {
                AutoStr* name = CVar_ComposePath(cvar);
                Con_Printf("%s is read-only. It can't be changed (not even with force)\n", Str_Text(name));
            }
            else if((cvar->flags & CVF_PROTECTED) && !forced)
            {
                AutoStr* name = CVar_ComposePath(cvar);
                Con_Printf("%s is protected. You shouldn't change its value.\n"
                           "Use the command: '%s force %s' to modify it anyway.\n",
                           Str_Text(name), Str_Text(name), argptr);
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
                        out_of_range = true;
                    else
                        CVar_SetInteger(cvar, val);
                    break;
                  }
                case CVT_INT: {
                    int val = strtol(argptr, NULL, 0);
                    if(!forced &&
                       ((!(cvar->flags & CVF_NO_MIN) && val < cvar->min) ||
                        (!(cvar->flags & CVF_NO_MAX) && val > cvar->max)))
                        out_of_range = true;
                    else
                        CVar_SetInteger(cvar, val);
                    break;
                  }
                case CVT_FLOAT: {
                    float val = strtod(argptr, NULL);
                    if(!forced &&
                       ((!(cvar->flags & CVF_NO_MIN) && val < cvar->min) ||
                        (!(cvar->flags & CVF_NO_MAX) && val > cvar->max)))
                        out_of_range = true;
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

        if(out_of_range)
        {
            AutoStr* name = CVar_ComposePath(cvar);
            if(!(cvar->flags & (CVF_NO_MIN | CVF_NO_MAX)))
            {
                char temp[20];
                strcpy(temp, M_TrimmedFloat(cvar->min));
                Con_Printf("Error: %s <= %s <= %s\n", temp, Str_Text(name), M_TrimmedFloat(cvar->max));
            }
            else if(cvar->flags & CVF_NO_MAX)
            {
                Con_Printf("Error: %s >= %s\n", Str_Text(name), M_TrimmedFloat(cvar->min));
            }
            else
            {
                Con_Printf("Error: %s <= %s\n", Str_Text(name), M_TrimmedFloat(cvar->max));
            }
        }
        else if(!setting || !conSilentCVars) // Show the value.
        {
            if(setting && hasCallback)
            {
                // Lookup the cvar again - our pointer may have been invalidated.
                cvar = Con_FindVariable(args.argv[0]);
            }

            if(cvar)
            {   // It still exists.
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
    Con_Printf("%s: unknown identifier, or command arguments invalid.\n", args.argv[0]);
    return false;
}

/**
 * Splits the command into subcommands and queues them into the
 * execution buffer.
 */
static void Con_SplitIntoSubCommands(const char *command,
                                     timespan_t markerOffset, byte src,
                                     boolean isNetCmd)
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

#if 0
//#ifdef __CLIENT__
/**
 * Ambiguous string check. 'amb' is cut at the first character that
 * differs when compared to 'str' (case ignored).
 */
static void stramb(char *amb, const char *str)
{
    while(*str && tolower((unsigned) *amb) == tolower((unsigned) *str))
    {
        amb++;
        str++;
    }
    *amb = 0;
}

/**
 * Look at the last word and try to complete it. If there are several
 * possibilities, print them.
 *
 * @return          Number of possible completions.
 */
static int completeWord(int mode)
{
    static char lastWord[100];

    int cp = (int) strlen(cmdLine) - 1;
    char word[100], *wordBegin;
    char unambiguous[256];
    const knownword_t* completeWord = NULL;
    const knownword_t** matches = NULL;
    uint numMatches = 0;

    if(mode == 1)
        cp = (int)complPos - 1;
    if(cp < 0)
        return 0;

    memset(unambiguous, 0, sizeof(unambiguous));

    // Skip over any whitespace behind the cursor.
    while(cp > 0 && cmdLine[cp] == ' ')
        cp--;

    // Rewind the word pointer until space or a semicolon is found.
    while(cp > 0 &&
          cmdLine[cp - 1] != ' ' &&
          cmdLine[cp - 1] != ';' &&
          cmdLine[cp - 1] != '"')
        cp--;

    // Now cp is at the beginning of the word that needs completing.
    strcpy(word, wordBegin = cmdLine + cp);

    if(mode == 1)
        word[complPos - cp] = 0; // Only check a partial word.

    if(strlen(word) != 0)
    {
        matches = Con_CollectKnownWordsMatchingWord(word, WT_ANY, &numMatches);
    }

    // If this a new word; reset the matches iterator.
    if(stricmp(word, lastWord))
    {
        lastCompletion = 0;
        memcpy(lastWord, word, sizeof(lastWord));
    }

    if(!numMatches)
        return 0;

    // At this point we have at least one completion for the word.
    if(mode == 1)
    {
        // Completion Mode 1: Cycle through the possible completions.
        uint idx;

        /// @note lastCompletion uses a 1-based index.
        if(lastCompletion == 0 /* first completion attempt? */||
           lastCompletion >= numMatches /* reached the end? */)
            idx = 1;
        else
            idx = lastCompletion + 1;
        lastCompletion = idx;

        completeWord = matches[idx-1];
    }
    else
    {
        // Completion Mode 2: Print the possible completions
        boolean printCompletions = (numMatches > 1? true : false);
        const knownword_t** match;
        boolean updated = false;

        if(printCompletions)
            Con_Printf("Completions:\n");

        for(match = matches; *match; ++match)
        {
            const char* foundWord;

            switch((*match)->type)
            {
            case WT_CVAR: {
                cvar_t* cvar = (cvar_t*)(*match)->data;
                AutoStr* foundName = CVar_ComposePath(cvar);
                foundWord = Str_Text(foundName);
                if(printCompletions)
                    Con_PrintCVar(cvar, "  ");
                break;
              }
            case WT_CCMD: {
                ccmd_t* ccmd = (ccmd_t*)(*match)->data;
                foundWord = ccmd->name;
                if(printCompletions)
                    Con_FPrintf(CPF_LIGHT|CPF_YELLOW, "  %s\n", foundWord);
                break;
              }
            case WT_CALIAS: {
                calias_t* calias = (calias_t*)(*match)->data;
                foundWord = calias->name;
                if(printCompletions)
                    Con_FPrintf(CPF_LIGHT|CPF_YELLOW, "  %s == %s\n", foundWord, calias->command);
                break;
              }
            case WT_GAME: {
                Game *game = (Game *)(*match)->data;
                foundWord = Str_Text(game->identityKey());
                if(printCompletions)
                    Con_FPrintf(CPF_LIGHT|CPF_BLUE, "  %s\n", foundWord);
                break;
              }
            default:
                Con_Error("completeWord: Invalid word type %i.", (int)completeWord->type);
                exit(1); // Unreachable.
            }

            if(!unambiguous[0])
                strcpy(unambiguous, foundWord);
            else
                stramb(unambiguous, foundWord);

            if(!updated)
            {
                completeWord = *match;
                updated = true;
            }
        }
    }

    // Was a single match found?
    if(numMatches == 1 || (mode == 1 && numMatches > 1))
    {
        const char* str;

        switch(completeWord->type)
        {
        case WT_CALIAS:   str = ((calias_t*)completeWord->data)->name; break;
        case WT_CCMD:     str = ((ccmd_t*)completeWord->data)->name; break;
        case WT_CVAR: {
            AutoStr* foundName = CVar_ComposePath((cvar_t*)completeWord->data);
            str = Str_Text(foundName);
            break; }
        case WT_GAME: str = Str_Text(reinterpret_cast<Game *>(completeWord->data)->identityKey()); break;
        default:
            Con_Error("completeWord: Invalid word type %i.", (int)completeWord->type);
            exit(1); // Unreachable.
        }

        if(wordBegin - cmdLine + strlen(str) < CMDLINE_SIZE)
        {
            strcpy(wordBegin, str);
            cmdCursor = (uint) strlen(cmdLine);
        }
    }
    else if(numMatches > 1)
    {
        // More than one match; only complete the unambiguous part.
        if(wordBegin - cmdLine + strlen(unambiguous) < CMDLINE_SIZE)
        {
            strcpy(wordBegin, unambiguous);
            cmdCursor = (uint) strlen(cmdLine);
        }
    }

    free((void*)matches);
    return numMatches;
}
#endif // __CLIENT__

/**
 * Wrapper for Con_Execute
 * Public method for plugins to execute console commands.
 */
int DD_Execute(int silent, const char* command)
{
    return Con_Execute(CMDS_GAME, command, silent, false);
}

int Con_Execute(byte src, const char *command, int silent, boolean netCmd)
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

#if 0
//#ifdef __CLIENT__
static const char* getCommandFromHistory(uint idx)
{
    if(idx < oldCmdsSize) return oldCmds[idx];
    return NULL;
}

static void expandCommandHistory(void)
{
    if(!oldCmds || oldCmdsSize > oldCmdsMax)
    {
        if(!oldCmdsMax)
            oldCmdsMax = 16;
        else
            oldCmdsMax *= 2;

        oldCmds = (char**)realloc(oldCmds, oldCmdsMax * sizeof *oldCmds);
        if(!oldCmds) Con_Error("expandCommandHistory: Failed on (re)allocation of %lu bytes.", (unsigned long) (oldCmdsMax * sizeof *oldCmds));
    }
}

static void addCommandToHistory(const char* cmd)
{
    char** oldCmdAdr;
    size_t cmdLength;

    if(!cmd) return;

    cmdLength = strlen(cmd);

    ++oldCmdsSize;
    expandCommandHistory();
    oldCmdAdr = &oldCmds[oldCmdsSize-1];
    *oldCmdAdr = (char*)malloc(cmdLength+1);
    if(!*oldCmdAdr) Con_Error("addCommandToHistory: Failed on allocation of %lu bytes for new command.", (unsigned long) cmdLength);

    memcpy(*oldCmdAdr, cmd, cmdLength);
    (*oldCmdAdr)[cmdLength] = '\0';
}

static void processCmd(byte src)
{
    DD_ClearKeyRepeaters();

    // Add the command line to the oldCmds buffer.
    if(strlen(cmdLine) > 0)
    {
        addCommandToHistory(cmdLine);
        ocPos = oldCmdsSize;
    }

    Con_Execute(src, cmdLine, false, false);
}

static void updateCmdLine(void)
{
    if(ocPos >= oldCmdsSize)
    {
        memset(cmdLine, 0, sizeof(cmdLine));
    }
    else
    {
        strncpy(cmdLine, getCommandFromHistory(ocPos), CMDLINE_SIZE);
    }

    cmdCursor = complPos = (uint) strlen(cmdLine);
}
#endif
//#endif // __CLIENT__

void Con_Open(int yes)
{
#ifdef __CLIENT__
    if(yes)
    {
        ClientWindow::main().console().openLog();
    }
    else
    {
        ClientWindow::main().console().closeLog();
    }
#endif

#ifdef __SERVER__
    DENG_UNUSED(yes);
#endif

    /*
    if(isDedicated)
        yes = true;

    Rend_ConsoleOpen(yes);
    if(yes)
    {
        ConsoleActive = true;
        ConsoleTime = 0;
        bLineOff = 0;
        memset(cmdLine, 0, sizeof(cmdLine));
        cmdCursor = 0;
    }
    else
    {
        complPos = 0;
        lastCompletion = 0;
        ocPos = oldCmdsSize;
        ConsoleActive = false;
    }

#ifdef __CLIENT__
    B_ActivateContext(B_ContextByName(CONSOLE_BINDING_CONTEXT_NAME), yes);
#endif*/
}

#if 0
void Con_Resize(void)
{
    if(!ConsoleInited) return;
    Con_ResizeHistoryBuffer();
    Rend_ConsoleResize(true/*force*/);
}

#ifdef __CLIENT__
static void insertOnCommandLine(byte ch)
{
    size_t len = strlen(cmdLine);

    // If not in insert mode, push the rest of the command-line forward.
    if(!cmdInsMode)
    {
        assert(len <= CMDLINE_SIZE);
        if(len == CMDLINE_SIZE)
            return; // Can't place character.

        if(cmdCursor < len)
        {
            memmove(cmdLine + cmdCursor + 1, cmdLine + cmdCursor, CMDLINE_SIZE - cmdCursor);

            // The last char is always zero, though.
            cmdLine[CMDLINE_SIZE] = 0;
        }
    }

    cmdLine[cmdCursor] = ch;
    if(cmdCursor < CMDLINE_SIZE)
    {
        ++cmdCursor;
        // Do we need to replace the terminator?
        if(cmdCursor == len + 1)
            cmdLine[cmdCursor] = 0;
    }
    complPos = cmdCursor;
}
#endif

boolean Con_Responder(const ddevent_t* ev)
{
#ifdef __CLIENT__
    // The console is only interested in keyboard toggle events.
    if(!IS_KEY_TOGGLE(ev))
        return false;

    if(App_GameLoaded())
    {
        // Special console key: Shift-Escape opens the Control Panel.
        if(!conInputLock && shiftDown && IS_TOGGLE_DOWN_ID(ev, DDKEY_ESCAPE))
        {
            Con_Execute(CMDS_DDAY, "panel", true, false);
            return true;
        }

        if(!ConsoleActive)
        {
            // We are only interested in the activation key (without Shift).
            if(IS_TOGGLE_DOWN_ID(ev, consoleActiveKey) && !shiftDown)
            {
                Con_Open(true);
                return true;
            }
            return false;
        }
    }
    else if(!ConsoleActive)
    {
        // Any key will open the console.
        if(!App_GameLoaded() && IS_TOGGLE_DOWN(ev))
        {
            Con_Open(true);
            return true;
        }
        return false;
    }

    // All keyups are eaten by the console.
    if(IS_TOGGLE_UP(ev))
    {
        if(!shiftDown && conInputLock)
            conInputLock = false; // Release the lock.
        return true;
    }

    // We only want keydown events.
    if(!IS_KEY_PRESS(ev))
        return false;

    // In this case the console is active and operational.
    // Check the shutdown key.
    if(!conInputLock)
    {
        if(ev->toggle.id == consoleActiveKey)
        {
            if(altDown) // Alt-Tilde to fullscreen and halfscreen.
            {
                Rend_ConsoleToggleFullscreen();
                return true;
            }
            if(!shiftDown)
            {
                Con_Open(false);
                return true;
            }
        }
        else
        {
            switch(ev->toggle.id)
            {
            case DDKEY_ESCAPE:
                // Hitting Escape in the console closes it.
                Con_Open(false);
                return false; // Let the menu know about this.

            case DDKEY_PGUP:
                if(shiftDown)
                {
                    Rend_ConsoleMove(-3);
                    return true;
                }
                break;

            case DDKEY_PGDN:
                if(shiftDown)
                {
                    Rend_ConsoleMove(3);
                    return true;
                }
                break;

            default:
                break;
            }
        }
    }

    switch(ev->toggle.id)
    {
    case DDKEY_UPARROW:
        if(conInputLock)
            break;

        if(ocPos != 0)
            ocPos--;
        // Update the command line.
        updateCmdLine();
        return true;

    case DDKEY_DOWNARROW:
        if(conInputLock)
            break;

        if(ocPos < oldCmdsSize)
            ocPos++;

        updateCmdLine();
        return true;

    case DDKEY_PGUP: {
        uint num;

        if(conInputLock)
            break;

        num = CBuffer_NumLines(histBuf);
        if(num > 0)
        {
            bLineOff = MIN_OF(bLineOff + 3, num - 1);
        }
        return true;
      }
    case DDKEY_PGDN:
        if(conInputLock)
            break;

        bLineOff = MAX_OF((int)bLineOff - 3, 0);
        return true;

    case DDKEY_END:
        if(conInputLock)
            break;
        bLineOff = 0;
        return true;

    case DDKEY_HOME:
        if(conInputLock)
            break;
        bLineOff = CBuffer_NumLines(histBuf);
        if(bLineOff != 0)
            bLineOff--;
        return true;

    case DDKEY_RETURN:
    case DDKEY_ENTER:
        if(conInputLock)
            break;

        // Return to the bottom.
        bLineOff = 0;

        // Print the command line with yellow text.
        Con_FPrintf(CPF_YELLOW, ">%s\n", cmdLine);
        // Process the command line.
        processCmd(CMDS_CONSOLE);
        // Clear it.
        memset(cmdLine, 0, sizeof(cmdLine));
        cmdCursor = 0;
        complPos = 0;
        Rend_ConsoleCursorResetBlink();
        return true;

    case DDKEY_INS:
        if(conInputLock)
            break;

        cmdInsMode = !cmdInsMode; // Toggle text insert mode.
        return true;

    case DDKEY_DEL:
        if(conInputLock)
            break;

        if(cmdLine[cmdCursor] != 0)
        {
            memmove(cmdLine + cmdCursor, cmdLine + cmdCursor + 1,
                    CMDLINE_SIZE - cmdCursor + 1);
            complPos = cmdCursor;
            Rend_ConsoleCursorResetBlink();
        }
        return true;

    case DDKEY_BACKSPACE:
        if(conInputLock)
            break;

        if(cmdCursor > 0)
        {
            memmove(cmdLine + cmdCursor - 1, cmdLine + cmdCursor, CMDLINE_SIZE + 1 - cmdCursor);
            cmdCursor--;
            complPos = cmdCursor;
            Rend_ConsoleCursorResetBlink();
        }
        return true;

    case DDKEY_TAB:
        if(cmdLine[0] != 0)
        {
            int mode;

            if(shiftDown) // one time toggle of completion mode.
            {
                mode = (conCompMode == 0)? 1 : 0;
                conInputLock = true; // prevent most user input.
            }
            else
            {
                mode = conCompMode;
            }

            // Attempt to complete the word.
            completeWord(mode);
            if(0 == mode)
                bLineOff = 0;
            Rend_ConsoleCursorResetBlink();
        }
        return true;

    case DDKEY_LEFTARROW:
        if(conInputLock)
            break;

        if(cmdCursor > 0)
        {
            if(shiftDown)
                cmdCursor = 0;
            else
                cmdCursor--;
        }
        complPos = cmdCursor;
        Rend_ConsoleCursorResetBlink();
        break;

    case DDKEY_RIGHTARROW:
        if(conInputLock)
            break;

        if(cmdCursor < CMDLINE_SIZE)
        {
            if(cmdLine[cmdCursor] == 0)
            {
                if(oldCmdsSize != 0 && ocPos > 0)
                {
                    const char* oldCmd = getCommandFromHistory(ocPos - 1);
                    size_t oldCmdLength = (oldCmd? strlen(oldCmd) : 0);
                    if(oldCmd && cmdCursor < oldCmdLength)
                    {
                        cmdLine[cmdCursor] = oldCmd[cmdCursor];
                        cmdCursor++;
                    }
                }
            }
            else
            {
                if(shiftDown)
                    cmdCursor = (uint) strlen(cmdLine);
                else
                    cmdCursor++;
            }
        }

        complPos = cmdCursor;
        Rend_ConsoleCursorResetBlink();
        break;

    case DDKEY_F5:
        if(conInputLock)
            break;

        Con_Execute(CMDS_DDAY, "clear", true, false);
        break;

    default: { // Check for a character.
        int i;

        if(conInputLock)
            break;

        if(ev->toggle.id == 'c' && altDown) // Alt+C: clear the current cmdline
        {
            /// @todo  Make this a binding?
            memset(cmdLine, 0, sizeof(cmdLine));
            cmdCursor = 0;
            complPos = 0;
            Rend_ConsoleCursorResetBlink();
            return true;
        }

        if(ev->toggle.text[0])
        {
            // Insert any text specified in the event.
            for(i = 0; ev->toggle.text[i]; ++i)
            {
                insertOnCommandLine(ev->toggle.text[i]);
            }

            Rend_ConsoleCursorResetBlink();
        }
        return true;

#if 0 // old mapping
        ch = ev->toggle.id;
        ch = DD_ModKey(ch);
        if(ch < 32 || (ch > 127 && ch < DD_HIGHEST_KEYCODE))
            return true;
#endif
    }}
#else
    DENG_UNUSED(ev);
#endif // __CLIENT__
    // The console is very hungry for keys...
    return true;
}
#endif

void Con_PrintRuler(void)
{
    /*
    if(!ConsoleInited || ConsoleSilent)
        return;

    CBuffer_Write(histBuf, CBLF_RULER, NULL);

    if(consoleDump)
    {
    */

    LogBuffer_Msg(_E(R) "\n");

    //}
}

/// @param flags  @ref consolePrintFlags
static void conPrintf(int flags, const char* format, va_list args)
{
    const char* text = 0;

    if(format && format[0] && args)
    {
        if(prbuff == NULL)
            prbuff = (char *) M_Malloc(PRBUFF_SIZE);

        // Format the message to prbuff.
        dd_vsnprintf(prbuff, PRBUFF_SIZE, format, args);
        text = prbuff;

        if(consoleDump)
        {
            LogBuffer_Msg(prbuff);
        }
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

    /*
    if(!isDedicated && !novideo)
    {
        int cblFlags = 0;

        // Translate print flags:
        if(flags & CPF_BLACK)   cblFlags |= CBLF_BLACK;
        if(flags & CPF_BLUE)    cblFlags |= CBLF_BLUE;
        if(flags & CPF_GREEN)   cblFlags |= CBLF_GREEN;
        if(flags & CPF_CYAN)    cblFlags |= CBLF_CYAN;
        if(flags & CPF_RED)     cblFlags |= CBLF_RED;
        if(flags & CPF_MAGENTA) cblFlags |= CBLF_MAGENTA;
        if(flags & CPF_YELLOW)  cblFlags |= CBLF_YELLOW;
        if(flags & CPF_WHITE)   cblFlags |= CBLF_WHITE;
        if(flags & CPF_LIGHT)   cblFlags |= CBLF_LIGHT;
        if(flags & CPF_CENTER)  cblFlags |= CBLF_CENTER;

        CBuffer_Write(histBuf, cblFlags, text);

        if(consoleSnapBackOnPrint)
        {
            // Now that something new has been printed, it will be shown.
            bLineOff = 0;
        }
    }*/
}

void Con_Printf(const char* format, ...)
{
    va_list args;
    if(!ConsoleInited || ConsoleSilent)
        return;
    if(!format || !format[0])
        return;
    va_start(args, format);
    conPrintf(CPF_WHITE, format, args);
    va_end(args);
}

void Con_FPrintf(int flags, const char* format, ...)
{
    if(!ConsoleInited || ConsoleSilent)
        return;

    if(!format || !format[0])
        return;

    {va_list args;
    va_start(args, format);
    conPrintf(flags, format, args);
    va_end(args);}
}

static void printListPath(const ddstring_t* path, int flags, int index)
{
    assert(path);
    if(flags & PPF_TRANSFORM_PATH_PRINTINDEX)
        Con_Printf("%i: ", index);
    Con_Printf("%s", (flags & PPF_TRANSFORM_PATH_MAKEPRETTY)? F_PrettyPath(Str_Text(path)) : Str_Text(path));
}

void Con_PrintPathList4(const char* pathList, char delimiter, const char* separator, int flags)
{
    assert(pathList && pathList[0]);
    {
    const char* p = pathList;
    ddstring_t path;
    int n = 0;
    Str_Init(&path);
    while((p = Str_CopyDelim2(&path, p, delimiter, CDF_OMIT_DELIMITER)))
    {
        printListPath(&path, flags, n++);
        if(separator && !(flags & PPF_MULTILINE) && p && p[0])
            Con_Printf("%s", separator);
        if(flags & PPF_MULTILINE)
            Con_Printf("\n");
    }
    if(Str_Length(&path) != 0)
    {
        printListPath(&path, flags, n++);
        if(flags & PPF_MULTILINE)
            Con_Printf("\n");
    }
    Str_Free(&path);
    }
}

void Con_PrintPathList3(const char* pathList, char delimiter, const char* separator)
{
    Con_PrintPathList4(pathList, delimiter, separator, DEFAULT_PRINTPATHFLAGS);
}

void Con_PrintPathList2(const char* pathList, char delimiter)
{
    Con_PrintPathList3(pathList, delimiter, " ");
}

void Con_PrintPathList(const char* pathList)
{
    Con_PrintPathList2(pathList, ';');
}

#undef Con_Message
void Con_Message(char const *message, ...)
{
    if(!message[0]) return;

    va_list argptr;
    char *buffer = (char *) M_Malloc(PRBUFF_SIZE);
    va_start(argptr, message);
    dd_vsnprintf(buffer, PRBUFF_SIZE, message, argptr);
    va_end(argptr);

    // Ensure the message has a terminating new line character.
    int messageLen = qstrlen(buffer);
    if(messageLen && buffer[messageLen - 1] != '\n')
        std::strcat(buffer, "\n");

    // These messages are always dumped. If consoleDump is set,
    // Con_Printf() will dump the message for us.
    if(!consoleDump)
    {
        //printf("%s", buffer);
        LogBuffer_Msg(buffer);
    }

    // Also print in the console.
    Con_Printf("%s", buffer);

    M_Free(buffer);
}

void Con_Error(char const *error, ...)
{
    static boolean errorInProgress = false;

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
    //fprintf(outFile, "%s\n", err);
    LogBuffer_Msg(err);
    LogBuffer_Msg("\n");

    strcpy(buff, "");
    /*
    if(histBuf != NULL)
    {
        // Flush anything still in the write buffer.
        CBuffer_Flush(histBuf);
        numBufLines = CBuffer_NumLines(histBuf);
        for(i = 5; i > 1; i--)
        {
            const cbline_t* cbl = CBuffer_GetLine(histBuf, numBufLines - i);
            if(!cbl || !cbl->text) continue;
            strcat(buff, cbl->text);
            strcat(buff, "\n");
        }
    }
    */
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
    Sys_Shutdown();

#ifdef __CLIENT__
    DisplayMode_Shutdown();

    DENG2_GUI_APP->loop().pause();

    // This is an abnormal shutdown, we cannot continue drawing any of the
    // windows. (Alternatively could hide/disable drawing of the windows.) Note
    // that the app's event loop is running normally while we show the native
    // message box below.
    ClientApp::windowSystem().closeAll();
#endif

    if(message) // Only show if a message given.
    {
        // Make sure all the buffered stuff goes into the file.
        LogBuffer_Flush();

        /// @todo Get the actual output filename (might be a custom one).
        Sys_MessageBoxWithDetailsFromFile(MBT_ERROR, DOOMSDAY_NICENAME, message,
                                          "See Details for complete messsage log contents.",
                                          de::LogBuffer::appBuffer().outputFile().toUtf8());
    }

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
    boolean remove = false;

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

    //Con_PrintRuler();

    LOG_MSG(_E(1) DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT " Console");

#define TABBED(A, B) "\n" _E(Ta) _E(b) "  " << A << " " _E(.) _E(Tb) << B

#ifdef __CLIENT__
    LOG_MSG(_E(D) "Keys:" _E(.))
            << TABBED("Esc", "Open/close the taskbar")
            << TABBED("Shift-Esc", "Open the console")
            << TABBED("F5", "Clear the console message history")
            << TABBED("Home", "Jump to beginning of line")
            << TABBED("End", "Jump to end of line")
            << TABBED("PgUp/Dn", "Scroll up/down in the history, or expand the history to full height")
            << TABBED("Shift-PgUp/Dn", "Jump to the top/bottom of the history");
#endif
    LOG_MSG(_E(D) "Getting started:");
    LOG_MSG("  " _E(>) "Enter " _E(b) "help (what)" _E(.) " for information about " _E(l) "(what)");
    LOG_MSG("  " _E(>) "Enter " _E(b) "listcmds" _E(.) " to list available commands");
    LOG_MSG("  " _E(>) "Enter " _E(b) "listgames" _E(.) " to list installed games and their status");
    LOG_MSG("  " _E(>) "Enter " _E(b) "listvars" _E(.) " to list available variables");
    //Con_PrintRuler();

#undef TABBED

    return true;
}

D_CMD(Clear)
{
    DENG2_UNUSED3(src, argc, argv);

    /*
    CBuffer_Clear(histBuf);
    bLineOff = 0;*/

#ifdef __CLIENT__
    ClientWindow::main().console().clearLog();
#endif
    return true;
}

D_CMD(Version)
{
    DENG2_UNUSED3(src, argc, argv);

    //Con_Printf("%s %s\n", DOOMSDAY_NICENAME, DOOMSDAY_VERSION_FULLTEXT);
    //Con_Printf("Homepage: %s\n", DOOMSDAY_HOMEURL);
    //Con_Printf("Project homepage: %s\n", DENGPROJECT_HOMEURL);

    LOG_MSG(_E(D) DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_FULLTEXT);
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
    if(Updater_IsDownloadInProgress())
    {
        Con_Message("Cannot quit while downloading update.");
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

    // Defer this decision to the loaded game.
    return gx.TryShutdown();
}

D_CMD(Alias)
{
    DENG2_UNUSED(src);

    if(argc != 3 && argc != 2)
    {
        Con_Printf("Usage: %s (alias) (cmd)\n", argv[0]);
        Con_Printf("Example: alias bigfont \"font size 3\".\n");
        Con_Printf("Use %%1-%%9 to pass the alias arguments to the command.\n");
        return true;
    }

    Con_Alias(argv[1], argc == 3 ? argv[2] : NULL);
    if(argc != 3)
        Con_Printf("Alias '%s' deleted.\n", argv[1]);

    return true;
}

D_CMD(Parse)
{
    DENG2_UNUSED(src);

    int     i;

    for(i = 1; i < argc; ++i)
    {
        Con_Printf("Parsing %s.\n", argv[i]);
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
        Con_Printf("%s\n", argv[i]);
#if _DEBUG
        printf("%s\n", argv[i]);
#endif
    }
    return true;
}

static boolean cvarAddSub(const char* name, float delta, boolean force)
{
    cvar_t* cvar = Con_FindVariable(name);
    float val;

    if(!cvar)
    {
        if(name && name[0])
            Con_Printf("%s is not a known (cvar) name.\n", name);
        return false;
    }

    if(cvar->flags & CVF_READ_ONLY)
    {
        Con_Printf("%s (cvar) is read-only. It can not be changed (not even with force).\n", name);
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

    boolean             force = false;
    float               delta = 0;

    if(argc <= 2)
    {
        Con_Printf("Usage: %s (cvar) (val) (force)\n", argv[0]);
        Con_Printf("Use force to make cvars go off limits.\n");
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

    boolean force = false;
    cvar_t* cvar;
    float val;

    if(argc == 1)
    {
        Con_Printf("Usage: %s (cvar) (force)\n", argv[0]);
        Con_Printf("Use force to make cvars go off limits.\n");
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
        Con_Printf("%s (cvar) is read-only. It can't be changed (not even with force)\n", argv[1]);
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
    boolean     isTrue = false;

    if(argc != 5 && argc != 6)
    {
        Con_Printf("Usage: %s (cvar) (operator) (value) (cmd) (else-cmd)\n",
                   argv[0]);
        Con_Printf("Operator must be one of: not, =, >, <, >=, <=.\n");
        Con_Printf("The (else-cmd) can be omitted.\n");
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
        Con_Error("CCmdIf: Invalid cvar type %i.", (int)var->type);
        exit(1); // Unreachable.
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

#if 0
D_CMD(Font)
{
    DENG2_UNUSED(src);

    if(argc == 1 || argc > 3)
    {
        int listCount, i;
        ddstring_t** list;

        Con_Printf("Usage: %s (cmd) (args)\n", argv[0]);
        Con_Printf("Commands: default, leading, name, size, tracking, xsize, ysize.\n");
        Con_Printf("Names: ");
        list = Fonts_CollectNames(&listCount);
        for(i = 0; i < listCount-1; ++i)
        {
            Con_Printf("%s, ", Str_Text(list[i]));
            Str_Delete(list[i]);
        }
        Con_Printf("%s.\n", Str_Text(list[i]));
        Str_Delete(list[i]);
        free(list);
        Con_Printf("Size 1.0 is normal.\n");
        return true;
    }

    if(!stricmp(argv[1], "default"))
    {
        de::Uri uri(R_ChooseFixedFont(), RC_NULL);
        fontid_t newFont = Fonts_ResolveUri(reinterpret_cast<uri_s *>(&uri));
        if(newFont)
        {
            Con_SetFont(newFont);
            Con_SetFontScale(1, 1);
            Con_SetFontLeading(1);
            Con_SetFontTracking(0);
        }
        return true;
    }

    if(!stricmp(argv[1], "name") && argc == 3)
    {
        de::Uri uri(String(argv[2]), RC_NULL);
        fontid_t newFont = Fonts_ResolveUri2(reinterpret_cast<uri_s *>(&uri), true/*quiet please*/);
        if(newFont)
        {
            QScopedPointer<de::Uri> uri(reinterpret_cast<de::Uri *>(Fonts_ComposeUri(newFont)));
            Con_SetFont(newFont);
            if(!uri->scheme().compareWithoutCase("Game"))
            {
                Con_SetFontScale(1.5f, 2);
                Con_SetFontLeading(1.25f);
                Con_SetFontTracking(1);
            }
            return true;
        }
        Con_Printf("Unknown font '%s'\n", argv[2]);
        return true;
    }

    if(argc == 3)
    {
        if(!stricmp(argv[1], "leading"))
        {
            Con_SetFontLeading(strtod(argv[2], NULL));
        }
        else if(!stricmp(argv[1], "tracking"))
        {
            Con_SetFontTracking(strtod(argv[2], NULL));
        }
        else
        {
            int axes = 0;

            if(!stricmp(argv[1], "size"))       axes |= 0x1|0x2;
            else if(!stricmp(argv[1], "xsize")) axes |= 0x1;
            else if(!stricmp(argv[1], "ysize")) axes |= 0x2;

            if(axes != 0)
            {
                float newScale[2] = { 1, 1 };
                if(axes == 0x1 || axes == (0x1|0x2))
                {
                    newScale[0] = strtod(argv[2], NULL);
                    if(newScale[0] <= 0)
                        newScale[0] = 1;
                }
                if(axes == 0x2)
                {
                    newScale[1] = strtod(argv[2], NULL);
                    if(newScale[1] <= 0)
                        newScale[1] = 1;
                }
                else if(axes == (0x1|0x2))
                {
                    newScale[1] = newScale[0];
                }

                Con_SetFontScale(newScale[0], newScale[1]);
            }
        }

        return true;
    }

    return false;
}
#endif

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

    Con_Error("Fatal error.\n");
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

    Con_Message,

    Con_Printf,
    Con_FPrintf,
    Con_PrintRuler,
    Con_Error,

    Con_SetPrintFilter,

    DD_Execute,
    DD_Executef,
};
