/**\file con_data.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * Console databases for cvars, ccmds, aliases and known words.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "de_base.h"
#include "de_console.h"

#include "m_args.h"
#include "m_misc.h"
#include "blockset.h"
//#include "pathdirectory.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(HelpWhat);
D_CMD(ListAliases);
D_CMD(ListCmds);
D_CMD(ListVars);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;

/// Console variable database and search directory.
static uint numCVars = 0;
static cvar_t** cvars = 0;
//static pathdirectory_t* cvarDirectory;

static ddccmd_t* ccmdListHead;
/// \todo Replace with a data structure that allows for deletion of elements.
static blockset_t* ccmdBlockSet;
/// Running total of the number of uniquely-named commands.
static uint numUniqueNamedCCmds;

/// \todo Replace with a BST.
static uint numCAliases;
static calias_t** caliases;

/// The list of known words (for completion).
/// \todo Replace with a persistent self-balancing BST (Treap?)?
static knownword_t* knownWords;
static uint numKnownWords;
static boolean knownWordsNeedUpdate;

// CODE --------------------------------------------------------------------

void Con_DataRegister(void)
{
    C_CMD("help",           "s",    HelpWhat);
    C_CMD("listaliases",    NULL,   ListAliases);
    C_CMD("listcmds",       NULL,   ListCmds);
    C_CMD("listvars",       NULL,   ListVars);
}

static void clearVariables(void)
{
    static char* emptyString = "";

    // Free the data of the data cvars.
    { uint i;
    cvar_t** cvar;
    for(i = 0, cvar = cvars; i < numCVars; ++i, ++cvar)
    {
        if(((*cvar)->flags & CVF_CAN_FREE) &&
           (*cvar)->type == CVT_CHARPTR)
        {
            char** ptr = (char**) (*cvar)->ptr;

            // \note Multiple vars could be using the same pointer (so ensure
            // that we attempt to free only once).
            { uint k;
            for(k = i; k < numCVars; ++k)
                if(cvars[k]->type == CVT_CHARPTR &&
                   *(char**) cvars[k]->ptr == *ptr)
                {
                    cvars[k]->flags &= ~CVF_CAN_FREE;
                }
            }
            free(*ptr);
            *ptr = emptyString;
        }
        free(*cvar);
    }}

    //PathDirectory_Destruct(cvarDirectory); cvarDirectory = 0;
}

/// Construct a new variable from the specified template and add it to the database.
static cvar_t* addVariable(const cvartemplate_t* tpl)
{
    cvar_t* newVar;
    uint idx;

    cvars = realloc(cvars, sizeof(*cvars) * ++numCVars);

    // Find the insertion point.
    for(idx = 0; idx < numCVars-1; ++idx)
        if(stricmp(cvars[idx]->name, tpl->name) > 0)
            break;

    // Make room for the new variable.
    if(idx != numCVars-1)
        memmove(cvars + idx + 1, cvars + idx, sizeof(*cvars) * (numCVars - 1 - idx));

    newVar = cvars[idx] = malloc(sizeof(*newVar));
    newVar->flags = tpl->flags;
    newVar->type = tpl->type;
    newVar->ptr = tpl->ptr;
    newVar->min = tpl->min;
    newVar->max = tpl->max;
    newVar->notifyChanged = tpl->notifyChanged;
    // Make a static copy of the name in the zone (this allows the source
    // data to change in case of dynamic registrations).
    { char* nameCopy = Z_Malloc(strlen(tpl->name) + 1, PU_APPSTATIC, NULL);
    if(NULL == nameCopy)
        Con_Error("Con_AddVariable: Failed on allocation of %lu bytes for variable name.", 
            (unsigned long) (strlen(tpl->name) + 1));
    strcpy(nameCopy, tpl->name);
    newVar->name = nameCopy;
    }

    /*{ ddstring_t path; Str_Init(&path); Str_Set(&path, newVar->name);
    PathDirectory_Insert(cvarDirectory, &path, newVar, '-');
    Str_Free(&path);
    }*/

    knownWordsNeedUpdate = true;
    return newVar;
}

static void clearAliases(void)
{
    if(caliases)
    {
        // Free the alias data.
        calias_t** cal;
        uint i;
        for(i = 0, cal = caliases; i < numCAliases; ++i, ++cal)
        {
            free((*cal)->name);
            free((*cal)->command);
            free(*cal);
        }
        free(caliases);
    }
    caliases = 0;
    numCAliases = 0;
}

static void clearCommands(void)
{
    if(ccmdBlockSet)
        BlockSet_Destruct(ccmdBlockSet);
    ccmdBlockSet = 0;
    ccmdListHead = 0;
    numUniqueNamedCCmds = 0;
}

static void clearKnownWords(void)
{
    if(knownWords)
        free(knownWords);
    knownWords = 0;
    numKnownWords = 0;
    knownWordsNeedUpdate = false;
}

static const char* getKnownWordName(const knownword_t* word)
{
    assert(word);
    switch(word->type)
    {
    case WT_CCMD:     return ((ddccmd_t*)word->data)->name;
    case WT_CVAR:     return ((cvar_t*)word->data)->name;
    case WT_CALIAS:   return ((calias_t*)word->data)->name;
    case WT_GAMEINFO: return Str_Text(GameInfo_IdentityKey((gameinfo_t*)word->data));
    }
    assert(0);
    return 0;
}

static int C_DECL compareKnownWordByName(const void* e1, const void* e2)
{
    return stricmp(getKnownWordName((const knownword_t*)e1),
                   getKnownWordName((const knownword_t*)e2));
}

static boolean removeFromKnownWords(knownwordtype_t type, void* data)
{
    assert(VALID_KNOWNWORDTYPE(type) && data != 0);
    { uint i;
    for(i = 0; i < numKnownWords; ++i)
    {
        knownword_t* word = &knownWords[i];

        if(word->type != type || word->data != data)
            continue;

        if(i != numKnownWords-1)
            memmove(knownWords + i, knownWords + i + 1, sizeof(knownword_t) * (numKnownWords - 1 - i));

        --numKnownWords;
        if(numKnownWords != 0)
        {
            knownWords = realloc(knownWords, sizeof(knownword_t) * numKnownWords);
        }
        else
        {
            free(knownWords); knownWords = 0;
        }
        return true;
    }}
    return false;
}

/**
 * Collate all known words and sort them alphabetically.
 * Commands, variables (except those hidden) and aliases are known words.
 *
 * \todo Quicksort is not the best algorithm for this. We are merging two
 * already sorted lists and one not-sorted.
 *
 * A better solution would be:
 *
 *   1) Add ccmds to knownWords and qsort them.
 *   2) Use a Mergesort-like algorithm when adding the cvars and caliases
 *      to join the three sets into one.
 */
static void updateKnownWords(void)
{
    uint c, knownCVars, knownGames;
    size_t len;

    if(!knownWordsNeedUpdate)
        return;

    // Count the number of visible console variables.
    knownCVars = 0;
    { uint i;
    cvar_t** cvar;
    for(i = 0, cvar = cvars; i < numCVars; ++i, ++cvar)
        if(!((*cvar)->flags & CVF_HIDE))
            ++knownCVars;
    }

    knownGames = 0;
    { int i, gameInfoCount = DD_GameInfoCount();
    for(i = 0; i < gameInfoCount; ++i)
    {
        gameinfo_t* info = DD_GameInfoByIndex(i+1);
        if(!DD_IsNullGameInfo(info))
            ++knownGames;
    }}

    // Build the known words table.
    numKnownWords = numUniqueNamedCCmds + knownCVars + numCAliases + knownGames;
    len = sizeof(knownword_t) * numKnownWords;
    knownWords = realloc(knownWords, len);
    memset(knownWords, 0, len);

    c = 0;
    // Add commands?
    /// \note ccmd list is NOT yet sorted.
    { ddccmd_t* ccmd;
    for(ccmd = ccmdListHead; ccmd; ccmd = ccmd->next)
    {
        if(ccmd->prevOverload)
            continue; // Skip overloaded variants.
        knownWords[c].type = WT_CCMD;
        knownWords[c].data = ccmd;
        ++c;
    }}

    // Add variables?
    if(0 != knownCVars)
    {
        /// \note cvars array is already sorted.
        cvar_t** cvar;
        uint i;
        for(i = 0, cvar = cvars; i < numCVars; ++i, ++cvar)
        {
            if((*cvar)->flags & CVF_HIDE)
                continue;
            knownWords[c].type = WT_CVAR;
            knownWords[c].data = *cvar;
            ++c;
        }
    }

    // Add aliases?
    if(0 != numCAliases)
    {
        /// \note caliases array is already sorted.
        calias_t** cal;
        uint i;
        for(i = 0, cal = caliases; i < numCAliases; ++i, ++cal)
        {
            knownWords[c].type = WT_CALIAS;
            knownWords[c].data = *cal;
            ++c;
        }
    }

    // Add gameinfos?
    if(0 != knownGames)
    {
        int i, gameInfoCount = DD_GameInfoCount();
        for(i = 0; i < gameInfoCount; ++i)
        {
            gameinfo_t* info = DD_GameInfoByIndex(i+1);
            if(DD_IsNullGameInfo(info))
                continue;
            knownWords[c].type = WT_GAMEINFO;
            knownWords[c].data = info;
            ++c;
        }
    }

    // Sort it so we get nice alphabetical word completions.
    qsort(knownWords, numKnownWords, sizeof(knownword_t), compareKnownWordByName);
    knownWordsNeedUpdate = false;
}

void Con_SetString2(const char* name, char* text, int svflags)
{
    assert(inited);
    {
    cvar_t* cvar = Con_FindVariable(name);
    boolean changed = false;

    if(!cvar)
        return;

    if((cvar->flags & CVF_READ_ONLY) && !(svflags & SVF_WRITE_OVERRIDE))
    {
        Con_Printf("%s (cvar) is read-only. It can't be changed "
                   "(not even with force)\n", name);
        return;
    }

    if(cvar->type == CVT_CHARPTR)
    {
        if(!CV_CHARPTR(cvar) && strlen(name) != 0 || stricmp(text, CV_CHARPTR(cvar)))
            changed = true;

        // Free the old string, if one exists.
        if((cvar->flags & CVF_CAN_FREE) && CV_CHARPTR(cvar))
            free(CV_CHARPTR(cvar));
        // Allocate a new string.
        cvar->flags |= CVF_CAN_FREE;
        CV_CHARPTR(cvar) = malloc(strlen(text) + 1);
        strcpy(CV_CHARPTR(cvar), text);

        // Make the change notification callback
        if(cvar->notifyChanged != NULL && changed)
            cvar->notifyChanged();
    }
    else
        Con_Error("Con_SetString: cvar is not of type char*.\n");
    }
}

void Con_SetString(const char* name, char* text)
{
    Con_SetString2(name, text, 0);
}

void Con_SetInteger2(const char* name, int value, int svflags)
{
    assert(inited);
    {
    cvar_t* var = Con_FindVariable(name);
    boolean changed = false;

    if(!var)
        return;

    if((var->flags & CVF_READ_ONLY) && !(svflags & SVF_WRITE_OVERRIDE))
    {
        Con_Printf("%s (cvar) is read-only. It can't be changed "
                   "(not even with force)\n", var->name);
        return;
    }

    switch(var->type)
    {
    case CVT_INT:
        if(CV_INT(var) != value)
            changed = true;
        CV_INT(var) = value;
        break;
    case CVT_BYTE:
        if(CV_BYTE(var) != (byte) value)
            changed = true;
        CV_BYTE(var) = (byte) value;
        break;
    case CVT_FLOAT:
        if(CV_FLOAT(var) != (float) value)
            changed = true;
        CV_FLOAT(var) = (float) value;
        break;
    default:
        Con_Message("Warning:Con_SetInteger: Attempt to set incompatible cvar "
                    "%s to %i, ignoring.\n", var->name, value);
        return;
    }

    // Make a change notification callback?
    if(var->notifyChanged != 0 && changed)
        var->notifyChanged();
    }
}

void Con_SetInteger(const char* name, int value)
{
    Con_SetInteger2(name, value, 0);
}

void Con_SetFloat2(const char* name, float value, int svflags)
{
    assert(inited);
    {
    cvar_t* var = Con_FindVariable(name);
    boolean changed = false;

    if(!var)
        return;

    if((var->flags & CVF_READ_ONLY) && !(svflags & SVF_WRITE_OVERRIDE))
    {
        Con_Printf("%s (cvar) is read-only. It can't be changed "
                   "(not even with force)\n", name);
        return;
    }

    switch(var->type)
    {
    case CVT_INT:
        if(CV_INT(var) != (int) value)
            changed = true;
        CV_INT(var) = (int) value;
        break;
    case CVT_BYTE:
        if(CV_BYTE(var) != (byte) value)
            changed = true;
        CV_BYTE(var) = (byte) value;
        break;
    case CVT_FLOAT:
        if(CV_FLOAT(var) != value)
            changed = true;
        CV_FLOAT(var) = value;
        break;
    default:
        Con_Message("Warning:Con_SetFloat: Attempt to set incompatible cvar "
                    "%s to %g, ignoring.\n", name, value);
        return;
    }

    // Make a change notification callback?
    if(var->notifyChanged != 0 && changed)
        var->notifyChanged();
    }
}

void Con_SetFloat(const char* name, float value)
{
    Con_SetFloat2(name, value, 0);
}

int Con_GetInteger(const char* name)
{
    assert(inited);
    {
    cvar_t* var;
    if((var = Con_FindVariable(name)) != 0)
        switch(var->type)
        {
        case CVT_BYTE:      return CV_BYTE(var);
        case CVT_INT:       return CV_INT(var);
        case CVT_FLOAT:     return CV_FLOAT(var);
        case CVT_CHARPTR:   return strtol(CV_CHARPTR(var), 0, 0);
        }
    return 0;
    }
}

float Con_GetFloat(const char* name)
{
    assert(inited);
    { cvar_t* var;
    if((var = Con_FindVariable(name)) != 0)
        switch(var->type)
        {
        case CVT_BYTE:      return CV_BYTE(var);
        case CVT_INT:       return CV_INT(var);
        case CVT_FLOAT:     return CV_FLOAT(var);
        case CVT_CHARPTR:   return strtod(CV_CHARPTR(var), 0);
        }
    }
    return 0;
}

byte Con_GetByte(const char* name)
{
    assert(inited);
    { cvar_t* var;
    if((var = Con_FindVariable(name)) != 0)
        switch(var->type)
        {
        case CVT_BYTE:      return CV_BYTE(var);
        case CVT_INT:       return CV_INT(var);
        case CVT_FLOAT:     return CV_FLOAT(var);
        case CVT_CHARPTR:   return strtol(CV_CHARPTR(var), 0, 0);
        }
    }
    return 0;
}

char* Con_GetString(const char* name)
{
    cvar_t* var = Con_FindVariable(name);
    if(!var || var->type != CVT_CHARPTR)
        return "";
    return CV_CHARPTR(var);
}

void Con_AddVariable(const cvartemplate_t* tpl)
{   
    assert(inited);
    if(NULL == tpl)
    {
        Con_Message("Warning:Con_AddVariable: Passed invalid value for argument 'tpl', ignoring.\n");
        return;
    }
    if(CVT_NULL == tpl->type)
    {
        Con_Message("Warning:Con_AddVariable: Attempt to register variable \"%s\" as type "
            "CVT_NULL, ignoring.\n", tpl->name);
        return;
    }

    if(Con_FindVariable(tpl->name))
        Con_Error("Error: A CVAR with the name \"%s\" is already registered.", tpl->name);

    addVariable(tpl);
}

void Con_AddVariableList(const cvartemplate_t* tplList)
{
    assert(inited);
    if(!tplList)
    {
        Con_Message("Warning:Con_AddVariableList: Passed invalid value for "
                    "argument 'tplList', ignoring.\n");
        return;
    }
    for(; tplList->name; ++tplList)
    {
        if(Con_FindVariable(tplList->name))
            Con_Error("Error: A CVAR with the name \"%s\" is already registered.", tplList->name);

        addVariable(tplList);
    }
}

cvar_t* Con_FindVariable(const char* name)
{
    assert(inited);
    {
    int result;
    uint bottomIdx, topIdx, pivot;
    cvar_t* var;
    boolean isDone;

    if(numCVars == 0)
        return 0;
    if(!name || !name[0])
        return 0;

    bottomIdx = 0;
    topIdx = numCVars-1;
    var = NULL;
    isDone = false;
    while(bottomIdx <= topIdx && !isDone)
    {
        pivot = bottomIdx + (topIdx - bottomIdx)/2;

        result = stricmp(cvars[pivot]->name, name);
        if(result == 0)
        {   // Found.
            var = cvars[pivot];
            isDone = true;
        }
        else
        {
            if(result > 0)
            {
                if(pivot == 0)
                {   // Not present.
                    isDone = true;
                }
                else
                    topIdx = pivot - 1;
            }
            else
                bottomIdx = pivot + 1;
        }
    }

    return var;
    }
}

/// \note Part of the Doomsday public API
cvartype_t Con_GetVariableType(const char* name)
{
    cvar_t* var = Con_FindVariable(name);
    if(NULL == var) return CVT_NULL;
    return var->type;
}

void Con_PrintCVar(cvar_t* var, char* prefix)
{
    assert(inited);
    {
    char equals = '=';

    if(!var)
        return;

    if((var->flags & CVF_PROTECTED) || (var->flags & CVF_READ_ONLY))
        equals = ':';

    if(prefix)
        Con_Printf("%s", prefix);

    switch(var->type)
    {
    case CVT_BYTE:
        Con_Printf("%s %c %d", var->name, equals, CV_BYTE(var));
        break;
    case CVT_INT:
        Con_Printf("%s %c %d", var->name, equals, CV_INT(var));
        break;
    case CVT_FLOAT:
        Con_Printf("%s %c %g", var->name, equals, CV_FLOAT(var));
        break;
    case CVT_CHARPTR:
        Con_Printf("%s %c \"%s\"", var->name, equals, CV_CHARPTR(var));
        break;
    default:
        Con_Printf("%s (bad type!)", var->name);
        break;
    }
    Con_Printf("\n");
    }
}

void Con_AddCommand(const ccmdtemplate_t* ccmd)
{
    assert(inited);
    {
    int minArgs, maxArgs;
    cvartype_t args[MAX_ARGS];
    ddccmd_t* newCCmd, *overloaded = 0;

    if(!ccmd)
        return;

    if(!ccmd->name)
        Con_Error("Con_AddCommand: CCmd missing a name.");

/*#if _DEBUG
Con_Message("Con_AddCommand: \"%s\" \"%s\" (%i).\n", ccmd->name,
            ccmd->argTemplate, ccmd->flags);
#endif*/

    // Decode the usage string if present.
    if(ccmd->argTemplate != 0)
    {
        size_t l, len;
        cvartype_t type = CVT_NULL;
        boolean unlimitedArgs;
        char c;

        len = strlen(ccmd->argTemplate);
        minArgs = 0;
        unlimitedArgs = false;
        for(l = 0; l < len; ++l)
        {
            c = ccmd->argTemplate[l];
            switch(c)
            {
            // Supported type symbols:
            case 'b': type = CVT_BYTE;     break;
            case 'i': type = CVT_INT;      break;
            case 'f': type = CVT_FLOAT;    break;
            case 's': type = CVT_CHARPTR;  break;

            // Special symbols:
            case '*':
                // Variable arg list.
                if(l != len-1)
                    Con_Error("Con_AddCommand: CCmd '%s': '*' character "
                              "not last in argument template: \"%s\".",
                              ccmd->name, ccmd->argTemplate);

                unlimitedArgs = true;
                type = CVT_NULL; // Not a real argument.
                break;

            // Erroneous symbol:
            default:
                Con_Error("Con_AddCommand: CCmd '%s': Invalid character "
                          "'%c' in argument template: \"%s\".", ccmd->name, c,
                          ccmd->argTemplate);
            }

            if(type != CVT_NULL)
            {
                if(minArgs >= MAX_ARGS)
                    Con_Error("Con_AddCommand: CCmd '%s': Too many arguments. "
                              "Limit is %i.", ccmd->name, MAX_ARGS);

                args[minArgs++] = type;
            }
        }

        // Set the min/max parameter counts for this ccmd.
        if(unlimitedArgs)
        {
            maxArgs = -1;
            if(minArgs == 0)
                minArgs = -1;
        }
        else
            maxArgs = minArgs;

/*#if _DEBUG
{
int i;
Con_Message("Con_AddCommand: CCmd \"%s\": minArgs %i, maxArgs %i: \"",
            ccmd->name, minArgs, maxArgs);
for(i = 0; i < minArgs; ++i)
{
    switch(args[i])
    {
    case CVT_BYTE:    c = 'b'; break;
    case CVT_INT:     c = 'i'; break;
    case CVT_FLOAT:   c = 'f'; break;
    case CVT_CHARPTR: c = 's'; break;
    }
    Con_Printf("%c", c);
}
Con_Printf("\".\n");
}
#endif*/
    }
    else // It's usage is NOT validated by Doomsday.
    {
        minArgs = maxArgs = -1;

/*#if _DEBUG
if(ccmd->args == NULL)
  Con_Message("Con_AddCommand: CCmd \"%s\" will not have it's usage "
              "validated.\n", ccmd->name);
#endif*/
    }

    // Now check that the ccmd to be registered is unique.
    // We allow multiple ccmds with the same name if we can determine by
    // their paramater lists that they are unique (overloading).
    { ddccmd_t* other;
    if((other = Con_FindCommand(ccmd->name)) != 0)
    {
        boolean unique = true;

        // The ccmd being registered is NOT a deng validated ccmd
        // and there is already an existing ccmd by this name?
        if(minArgs == -1 && maxArgs == -1)
            unique = false;

        if(unique)
        {
            // Check each variant.
            ddccmd_t* variant = other;
            do
            {
                // An existing ccmd with no validation?
                if(variant->minArgs == -1 && variant->maxArgs == -1)
                    unique = false;
                // An existing ccmd with a lower minimum and no maximum?
                else if(variant->minArgs < minArgs && variant->maxArgs == -1)
                    unique = false;
                // An existing ccmd with a larger min and this ccmd has no max?
                else if(variant->minArgs > minArgs && maxArgs == -1)
                    unique = false;
                // An existing ccmd with the same minimum number of args?
                else if(variant->minArgs == minArgs)
                {
                    // \todo Implement support for paramater type checking.
                    unique = false;
                }

                // Sanity check.
                if(!unique && variant->execFunc == ccmd->execFunc)
                    Con_Error("Con_AddCommand: A CCmd by the name '%s' is "
                              "already registered and the callback funcs are "
                              "the same, is this really what you wanted?",
                              ccmd->name);
            } while((variant = variant->nextOverload) != 0);
        }

        if(!unique)
            Con_Error("Con_AddCommand: A CCmd by the name '%s' "
                      "is already registered. Their parameter lists "
                      "would be ambiguant.", ccmd->name);

        overloaded = other;
    }}

    if(!ccmdBlockSet)
        ccmdBlockSet = BlockSet_Construct(sizeof(ddccmd_t), 32);
    newCCmd = BlockSet_Allocate(ccmdBlockSet);
    // Make a static copy of the name in the zone (this allows the source
    // data to change in case of dynamic registrations).
    { char* nameCopy = Z_Malloc(strlen(ccmd->name) + 1, PU_APPSTATIC, NULL);
    if(NULL == nameCopy)
        Con_Error("Con_AddCommand: Failed on allocation of %lu bytes for command name.", 
            (unsigned long) (strlen(ccmd->name) + 1));
    strcpy(nameCopy, ccmd->name);
    newCCmd->name = nameCopy;
    }
    newCCmd->execFunc = ccmd->execFunc;
    newCCmd->flags = ccmd->flags;
    newCCmd->nextOverload = newCCmd->prevOverload = 0;
    newCCmd->minArgs = minArgs;
    newCCmd->maxArgs = maxArgs;
    memcpy(newCCmd->args, &args, sizeof(newCCmd->args));

    // Link it to the head of the global list of ccmds.
    newCCmd->next = ccmdListHead;
    ccmdListHead = newCCmd;

    if(!overloaded)
    {
        ++numUniqueNamedCCmds;
        knownWordsNeedUpdate = true;
        return;
    }

    // Link it to the head of the overload list.
    newCCmd->nextOverload = overloaded;
    overloaded->prevOverload = newCCmd;
    }
}

void Con_AddCommandList(const ccmdtemplate_t* cmdList)
{
    if(!cmdList)
        return;

    for(; cmdList->name; ++cmdList)
        Con_AddCommand(cmdList);
}

ddccmd_t* Con_FindCommand(const char* name)
{
    assert(inited);
    // \todo Use a faster than O(n) linear search.
    if(name && name[0])
    {
        ddccmd_t* ccmd;
        for(ccmd = ccmdListHead; ccmd; ccmd = ccmd->next)
            if(!stricmp(name, ccmd->name))
            {
                // Locate the head of the overload list.
                while(ccmd->prevOverload) { ccmd = ccmd->prevOverload; }
                return ccmd;
            }
    }
    return 0;
}

ddccmd_t* Con_FindCommandMatchArgs(cmdargs_t* args)
{
    assert(inited);

    if(!args)
        return 0;

    { ddccmd_t* ccmd;
    if((ccmd = Con_FindCommand(args->argv[0])) != 0)
    {
        ddccmd_t* variant = ccmd;
        // Check each variant.
        do
        {
            boolean invalidArgs = false;
            // Are we validating the arguments?
            // \note Strings are considered always valid.
            if(!(variant->minArgs == -1 && variant->maxArgs == -1))
            {
                int j;

                // Do we have the right number of arguments?
                if(args->argc-1 < variant->minArgs)
                    invalidArgs = true;
                else if(variant->maxArgs != -1 && args->argc-1 > variant->maxArgs)
                    invalidArgs = true;
                else
                {
                    // Presently we only validate upto the minimum number of args.
                    // \todo Validate non-required args.
                    for(j = 0; j < variant->minArgs && !invalidArgs; ++j)
                    {
                        switch(variant->args[j])
                        {
                        case CVT_BYTE:
                            invalidArgs = !M_IsStringValidByte(args->argv[j+1]);
                            break;
                        case CVT_INT:
                            invalidArgs = !M_IsStringValidInt(args->argv[j+1]);
                            break;
                        case CVT_FLOAT:
                            invalidArgs = !M_IsStringValidFloat(args->argv[j+1]);
                            break;
                        default:
                            break;
                        }
                    }
                }
            }

            if(!invalidArgs)
                return variant; // This is the one!
        } while((variant = variant->nextOverload) != 0);

        // Perhaps the user needs some help.
        Con_PrintCCmdUsage(ccmd, false);
    }}

    // No command found, or none with matching arguments.
    return 0;
}

boolean Con_IsValidCommand(const char* name)
{
    if(!name || !name[0])
        return false;

    // Try the console commands first.
    if(Con_FindCommand(name) != 0)
        return true;

    // Try the aliases (aliai?) then.
    return (Con_FindAlias(name) != 0);
}

void Con_PrintCCmdUsage(ddccmd_t* ccmd, boolean showExtra)
{
    assert(inited);

    if(!ccmd || (ccmd->minArgs == -1 && ccmd->maxArgs == -1))
        return;

    // Print the expected form for this ccmd.
    Con_Printf("Usage: %s", ccmd->name);
    { int i;
    for(i = 0; i < ccmd->minArgs; ++i)
    {
        switch(ccmd->args[i])
        {
        case CVT_BYTE:      Con_Printf(" (byte)");      break;
        case CVT_INT:       Con_Printf(" (int)");       break;
        case CVT_FLOAT:     Con_Printf(" (float)");     break;
        case CVT_CHARPTR:   Con_Printf(" (string)");    break;
        default: break;
        }
    }}
    if(ccmd->maxArgs == -1)
        Con_Printf(" ...");
    Con_Printf("\n");

    if(showExtra)
    {
        // Check for extra info about this ccmd's usage.
        char* str;
        if((str = DH_GetString(DH_Find(ccmd->name), HST_INFO)))
        {
            // Lets indent for neatness.
            char* line;
            while(*(line = M_StrTok(&str, "\n")))
                Con_Printf("  %s\n", line);
        }
    }
}

calias_t* Con_FindAlias(const char* name)
{
    assert(inited);
    {
    uint bottomIdx, topIdx, pivot;
    calias_t* cal;
    boolean isDone;
    int result;

    if(numCAliases == 0)
        return 0;
    if(!name || !name[0])
        return 0;

    bottomIdx = 0;
    topIdx = numCAliases-1;
    cal = NULL;
    isDone = false;
    while(bottomIdx <= topIdx && !isDone)
    {
        pivot = bottomIdx + (topIdx - bottomIdx)/2;

        result = stricmp(caliases[pivot]->name, name);
        if(result == 0)
        {
            // Found.
            cal = caliases[pivot];
            isDone = true;
        }
        else
        {
            if(result > 0)
            {
                if(pivot == 0)
                {
                    // Not present.
                    isDone = true;
                }
                else
                    topIdx = pivot - 1;
            }
            else
                bottomIdx = pivot + 1;
        }
    }

    return cal;
    }
}

calias_t* Con_AddAlias(const char* name, const char* command)
{
    assert(inited);
    {
    calias_t* newAlias;
    uint idx;

    if(!name || !name[0] || !command || !command[0])
        return 0;

    caliases = realloc(caliases, sizeof(*caliases) * ++numCAliases);

    // Find the insertion point.
    for(idx = 0; idx < numCAliases-1; ++idx)
        if(stricmp(caliases[idx]->name, name) > 0)
            break;

    // Make room for the new alias.
    if(idx != numCAliases-1)
        memmove(caliases + idx + 1, caliases + idx, sizeof(*caliases) * (numCAliases - 1 - idx));

    // Add the new variable, making a static copy of the name in the zone (this allows
    // the source data to change in case of dynamic registrations).
    newAlias = caliases[idx] = malloc(sizeof(*newAlias));
    newAlias->name = malloc(strlen(name) + 1);
    strcpy(newAlias->name, name);
    newAlias->command = malloc(strlen(command) + 1);
    strcpy(newAlias->command, command);

    knownWordsNeedUpdate = true;
    return newAlias;
    }
}

void Con_DeleteAlias(calias_t* cal)
{
    assert(inited && cal);
    {
    uint idx;

    for(idx = 0; idx < numCAliases; ++idx)
        if(caliases[idx] == cal)
            break;
    if(idx == numCAliases)
        return;

    // Try to avoid rebuilding known words by simply removing ourselves.
    if(!knownWordsNeedUpdate)
        removeFromKnownWords(WT_CALIAS, (void*)cal);

    free(cal->name);
    free(cal->command);
    free(cal);

    if(idx < numCAliases - 1)
        memmove(caliases + idx, caliases + idx + 1, sizeof(*caliases) * (numCAliases - idx - 1));
    --numCAliases;
    }
}

int Con_IterateKnownWords(const char* pattern, knownwordtype_t type,
    int (*callback) (const knownword_t* word, void* paramaters), void* paramaters)
{
    assert(inited && callback);
    {
    knownwordtype_t matchType = (VALID_KNOWNWORDTYPE(type)? type : WT_ANY);
    size_t patternLength = (pattern? strlen(pattern) : 0);
    int result = 0;

    updateKnownWords();

    { uint i;
    for(i = 0; i < numKnownWords; ++i)
    {
        const knownword_t* word = &knownWords[i];
        if(matchType != WT_ANY && word->type != type)
            continue;
        if(patternLength != 0 && strnicmp(getKnownWordName(word), pattern, patternLength))
            continue;
        if((result = callback(word, paramaters)) != 0)
            break;
    }}

    return result;
    }
}

static int countMatchedWordWorker(const knownword_t* word, void* paramaters)
{
    assert(word && paramaters);
    {
    uint* count = (uint*) paramaters;
    ++(*count);
    return 0; // Continue iteration.
    }
}

typedef struct {
    const knownword_t** matches; /// Matched word array.
    uint index; /// Current position in the collection.
} collectmatchedwordworker_paramaters_t;

static int collectMatchedWordWorker(const knownword_t* word, void* paramaters)
{
    assert(word && paramaters);
    {
    collectmatchedwordworker_paramaters_t* p = (collectmatchedwordworker_paramaters_t*) paramaters;
    p->matches[p->index++] = word;
    return 0; // Continue iteration.
    }
}

const knownword_t** Con_CollectKnownWordsMatchingWord(const char* word,
    knownwordtype_t type, uint* count)
{
    assert(inited);
    {
    uint localCount = 0;

    Con_IterateKnownWords(word, type, countMatchedWordWorker, &localCount);

    if(count)
        *count = localCount;

    if(localCount != 0)
    {
        collectmatchedwordworker_paramaters_t p;

        // Allocate the array, plus one for the terminator.
        p.matches = malloc(sizeof(*p.matches) * (localCount + 1));
        p.index = 0;

        // Collect the pointers.
        Con_IterateKnownWords(word, type, collectMatchedWordWorker, &p);

        // Terminate.
        p.matches[localCount] = 0;

        return p.matches;
    }
    return 0; // No matches.
    }
}

void Con_InitDatabases(void)
{
    if(inited) return;

    // Create the empty variable directory now.
    //cvarDirectory = PathDirectory_ConstructDefault();
    numCVars = 0;
    cvars = 0;

    ccmdListHead = 0;
    ccmdBlockSet = 0;
    numUniqueNamedCCmds = 0;

    numCAliases = 0;
    caliases = 0;

    knownWords = 0;
    numKnownWords = 0;
    knownWordsNeedUpdate = false;

    inited = true;
}

void Con_ShutdownDatabases(void)
{
    if(!inited)
        return;

    clearKnownWords();
    clearAliases();
    clearCommands();
    clearVariables();

    inited = false;
}

D_CMD(HelpWhat)
{
    uint found = 0;

    if(!stricmp(argv[1], "(what)"))
    {
        Con_Printf("You've got to be kidding!\n");
        return true;
    }

    // Try the console commands first.
    { ddccmd_t* ccmd;
    if((ccmd = Con_FindCommand(argv[1])) != 0)
    {
        char* str;
        if((str = DH_GetString(DH_Find(ccmd->name), HST_DESCRIPTION)))
            Con_Printf("%s\n", str);

        // Print usage info for each variant.
        do
        {
            Con_PrintCCmdUsage(ccmd, (found == 0));
            found++;
        } while((ccmd = ccmd->nextOverload) != 0);
    }}

    if(found == 0) // Perhaps its a cvar then?
    {
        cvar_t* cvar;
        if((cvar = Con_FindVariable(argv[1])) != 0)
        {
            char* str;
            if((str = DH_GetString(DH_Find(cvar->name), HST_DESCRIPTION)) != 0)
            {
                Con_Printf("%s\n", str);
                found = true;
            }
        }
    }

    if(found == 0) // Maybe an alias?
    {
        calias_t* calias;
        if((calias = Con_FindAlias(argv[1])) != 0)
        {
            Con_Printf("An alias for:\n%s\n", calias->command);
            found = true;
        }
    }

    if(found == 0) // Still not found?
        Con_Printf("There is no help about '%s'.\n", argv[1]);

    return true;
}

static int printKnownWordWorker(const knownword_t* word, void* paramaters)
{
    assert(word);
    switch(word->type)
    {
      case WT_CCMD: {
        ddccmd_t* ccmd = (ddccmd_t*) word->data;
        char* str;

        if(ccmd->prevOverload)
            return 0; // Skip overloaded variants.

        if((str = DH_GetString(DH_Find(ccmd->name), HST_DESCRIPTION)))
            Con_FPrintf(CBLF_LIGHT|CBLF_YELLOW, "  %s (%s)\n", ccmd->name, str);
        else
            Con_FPrintf(CBLF_LIGHT|CBLF_YELLOW, "  %s\n", ccmd->name);
        break;
      }
      case WT_CVAR: {
        cvar_t* cvar = (cvar_t*) word->data;

        if(cvar->flags & CVF_HIDE)
            return 0; // Skip hidden variables.

        Con_PrintCVar(cvar, "  ");
        break;
      }
      case WT_CALIAS: {
        calias_t* cal = (calias_t*) word->data;
        Con_FPrintf(CBLF_LIGHT|CBLF_YELLOW, "  %s == %s\n", cal->name, cal->command);
        break;
      }
      case WT_GAMEINFO: {
        gameinfo_t* info = (gameinfo_t*) word->data;
        Con_FPrintf(CBLF_LIGHT|CBLF_BLUE, "  %s\n", Str_Text(GameInfo_IdentityKey(info)));
        break;
      }
    }
    return 0; // Continue iteration.
}

D_CMD(ListCmds)
{
    Con_Printf("Console commands:\n");
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CCMD, printKnownWordWorker, 0);
    return true;
}

D_CMD(ListVars)
{
    Con_Printf("Console variables:\n");
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CVAR, printKnownWordWorker, 0);
    return true;
}

D_CMD(ListAliases)
{
    Con_Printf("Aliases:\n");
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CALIAS, printKnownWordWorker, 0);
    return true;
}
