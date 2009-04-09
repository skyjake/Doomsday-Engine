/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * sc_man.c: Hexen script lump parsing.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include <stdlib.h>

#include "jhexen.h"

// MACROS ------------------------------------------------------------------

#define MAX_STRING_SIZE         (64)
#define ASCII_COMMENT           (';')
#define ASCII_QUOTE             (34)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char ScriptName[32];
static char* ScriptBuffer;
static char* ScriptPtr;
static char* ScriptEndPtr;
static char StringBuffer[MAX_STRING_SIZE];
static boolean ScriptOpen = false;
static boolean ScriptFreeCLib; // true = de-allocate using free()
static size_t ScriptSize;
static boolean AlreadyGot = false;
static char* lastReadString;
static int lastReadInteger;
static int sc_Line;
static boolean sc_End;
static boolean sc_Crossed;

// CODE --------------------------------------------------------------------

static void checkOpen(void)
{
    if(ScriptOpen == false)
    {
        Con_Error("SC_ call before SC_Open().");
    }
}

static void openScriptLump(lumpnum_t lump)
{
    SC_Close();

    strcpy(ScriptName, W_LumpName(lump));

    ScriptBuffer = (char *) W_CacheLumpNum(lump, PU_STATIC);
    ScriptSize = W_LumpLength(lump);

    ScriptFreeCLib = false; // De-allocate using Z_Free()

    ScriptPtr = ScriptBuffer;
    ScriptEndPtr = ScriptPtr + ScriptSize;
    sc_Line = 1;
    sc_End = false;
    ScriptOpen = true;
    lastReadString = StringBuffer;
    AlreadyGot = false;
}

static void openScriptFile(char* name)
{
    SC_Close();

    ScriptSize = M_ReadFile(name, (byte **) &ScriptBuffer);
    M_ExtractFileBase(name, ScriptName);
    ScriptFreeCLib = false; // De-allocate using Z_Free()

    ScriptPtr = ScriptBuffer;
    ScriptEndPtr = ScriptPtr + ScriptSize;
    sc_Line = 1;
    sc_End = false;
    ScriptOpen = true;
    lastReadString = StringBuffer;
    AlreadyGot = false;
}

static void openScriptCLib(char* name)
{
    SC_Close();

    ScriptSize = M_ReadFileCLib(name, (byte **) &ScriptBuffer);
    M_ExtractFileBase(name, ScriptName);
    ScriptFreeCLib = true;  // De-allocate using free()

    ScriptPtr = ScriptBuffer;
    ScriptEndPtr = ScriptPtr + ScriptSize;
    sc_Line = 1;
    sc_End = false;
    ScriptOpen = true;
    lastReadString = StringBuffer;
    AlreadyGot = false;
}

/**
 * Loads a script (from the WAD files) and prepares it for parsing.
 */
void SC_OpenLump(lumpnum_t lump)
{
    openScriptLump(lump);
}

/**
 * Loads a script (from a file) and prepares it for parsing.  Uses the
 * zone memory allocator for memory allocation and de-allocation.
 */
void SC_OpenFile(char* name)
{
    openScriptFile(name);
}

/**
 * Loads a script (from a file) and prepares it for parsing.  Uses C
 * library function calls for memory allocation and de-allocation.
 */
void SC_OpenFileCLib(char* name)
{
    openScriptCLib(name);
}

void SC_Close(void)
{
    if(ScriptOpen)
    {
        if(ScriptFreeCLib == true)
        {
            free(ScriptBuffer);
        }
        else
        {
            Z_Free(ScriptBuffer);
        }

        ScriptOpen = false;
    }
}

boolean SC_GetString(void)
{
    char*               text;
    boolean             foundToken;

    checkOpen();
    if(AlreadyGot)
    {
        AlreadyGot = false;
        return true;
    }

    foundToken = false;
    sc_Crossed = false;

    if(ScriptPtr >= ScriptEndPtr)
    {
        sc_End = true;
        return false;
    }

    while(foundToken == false)
    {
        while(*ScriptPtr <= 32)
        {
            if(ScriptPtr >= ScriptEndPtr)
            {
                sc_End = true;
                return false;
            }

            if(*ScriptPtr++ == '\n')
            {
                sc_Line++;
                sc_Crossed = true;
            }
        }

        if(ScriptPtr >= ScriptEndPtr)
        {
            sc_End = true;
            return false;
        }

        if(*ScriptPtr != ASCII_COMMENT)
        {   // Found a token
            foundToken = true;
        }
        else
        {   // Skip comment.
            while(*ScriptPtr++ != '\n')
            {
                if(ScriptPtr >= ScriptEndPtr)
                {
                    sc_End = true;
                    return false;

                }
            }

            sc_Line++;
            sc_Crossed = true;
        }
    }

    text = lastReadString;

    if(*ScriptPtr == ASCII_QUOTE)
    {   // Quoted string.
        ScriptPtr++;
        while(*ScriptPtr != ASCII_QUOTE)
        {
            *text++ = *ScriptPtr++;
            if(ScriptPtr == ScriptEndPtr ||
               text == &lastReadString[MAX_STRING_SIZE - 1])
            {
                break;
            }
        }

        ScriptPtr++;
    }
    else
    {   // Normal string.
        while((*ScriptPtr > 32) && (*ScriptPtr != ASCII_COMMENT))
        {
            *text++ = *ScriptPtr++;
            if(ScriptPtr == ScriptEndPtr ||
               text == &lastReadString[MAX_STRING_SIZE - 1])
            {
                break;
            }
        }
    }

    *text = 0;
    return true;
}

void SC_MustGetString(void)
{
    if(!SC_GetString())
    {
        SC_ScriptError("Missing string.");
    }
}

void SC_MustGetStringName(char* name)
{
    SC_MustGetString();
    if(!SC_Compare(name))
    {
        SC_ScriptError(NULL);
    }
}

boolean SC_GetNumber(void)
{
    char*               stopper;

    checkOpen();
    if(SC_GetString())
    {
        lastReadInteger = strtol(lastReadString, &stopper, 0);
        if(*stopper != 0)
        {
            Con_Error("SC_GetNumber: Bad numeric constant \"%s\".\n"
                      "Script %s, Line %d", lastReadString, ScriptName, sc_Line);
        }

        return true;
    }

    return false;
}

void SC_MustGetNumber(void)
{
    if(!SC_GetNumber())
    {
        SC_ScriptError("Missing integer.");
    }
}

/**
 * Assumes there is a valid string in lastReadString.
 */
void SC_UnGet(void)
{
    AlreadyGot = true;
}

/**
 * @return              Index of the first match to lastReadString from the
 *                      passed array of strings, ELSE @c -1,.
 */
int SC_MatchString(char** strings)
{
    int                 i;

    for(i = 0; *strings != NULL; ++i)
    {
        if(SC_Compare(*strings++))
        {
            return i;
        }
    }

    return -1;
}

int SC_MustMatchString(char** strings)
{
    int                 i;

    i = SC_MatchString(strings);
    if(i == -1)
    {
        SC_ScriptError(NULL);
    }

    return i;
}

boolean SC_Compare(char* text)
{
    if(strcasecmp(text, lastReadString) == 0)
    {
        return true;
    }

    return false;
}

int SC_LastReadInteger(void)
{
    return lastReadInteger;
}

const char* SC_LastReadString(void)
{
    return lastReadString;
}

void SC_ScriptError(char* message)
{
    if(message == NULL)
    {
        message = "Bad syntax.";
    }

    Con_Error("Script error, \"%s\" line %d: %s", ScriptName, sc_Line,
              message);
}
