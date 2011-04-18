/**\file con_config.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Config Files.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

static filename_t cfgFile;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void writeHeaderComment(FILE* file)
{
    if(DD_IsNullGameInfo(DD_GameInfo()))
        fprintf(file, "# " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT "\n");
    else
        fprintf(file, "# %s %s / " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT "\n",
                (char*) gx.GetVariable(DD_PLUGIN_NAME),
                (char*) gx.GetVariable(DD_PLUGIN_VERSION_SHORT));

    fprintf(file, "# This configuration file is generated automatically. Each line is a\n");
    fprintf(file, "# console command. Lines beginning with # are comments. Use autoexec.cfg\n");
    fprintf(file, "# for your own startup commands.\n\n");
}

static int writeVariableToFileWorker(const knownword_t* word, void* paramaters)
{
    assert(word && paramaters);
    {
    FILE* file = (FILE*)paramaters;
    ddcvar_t* cvar = (ddcvar_t*)word->data;

    if(cvar->flags & CVF_NO_ARCHIVE)
        return 0; // Continue iteration.

    // First print the comment (help text).
    { const char* str;
    if((str = DH_GetString(DH_Find(cvar->name), HST_DESCRIPTION)))
        M_WriteCommented(file, str);
    }

    fprintf(file, "%s ", cvar->name);
    if(cvar->flags & CVF_PROTECTED)
        fprintf(file, "force ");
    if(cvar->type == CVT_BYTE)
        fprintf(file, "%d", *(byte*) cvar->ptr);
    if(cvar->type == CVT_INT)
        fprintf(file, "%d", *(int*) cvar->ptr);
    if(cvar->type == CVT_FLOAT)
        fprintf(file, "%s", M_TrimmedFloat(*(float*) cvar->ptr));
    if(cvar->type == CVT_CHARPTR)
    {
        fprintf(file, "\"");
        if(*(char**) cvar->ptr)
            M_WriteTextEsc(file, *(char**) cvar->ptr);
        fprintf(file, "\"");
    }
    fprintf(file, "\n\n");
    return 0; // Continue iteration.
    }
}

static __inline void writeVariablesToFile(FILE* file)
{
    Con_IterateKnownWords(0, WT_CVAR, writeVariableToFileWorker, file);
}

static int writeAliasToFileWorker(const knownword_t* word, void* paramaters)
{
    assert(word && paramaters);
    {
    FILE* file = (FILE*) paramaters;
    calias_t* cal = (calias_t*) word->data;

    fprintf(file, "alias \"");
    M_WriteTextEsc(file, cal->name);
    fprintf(file, "\" \"");
    M_WriteTextEsc(file, cal->command);
    fprintf(file, "\"\n");
    return 0; // Continue iteration.
    }
}

static __inline void writeAliasesToFile(FILE* file)
{
    Con_IterateKnownWords(0, WT_CALIAS, writeAliasToFileWorker, file);
}

static boolean writeConsoleState(const char* fileName)
{
    if(!fileName || !fileName[0])
        return false;

    { FILE* file;
    if((file = fopen(fileName, "wt")))
    {
        writeHeaderComment(file);
        fprintf(file, "#\n# CONSOLE VARIABLES\n#\n\n");
        writeVariablesToFile(file);

        fprintf(file, "\n#\n# ALIASES\n#\n\n");
        writeAliasesToFile(file);
        fclose(file);
        return true;
    }}
    Con_Message("writeConsoleState: Can't open %s for writing.\n", fileName);
    return false;
}

static boolean writeBindingsState(const char* fileName)
{
    if(!fileName || !fileName[0])
        return false;
    { FILE* file;
    if((file = fopen(fileName, "wt")))
    {
        writeHeaderComment(file);
        B_WriteToFile(file);
        fclose(file);
        return true;
    }}
    Con_Message("Con_WriteState: Can't open %s for writing.\n", fileName);
    return false;
}

boolean Con_ParseCommands(const char* fileName, boolean setdefault)
{
    DFILE* file;
    char buff[512];   

    // Is this supposed to be the default?
    if(setdefault)
    {
        strncpy(cfgFile, fileName, FILENAME_T_MAXLEN);
        cfgFile[FILENAME_T_LASTINDEX] = '\0';
    }

    // Open the file.
    if(!(file = F_Open(fileName, "rt")))
        return false;

    VERBOSE(Con_Printf("Con_ParseCommands: %s (def:%i)\n", M_PrettyPath(fileName), setdefault));

    // This file is filled with console commands.
    // Each line is a command.
    { int line = 1;
    for(;;)
    {
        M_ReadLine(buff, 512, file);
        if(buff[0] && !M_IsComment(buff))
        {
            // Execute the commands silently.
            if(!Con_Execute(CMDS_CONFIG, buff, setdefault, false))
                Con_Message("%s(%d): error executing command\n \"%s\"\n", M_PrettyPath(fileName), line, buff);
        }
        if(deof(file))
            break;
        line++;
    }}

    F_Close(file);
    return true;
}

/**
 * Writes the state of the console (variables, bindings, aliases) into
 * the given file, overwriting the previous contents.
 */
boolean Con_WriteState(const char* fileName, const char* bindingsFileName)
{
    VERBOSE( Con_Printf("Con_WriteState: %s %s\n", fileName, bindingsFileName) );

    writeConsoleState(fileName);
    // Bindings go into a separate file.
    writeBindingsState(bindingsFileName);
    return true;
}

/**
 * Saves all bindings, aliases and archiveable console variables.
 * The output file is a collection of console commands.
 */
void Con_SaveDefaults(void)
{
    Con_WriteState(cfgFile, (!isDedicated? Str_Text(GameInfo_BindingConfig(DD_GameInfo())) : 0));
}

D_CMD(WriteConsole)
{
    Con_Message("Writing to %s...\n", argv[1]);
    return !Con_WriteState(argv[1], NULL);
}
