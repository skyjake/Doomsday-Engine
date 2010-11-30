/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * con_config.c: Config Files
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

static void Con_WriteHeaderComment(FILE* file)
{
    if(DD_IsNullGameInfo(DD_GameInfo()))
        fprintf(file, "# Doomsday Engine " DOOMSDAY_VERSION_TEXT "\n");
    else
        fprintf(file, "# %s / Doomsday Engine " DOOMSDAY_VERSION_TEXT "\n", (char*) gx.GetVariable(DD_GAME_ID));

    fprintf(file, "# This configuration file is generated automatically. Each line is a\n");
    fprintf(file, "# console command. Lines beginning with # are comments. Use autoexec.cfg\n");
    fprintf(file, "# for your own startup commands.\n\n");
}

static boolean writeConsoleState(const char* fileName)
{
    if(!fileName || !fileName[0])
        return false;

    { FILE* file;
    if((file = fopen(fileName, "wt")))
    {
        Con_WriteHeaderComment(file);
        fprintf(file, "#\n# CONSOLE VARIABLES\n#\n\n");

        // We'll write all the console variables that are flagged for archiving.
        { uint i, numCVars = Con_CVarCount();
        for(i = 0; i < numCVars; ++i)
        {
            const cvar_t* var = Con_GetVariableIDX(i);

            if(var->type == CVT_NULL || (var->flags & CVF_NO_ARCHIVE))
                continue;

            // First print the comment (help text).
            { const char* str;
            if((str = DH_GetString(DH_Find(var->name), HST_DESCRIPTION)))
                M_WriteCommented(file, str);
            }

            fprintf(file, "%s ", var->name);
            if(var->flags & CVF_PROTECTED)
                fprintf(file, "force ");
            if(var->type == CVT_BYTE)
                fprintf(file, "%d", *(byte*) var->ptr);
            if(var->type == CVT_INT)
                fprintf(file, "%d", *(int*) var->ptr);
            if(var->type == CVT_FLOAT)
                fprintf(file, "%s", TrimmedFloat(*(float*) var->ptr));
            if(var->type == CVT_CHARPTR)
            {
                fprintf(file, "\"");
                M_WriteTextEsc(file, *(char**) var->ptr);
                fprintf(file, "\"");
            }
            fprintf(file, "\n\n");
        }}

        fprintf(file, "\n#\n# ALIASES\n#\n\n");
        Con_WriteAliasesToFile(file);
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
        Con_WriteHeaderComment(file);
        B_WriteToFile(file);
        fclose(file);
        return true;
    }}
    Con_Message("Con_WriteState: Can't open %s for writing.\n", fileName);
    return false;
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
