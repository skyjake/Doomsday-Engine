/** @file con_config.cpp Config files.
 * @ingroup console
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <ctype.h>

#include "de_platform.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"
#include "de_filesys.h"

#include "dd_main.h"
#include "dd_def.h"
#include "dd_help.h"
#include "dd_games.h"

static filename_t cfgFile;
static int flagsAllow = 0;

static void writeHeaderComment(FILE* file)
{
    if(!DD_GameLoaded())
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
    FILE* file = (FILE*)paramaters;
    cvar_t* var = (cvar_t*)word->data;
    AutoStr* path;
    assert(file && var);

    if(var->flags & CVF_NO_ARCHIVE)
        return 0; // Continue iteration.

    path = CVar_ComposePath(var);
    // First print the comment (help text).
    { const char* str;
    if(NULL != (str = DH_GetString(DH_Find(Str_Text(path)), HST_DESCRIPTION)))
        M_WriteCommented(file, str);
    }

    fprintf(file, "%s ", Str_Text(path));
    if(var->flags & CVF_PROTECTED)
        fprintf(file, "force ");
    switch(var->type)
    {
    case CVT_BYTE:
        fprintf(file, "%d", *(byte*) var->ptr);
        break;
    case CVT_INT:
        fprintf(file, "%d", *(int*) var->ptr);
        break;
    case CVT_FLOAT:
        fprintf(file, "%s", M_TrimmedFloat(*(float*) var->ptr));
        break;
    case CVT_CHARPTR:
        fprintf(file, "\"");
        if(CV_CHARPTR(var))
            M_WriteTextEsc(file, CV_CHARPTR(var));
        fprintf(file, "\"");
        break;
    case CVT_URIPTR:
        fprintf(file, "\"");
        if(CV_URIPTR(var))
        {
            AutoStr* valPath = Uri_Compose(CV_URIPTR(var));
            fprintf(file, "%s", Str_Text(valPath));
        }
        fprintf(file, "\"");
        break;
    default: break;
    }
    fprintf(file, "\n\n");

    return 0; // Continue iteration.
}

static __inline void writeVariablesToFile(FILE* file)
{
    Con_IterateKnownWords(0, WT_CVAR, writeVariableToFileWorker, file);
}

static int writeAliasToFileWorker(const knownword_t* word, void* paramaters)
{
    FILE* file = (FILE*) paramaters;
    calias_t* cal = (calias_t*) word->data;
    assert(file && cal);

    fprintf(file, "alias \"");
    M_WriteTextEsc(file, cal->name);
    fprintf(file, "\" \"");
    M_WriteTextEsc(file, cal->command);
    fprintf(file, "\"\n");
    return 0; // Continue iteration.
}

static __inline void writeAliasesToFile(FILE* file)
{
    Con_IterateKnownWords(0, WT_CALIAS, writeAliasToFileWorker, file);
}

static boolean writeConsoleState(const char* fileName)
{
    ddstring_t nativePath, fileDir;
    FILE* file;
    if(!fileName || !fileName[0]) return false;

    VERBOSE(Con_Message("Writing state to \"%s\"...\n", fileName));

    Str_Init(&nativePath);
    Str_Set(&nativePath, fileName);
    F_ToNativeSlashes(&nativePath, &nativePath);

    // Ensure the destination directory exists.
    Str_Init(&fileDir);
    F_FileDir(&fileDir, &nativePath);
    if(Str_Length(&fileDir))
    {
        F_MakePath(Str_Text(&fileDir));
    }
    Str_Free(&fileDir);

    file = fopen(Str_Text(&nativePath), "wt");
    Str_Free(&nativePath);
    if(file)
    {
        writeHeaderComment(file);
        fprintf(file, "#\n# CONSOLE VARIABLES\n#\n\n");
        writeVariablesToFile(file);

        fprintf(file, "\n#\n# ALIASES\n#\n\n");
        writeAliasesToFile(file);
        fclose(file);
        return true;
    }
    Con_Message("Warning:writeConsoleState: Failed opening \"%s\" for writing.\n", F_PrettyPath(fileName));
    return false;
}

static boolean writeBindingsState(const char* fileName)
{
    ddstring_t nativePath, fileDir;
    FILE* file;
    if(!fileName || !fileName[0]) return false;

    VERBOSE(Con_Message("Writing bindings to \"%s\"...\n", fileName));

    Str_Init(&nativePath);
    Str_Set(&nativePath, fileName);
    F_ToNativeSlashes(&nativePath, &nativePath);

    // Ensure the destination directory exists.
    Str_Init(&fileDir);
    F_FileDir(&fileDir, &nativePath);
    if(Str_Length(&fileDir))
    {
        F_MakePath(Str_Text(&fileDir));
    }
    Str_Free(&fileDir);

    file = fopen(Str_Text(&nativePath), "wt");
    Str_Free(&nativePath);
    if(file)
    {
        writeHeaderComment(file);
        B_WriteToFile(file);
        fclose(file);
        return true;
    }
    Con_Message("Warning:writeBindingsState: Failed opening \"%s\" for writing.\n", F_PrettyPath(fileName));
    return false;
}

boolean Con_ParseCommands(const char* fileName)
{
    return Con_ParseCommands2(fileName, 0);
}

boolean Con_ParseCommands2(const char* fileName, int flags)
{
    FileHandle* file;
    char buff[512];
    boolean setdefault = (flags & CPCF_SET_DEFAULT) != 0;

    // Is this supposed to be the default?
    if(setdefault)
    {
        strncpy(cfgFile, fileName, FILENAME_T_MAXLEN);
        cfgFile[FILENAME_T_LASTINDEX] = '\0';
    }

    // Update the allowed operations.
    flagsAllow |= flags & (CPCF_ALLOW_SAVE_STATE | CPCF_ALLOW_SAVE_BINDINGS);

    // Open the file.
    file = F_Open(fileName, "rt");
    if(!file)
    {
        VERBOSE(Con_Message("Could not open: \"%s\"\n", fileName));
        return false;
    }

    VERBOSE(Con_Message("Con_ParseCommands: %s (def:%i)\n", F_PrettyPath(fileName), setdefault));

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
                Con_Message("%s(%d): error executing command\n \"%s\"\n", F_PrettyPath(fileName), line, buff);
        }
        if(FileHandle_AtEnd(file))
            break;
        line++;
    }}

    F_Delete(file);

    return true;
}

/**
 * Writes the state of the console (variables, bindings, aliases) into
 * the given file, overwriting the previous contents.
 */
boolean Con_WriteState(const char* fileName, const char* bindingsFileName)
{
    if(fileName && (flagsAllow & CPCF_ALLOW_SAVE_STATE))
    {
        writeConsoleState(fileName);
    }
    if(bindingsFileName && (flagsAllow & CPCF_ALLOW_SAVE_BINDINGS))
    {
        // Bindings go into a separate file.
        writeBindingsState(bindingsFileName);
    }
    return true;
}

/**
 * Saves all bindings, aliases and archiveable console variables.
 * The output file is a collection of console commands.
 */
void Con_SaveDefaults(void)
{
    Con_WriteState(cfgFile, (!isDedicated? Str_Text(Game_BindingConfig(App_CurrentGame())) : 0));
}

D_CMD(WriteConsole)
{
    DENG2_UNUSED2(src, argc);

    Con_Message("Writing to \"%s\"...\n", argv[1]);
    return !Con_WriteState(argv[1], NULL);
}
