/**\file con_data.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include "cbuffer.h"
#include "m_misc.h"
#include "blockset.h"
#include "pathdirectory.h"

// MACROS ------------------------------------------------------------------

// Substrings in CVar names are delimited by this character.
#define CVARDIRECTORY_DELIMITER         '-'

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(HelpWhat);
D_CMD(HelpApropos);
D_CMD(ListAliases);
D_CMD(ListCmds);
D_CMD(ListVars);
#if _DEBUG
D_CMD(PrintVarStats);
#endif

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;

/// Console variable directory.
static PathDirectory* cvarDirectory;

static ccmd_t* ccmdListHead;
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

static char* emptyString = "";
static Uri* emptyUri;

// CODE --------------------------------------------------------------------

void Con_DataRegister(void)
{
    C_CMD("help",           "s",    HelpWhat);
    C_CMD("apropos",        "s",    HelpApropos);
    C_CMD("listaliases",    NULL,   ListAliases);
    C_CMD("listcmds",       NULL,   ListCmds);
    C_CMD("listvars",       NULL,   ListVars);
#if _DEBUG
    C_CMD("varstats",       NULL,   PrintVarStats);
#endif
}

static int markVariableUserDataFreed(PathDirectoryNode* node, void* parameters)
{
    assert(node && parameters);
    {
    cvar_t* var = PathDirectoryNode_UserData(node);
    void** ptr = (void**) parameters;
    if(var)
    switch(CVar_Type(var))
    {
    case CVT_CHARPTR:
        if(*ptr == CV_CHARPTR(var)) var->flags &= ~CVF_CAN_FREE;
        break;
    case CVT_URIPTR:
        if(*ptr == CV_URIPTR(var)) var->flags &= ~CVF_CAN_FREE;
        break;
    default: break;
    }
    return 0; // Continue iteration.
    }
}

static int clearVariable(PathDirectoryNode* node, void* parameters)
{
    cvar_t* var = (cvar_t*)PathDirectoryNode_UserData(node);

    DENG_UNUSED(parameters);

    if(var)
    {
        assert(PT_LEAF == PathDirectoryNode_Type(node));

        // Detach our user data from this node.
        PathDirectoryNode_SetUserData(node, 0);

        if(CVar_Flags(var) & CVF_CAN_FREE)
        {
            void** ptr = NULL;
            switch(CVar_Type(var))
            {
            case CVT_CHARPTR:
                if(!CV_CHARPTR(var)) break;

                ptr = (void**)var->ptr;
                /// @note Multiple vars could be using the same pointer (so only free once).
                PathDirectory_Iterate2(cvarDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, markVariableUserDataFreed, ptr);
                free(*ptr); *ptr = emptyString;
                break;
            case CVT_URIPTR:
                if(!CV_URIPTR(var)) break;

                ptr = (void**)var->ptr;
                /// @note Multiple vars could be using the same pointer (so only free once).
                PathDirectory_Iterate2(cvarDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, markVariableUserDataFreed, ptr);
                Uri_Delete((Uri*)*ptr); *ptr = emptyUri;
                break;
            default: {
#if _DEBUG
                ddstring_t* path = CVar_ComposePath(var);
                Con_Message("Warning:clearVariable: Attempt to free user data for non-pointer type variable %s [%p], ignoring.\n",
                    Str_Text(path), (void*)var);
                Str_Delete(path);
#endif
                break;
              }
            }
        }
        free(var);
    }
    return 0; // Continue iteration.
}

static void clearVariables(void)
{
    /// If _DEBUG we'll traverse all nodes and verify our clear logic.
#if _DEBUG
    int flags = 0;
#else
    int flags = PCF_NO_BRANCH;
#endif
    PathDirectory_Iterate(cvarDirectory, flags, NULL, PATHDIRECTORY_NOHASH, clearVariable);
    PathDirectory_Clear(cvarDirectory);
}

/// Construct a new variable from the specified template and add it to the database.
static cvar_t* addVariable(const cvartemplate_t* tpl)
{
    PathDirectoryNode* node = PathDirectory_Insert(cvarDirectory, tpl->path, CVARDIRECTORY_DELIMITER);
    cvar_t* newVar;

    if(PathDirectoryNode_UserData(node))
    {
        Con_Error("Con_AddVariable: A variable with path '%s' is already known!", tpl->path);
        return NULL; // Unreachable.
    }

    newVar = (cvar_t*) malloc(sizeof *newVar);
    if(!newVar)
        Con_Error("Con_AddVariable: Failed on allocation of %lu bytes for new CVar.", (unsigned long) sizeof *newVar);

    newVar->flags = tpl->flags;
    newVar->type = tpl->type;
    newVar->ptr = tpl->ptr;
    newVar->min = tpl->min;
    newVar->max = tpl->max;
    newVar->notifyChanged = tpl->notifyChanged;
    newVar->directoryNode = node;
    PathDirectoryNode_SetUserData(node, newVar);

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
        BlockSet_Delete(ccmdBlockSet);
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

static int C_DECL compareKnownWordByName(const void* a, const void* b)
{
    const knownword_t* wA = (const knownword_t*)a;
    const knownword_t* wB = (const knownword_t*)b;
    ddstring_t* textAString = NULL, *textBString = NULL;
    const char* textA, *textB;
    int result;

    switch(wA->type)
    {
    case WT_CALIAS:   textA = ((calias_t*)wA->data)->name; break;
    case WT_CCMD:     textA = ((ccmd_t*)wA->data)->name; break;
    case WT_CVAR:
        textAString = CVar_ComposePath((cvar_t*)wA->data);
        textA = Str_Text(textAString);
        break;
    case WT_GAME: textA = Str_Text(Game_IdentityKey((Game*)wA->data)); break;
    default:
        Con_Error("compareKnownWordByName: Invalid type %i for word A.", wA->type);
        exit(1); // Unreachable
    }

    switch(wB->type)
    {
    case WT_CALIAS:   textB = ((calias_t*)wB->data)->name; break;
    case WT_CCMD:     textB = ((ccmd_t*)wB->data)->name; break;
    case WT_CVAR:
        textBString = CVar_ComposePath((cvar_t*)wB->data);
        textB = Str_Text(textBString);
        break;
    case WT_GAME: textB = Str_Text(Game_IdentityKey((Game*)wB->data)); break;
    default:
        Con_Error("compareKnownWordByName: Invalid type %i for word B.", wB->type);
        exit(1); // Unreachable
    }

    result = stricmp(textA, textB);

    if(NULL != textAString) Str_Delete(textAString);
    if(NULL != textBString) Str_Delete(textBString);

    return result;
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

typedef struct {
    uint count;
    cvartype_t type;
    boolean hidden;
    boolean ignoreHidden;
} countvariableparams_t;

static int countVariable(const PathDirectoryNode* node, void* parameters)
{
    assert(NULL != node && NULL != parameters);
    {
    countvariableparams_t* p = (countvariableparams_t*) parameters;
    cvar_t* var = PathDirectoryNode_UserData(node);
    if(!(p->ignoreHidden && (var->flags & CVF_HIDE)))
    {
        if(!VALID_CVARTYPE(p->type) && !p->hidden)
        {
            if(!p->ignoreHidden || !(var->flags & CVF_HIDE))
                ++(p->count);
        }
        else if((p->hidden && (var->flags & CVF_HIDE)) ||
                (VALID_CVARTYPE(p->type) && p->type == CVar_Type(var)))
        {
            ++(p->count);
        }
    }
    return 0; // Continue iteration.
    }
}

static int addVariableToKnownWords(const PathDirectoryNode* node, void* parameters)
{
    assert(NULL != node && NULL != parameters);
    {
    cvar_t* var = PathDirectoryNode_UserData(node);
    uint* index = (uint*) parameters;
    if(NULL != var && !(var->flags & CVF_HIDE))
    {
        knownWords[*index].type = WT_CVAR;
        knownWords[*index].data = var;
        ++(*index);
    }
    return 0; // Continue iteration.
    }
}

/**
 * Collate all known words and sort them alphabetically.
 * Commands, variables (except those hidden) and aliases are known words.
 */
static void updateKnownWords(void)
{
    countvariableparams_t countCVarParams;
    int i, gameCount;
    ccmd_t* ccmd;
    size_t len;
    uint c;

    if(!knownWordsNeedUpdate) return;

    // Count the number of visible console variables.
    countCVarParams.count = 0;
    countCVarParams.type = -1;
    countCVarParams.hidden = false;
    countCVarParams.ignoreHidden = true;
    PathDirectory_Iterate2_Const(cvarDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, countVariable, &countCVarParams);

    // Build the known words table.
    numKnownWords = numUniqueNamedCCmds + countCVarParams.count + numCAliases + DD_GameCount();
    len = sizeof(knownword_t) * numKnownWords;
    knownWords = realloc(knownWords, len);
    memset(knownWords, 0, len);

    c = 0;
    // Add commands?
    /// \note ccmd list is NOT yet sorted.
    for(ccmd = ccmdListHead; ccmd; ccmd = ccmd->next)
    {
        // Skip overloaded variants.
        if(ccmd->prevOverload) continue;

        knownWords[c].type = WT_CCMD;
        knownWords[c].data = ccmd;
        ++c;
    }

    // Add variables?
    if(0 != countCVarParams.count)
    {
        /// \note cvars are NOT sorted.
        PathDirectory_Iterate2_Const(cvarDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, addVariableToKnownWords, &c);
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

    // Add games?
    gameCount = DD_GameCount();
    for(i = 0; i < gameCount; ++i)
    {
        Game* game = DD_GameByIndex(i+1);

        knownWords[c].type = WT_GAME;
        knownWords[c].data = game;
        ++c;
    }

    // Sort it so we get nice alphabetical word completions.
    qsort(knownWords, numKnownWords, sizeof(knownword_t), compareKnownWordByName);
    knownWordsNeedUpdate = false;
}

const ddstring_t* CVar_TypeName(cvartype_t type)
{
    static const ddstring_t names[CVARTYPE_COUNT] = {
        { "invalid" },
        { "CVT_BYTE" },
        { "CVT_INT" },
        { "CVT_FLOAT" },
        { "CVT_CHARPTR"},
        { "CVT_URIPTR" }
    };
    return &names[(VALID_CVARTYPE(type)? type : 0)];
}

cvartype_t CVar_Type(const cvar_t* var)
{
    assert(var);
    return var->type;
}

int CVar_Flags(const cvar_t* var)
{
    assert(var);
    return var->flags;
}

ddstring_t* CVar_ComposePath(const cvar_t* var)
{
    assert(var);
    return PathDirectory_ComposePath(PathDirectoryNode_Directory(var->directoryNode), var->directoryNode, Str_New(), NULL, CVARDIRECTORY_DELIMITER);
}

void CVar_SetUri2(cvar_t* var, const Uri* uri, int svFlags)
{
    assert(var);
    {
    Uri* newUri = NULL;
    boolean changed = false;

    if((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        ddstring_t* path = CVar_ComposePath(var);
        Con_Printf("%s (var) is read-only. It can't be changed (not even with force)\n", Str_Text(path));
        Str_Delete(path);
        return;
    }

    if(var->type != CVT_URIPTR)
    {
        Con_Error("CVar::SetUri: Not of type %s.", Str_Text(CVar_TypeName(CVT_URIPTR)));
        return; // Unreachable.
    }

    if(!CV_URIPTR(var) && !uri)
        return;

    // Compose the new uri.
    newUri = Uri_NewCopy(uri);

    if(!CV_URIPTR(var) || !Uri_Equality(CV_URIPTR(var), newUri))
        changed = true;

    // Free the old uri, if one exists.
    if((var->flags & CVF_CAN_FREE) && CV_URIPTR(var))
        Uri_Delete(CV_URIPTR(var));

    var->flags |= CVF_CAN_FREE;
    CV_URIPTR(var) = newUri;

    // Make the change notification callback
    if(var->notifyChanged != NULL && changed)
        var->notifyChanged();
    }
}

void CVar_SetUri(cvar_t* var, const Uri* uri)
{
    CVar_SetUri2(var, uri, 0);
}

void CVar_SetString2(cvar_t* var, const char* text, int svFlags)
{
    assert(NULL != var);
    {
    boolean changed = false;
    size_t oldLen, newLen;

    if((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        ddstring_t* path = CVar_ComposePath(var);
        Con_Printf("%s (var) is read-only. It can't be changed (not even with force)\n", Str_Text(path));
        Str_Delete(path);
        return;
    }

    if(var->type != CVT_CHARPTR)
    {
        Con_Error("CVar::SetString: Not of type %s.", Str_Text(CVar_TypeName(CVT_CHARPTR)));
        return; // Unreachable.
    }

    oldLen = (!CV_CHARPTR(var)? 0 : strlen(CV_CHARPTR(var)));
    newLen = (!text           ? 0 : strlen(text));

    if(oldLen == 0 && newLen == 0)
        return;

    if(oldLen != newLen || stricmp(text, CV_CHARPTR(var)))
        changed = true;

    // Free the old string, if one exists.
    if((var->flags & CVF_CAN_FREE) && CV_CHARPTR(var))
        free(CV_CHARPTR(var));

    // Allocate a new string.
    var->flags |= CVF_CAN_FREE;
    CV_CHARPTR(var) = (char*)malloc(newLen + 1);
    if(!CV_CHARPTR(var))
        Con_Error("CVar::SetString: Failed on allocation of %lu bytes for new string value.", (unsigned long) (newLen + 1));
    strcpy(CV_CHARPTR(var), text);

    // Make the change notification callback
    if(var->notifyChanged != NULL && changed)
        var->notifyChanged();
    }
}

void CVar_SetString(cvar_t* var, const char* text)
{
    CVar_SetString2(var, text, 0);
}

void CVar_SetInteger2(cvar_t* var, int value, int svFlags)
{
    assert(NULL != var);
    {
    boolean changed = false;

    if((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        ddstring_t* path = CVar_ComposePath(var);
        Con_Printf("%s (var) is read-only. It can't be changed (not even with force).\n", Str_Text(path));
        Str_Delete(path);
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
    default: {
        ddstring_t* path = CVar_ComposePath(var);
        Con_Message("Warning:CVar::SetInteger: Attempt to set incompatible var %s to %i, ignoring.\n", Str_Text(path), value);
        Str_Delete(path);
        return;
      }
    }

    // Make a change notification callback?
    if(var->notifyChanged != 0 && changed)
        var->notifyChanged();
    }
}

void CVar_SetInteger(cvar_t* var, int value)
{
    CVar_SetInteger2(var, value, 0);
}

void CVar_SetFloat2(cvar_t* var, float value, int svFlags)
{
    assert(NULL != var);
    {
    boolean changed = false;

    if((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        ddstring_t* path = CVar_ComposePath(var);
        Con_Printf("%s (cvar) is read-only. It can't be changed (not even with force).\n", Str_Text(path));
        Str_Delete(path);
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
    default: {
        ddstring_t* path = CVar_ComposePath(var);
        Con_Message("Warning:CVar::SetFloat: Attempt to set incompatible cvar %s to %g, ignoring.\n", Str_Text(path), value);
        Str_Delete(path);
        return;
      }
    }

    // Make a change notification callback?
    if(var->notifyChanged != 0 && changed)
        var->notifyChanged();
    }
}

void CVar_SetFloat(cvar_t* var, float value)
{
    CVar_SetFloat2(var, value, 0);
}

int CVar_Integer(const cvar_t* var)
{
    assert(var);
    switch(var->type)
    {
    case CVT_BYTE:      return CV_BYTE(var);
    case CVT_INT:       return CV_INT(var);
    case CVT_FLOAT:     return CV_FLOAT(var);
    case CVT_CHARPTR:   return strtol(CV_CHARPTR(var), 0, 0);
    default: {
#if _DEBUG
        ddstring_t* path = CVar_ComposePath(var);
        Con_Message("Warning:CVar::Integer: Attempted on incompatible variable %s [%p type:%s], returning 0\n",
            Str_Text(path), (void*)var, Str_Text(CVar_TypeName(CVar_Type(var))));
        Str_Delete(path);
#endif
        return 0;
      }
    }
}

float CVar_Float(const cvar_t* var)
{
    assert(var);
    switch(var->type)
    {
    case CVT_BYTE:      return CV_BYTE(var);
    case CVT_INT:       return CV_INT(var);
    case CVT_FLOAT:     return CV_FLOAT(var);
    case CVT_CHARPTR:   return strtod(CV_CHARPTR(var), 0);
    default: {
#if _DEBUG
        ddstring_t* path = CVar_ComposePath(var);
        Con_Message("Warning:CVar::Float: Attempted on incompatible variable %s [%p type:%s], returning 0\n",
            Str_Text(path), (void*)var, Str_Text(CVar_TypeName(CVar_Type(var))));
        Str_Delete(path);
#endif
        return 0;
      }
    }
}

byte CVar_Byte(const cvar_t* var)
{
    assert(var);
    switch(var->type)
    {
    case CVT_BYTE:      return CV_BYTE(var);
    case CVT_INT:       return CV_INT(var);
    case CVT_FLOAT:     return CV_FLOAT(var);
    case CVT_CHARPTR:   return strtol(CV_CHARPTR(var), 0, 0);
    default: {
#if _DEBUG
        ddstring_t* path = CVar_ComposePath(var);
        Con_Message("Warning:CVar::Byte: Attempted on incompatible variable %s [%p type:%s], returning 0\n",
            Str_Text(path), (void*)var, Str_Text(CVar_TypeName(CVar_Type(var))));
        Str_Delete(path);
#endif
        return 0;
      }
    }
}

char* CVar_String(const cvar_t* var)
{
    assert(var);
    /// \todo Why not implement in-place value to string conversion?
    switch(var->type)
    {
    case CVT_CHARPTR:   return CV_CHARPTR(var);
    default: {
#if _DEBUG
        ddstring_t* path = CVar_ComposePath(var);
        Con_Message("Warning:CVar::String: Attempted on incompatible variable %s [%p type:%s], returning emptyString\n",
            Str_Text(path), (void*)var, Str_Text(CVar_TypeName(CVar_Type(var))));
        Str_Delete(path);
#endif
        return emptyString;
      }
    }
}

Uri* CVar_Uri(const cvar_t* var)
{
    assert(var);
    /// \todo Why not implement in-place string to uri conversion?
    switch(var->type)
    {
    case CVT_URIPTR:   return CV_URIPTR(var);
    default: {
#if _DEBUG
        ddstring_t* path = CVar_ComposePath(var);
        Con_Message("Warning:CVar::String: Attempted on incompatible variable %s [%p type:%s], returning emptyUri\n",
            Str_Text(path), (void*)var, Str_Text(CVar_TypeName(CVar_Type(var))));
        Str_Delete(path);
#endif
        return emptyUri;
      }
    }
}

void Con_AddVariable(const cvartemplate_t* tpl)
{
    assert(inited);
    if(!tpl)
    {
        Con_Message("Warning:Con_AddVariable: Passed invalid value for argument 'tpl', ignoring.\n");
        return;
    }
    if(CVT_NULL == tpl->type)
    {
        Con_Message("Warning:Con_AddVariable: Attempt to register variable '%s' as type "
            "%s, ignoring.\n", Str_Text(CVar_TypeName(CVT_NULL)), tpl->path);
        return;
    }

    if(Con_FindVariable(tpl->path))
        Con_Error("Error: A CVAR with the name '%s' is already registered.", tpl->path);

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
    for(; tplList->path; ++tplList)
    {
        if(Con_FindVariable(tplList->path))
            Con_Error("Error: A CVAR with the name '%s' is already registered.", tplList->path);

        addVariable(tplList);
    }
}

cvar_t* Con_FindVariable(const char* path)
{
    PathDirectoryNode* node;
    assert(inited);
    node = PathDirectory_Find(cvarDirectory, PCF_NO_BRANCH|PCF_MATCH_FULL, path, CVARDIRECTORY_DELIMITER);
    if(!node) return NULL;
    return (cvar_t*) PathDirectoryNode_UserData(node);
}

/// \note Part of the Doomsday public API
cvartype_t Con_GetVariableType(const char* path)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return CVT_NULL;
    return var->type;
}

void Con_PrintCVar(cvar_t* var, char* prefix)
{
    assert(inited);
    {
    char equals = '=';
    ddstring_t* path;

    if(!var)
        return;

    if((var->flags & CVF_PROTECTED) || (var->flags & CVF_READ_ONLY))
        equals = ':';

    if(prefix)
        Con_Printf("%s", prefix);

    path = CVar_ComposePath(var);
    switch(var->type)
    {
    case CVT_BYTE:      Con_Printf("%s %c %d",       Str_Text(path), equals, CV_BYTE(var)); break;
    case CVT_INT:       Con_Printf("%s %c %d",       Str_Text(path), equals, CV_INT(var)); break;
    case CVT_FLOAT:     Con_Printf("%s %c %g",       Str_Text(path), equals, CV_FLOAT(var)); break;
    case CVT_CHARPTR:   Con_Printf("%s %c \"%s\"",   Str_Text(path), equals, CV_CHARPTR(var)); break;
    case CVT_URIPTR: {
        ddstring_t* valPath = (CV_URIPTR(var)? Uri_ToString(CV_URIPTR(var)) : NULL);
        Con_Printf("%s %c \"%s\"",   Str_Text(path), equals, (CV_URIPTR(var)? Str_Text(valPath) : "")); break;
        if(valPath) Str_Delete(valPath);
      }
    default:            Con_Printf("%s (bad type!)", Str_Text(path)); break;
    }
    Str_Delete(path);
    Con_Printf("\n");
    }
}

void Con_AddCommand(const ccmdtemplate_t* ccmd)
{
    assert(inited);
    {
    int minArgs, maxArgs;
    cvartype_t args[MAX_ARGS];
    ccmd_t* newCCmd, *overloaded = 0;

    if(!ccmd)
        return;

    if(!ccmd->name)
        Con_Error("Con_AddCommand: CCmd missing a name.");

/*#if _DEBUG
Con_Message("Con_AddCommand: '%s' \"%s\" (%i).\n", ccmd->name,
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
Con_Message("Con_AddCommand: CCmd '%s': minArgs %i, maxArgs %i: \"",
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
  Con_Message("Con_AddCommand: CCmd '%s' will not have it's usage validated.\n", ccmd->name);
#endif*/
    }

    // Now check that the ccmd to be registered is unique.
    // We allow multiple ccmds with the same name if we can determine by
    // their paramater lists that they are unique (overloading).
    { ccmd_t* other;
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
            ccmd_t* variant = other;
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
        ccmdBlockSet = BlockSet_New(sizeof(ccmd_t), 32);
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

ccmd_t* Con_FindCommand(const char* name)
{
    assert(inited);
    // \todo Use a faster than O(n) linear search.
    if(name && name[0])
    {
        ccmd_t* ccmd;
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

ccmd_t* Con_FindCommandMatchArgs(cmdargs_t* args)
{
    assert(inited);

    if(!args)
        return 0;

    { ccmd_t* ccmd;
    if((ccmd = Con_FindCommand(args->argv[0])) != 0)
    {
        ccmd_t* variant = ccmd;
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

void Con_PrintCCmdUsage(ccmd_t* ccmd, boolean printInfo)
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

    if(printInfo)
    {
        // Check for extra info about this ccmd's usage.
        char* info = DH_GetString(DH_Find(ccmd->name), HST_INFO);
        if(NULL != info)
        {
            // Lets indent for neatness.
            const size_t infoLen = strlen(info);
            char* line, *infoCopy, *infoCopyBuf = (char*) malloc(infoLen+1);
            if(NULL == infoCopyBuf)
                Con_Error("Con_PrintCCmdUsage: Failed on allocation of %lu bytes for "
                    "info copy buffer.", (unsigned long) (infoLen+1));
            memcpy(infoCopyBuf, info, infoLen+1);

            infoCopy = infoCopyBuf;
            while(*(line = M_StrTok(&infoCopy, "\n")))
            {
                Con_FPrintf(CPF_LIGHT, "  %s\n", line);
            }
            free(infoCopyBuf);
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

    // Try to avoid rebuilding known words by simply removing ourself.
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

/**
 * @return New ddstring with the text of the known word. Caller gets ownership.
 */
static ddstring_t* textForKnownWord(const knownword_t* word)
{
    ddstring_t* text = 0;

    switch(word->type)
    {
    case WT_CALIAS:   Str_Set(text = Str_New(), ((calias_t*)word->data)->name); break;
    case WT_CCMD:     Str_Set(text = Str_New(), ((ccmd_t*)word->data)->name); break;
    case WT_CVAR:     text = CVar_ComposePath((cvar_t*)word->data); break;
    case WT_GAME:     Str_Set(text = Str_New(), Str_Text(Game_IdentityKey((Game*)word->data))); break;
    default:
        Con_Error("textForKnownWord: Invalid type %i for word.", word->type);
        exit(1); // Unreachable
    }

    return text;
}

int Con_IterateKnownWords(const char* pattern, knownwordtype_t type,
    int (*callback) (const knownword_t* word, void* parameters), void* parameters)
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
        if(patternLength)
        {
            int compareResult;
            ddstring_t* textString = textForKnownWord(word);
            compareResult = strnicmp(Str_Text(textString), pattern, patternLength);
            Str_Delete(textString);

            if(compareResult)
                continue; // Didn't match.
        }
        if(0 != (result = callback(word, parameters)))
            break;
    }}

    return result;
    }
}

static int countMatchedWordWorker(const knownword_t* word, void* parameters)
{
    assert(word && parameters);
    {
    uint* count = (uint*) parameters;
    ++(*count);
    return 0; // Continue iteration.
    }
}

typedef struct {
    const knownword_t** matches; /// Matched word array.
    uint index; /// Current position in the collection.
} collectmatchedwordworker_paramaters_t;

static int collectMatchedWordWorker(const knownword_t* word, void* parameters)
{
    assert(word && parameters);
    {
    collectmatchedwordworker_paramaters_t* p = (collectmatchedwordworker_paramaters_t*) parameters;
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
    cvarDirectory = PathDirectory_New();

    ccmdListHead = 0;
    ccmdBlockSet = 0;
    numUniqueNamedCCmds = 0;

    numCAliases = 0;
    caliases = 0;

    knownWords = 0;
    numKnownWords = 0;
    knownWordsNeedUpdate = false;

    emptyUri = Uri_New();

    inited = true;
}

void Con_ClearDatabases(void)
{
    if(!inited) return;
    clearKnownWords();
    clearAliases();
    clearCommands();
    clearVariables();
}

void Con_ShutdownDatabases(void)
{
    if(!inited) return;

    Con_ClearDatabases();
    PathDirectory_Delete(cvarDirectory), cvarDirectory = NULL;
    Uri_Delete(emptyUri), emptyUri = NULL;
    inited = false;
}

static int aproposPrinter(const knownword_t* word, void* matching)
{
    ddstring_t* text = textForKnownWord(word);

    // See if 'matching' is anywhere in the known word.
    if(strcasestr(Str_Text(text), matching))
    {
        const int maxLen = CBuffer_MaxLineLength(Con_HistoryBuffer());
        int avail;
        ddstring_t buf;
        const char* wType[KNOWNWORDTYPE_COUNT] = {
            "[cmd]", "[var]", "[alias]", "[game]"
        };
        Str_Init(&buf);
        Str_Appendf(&buf, "%7s ", wType[word->type]);
        Str_Appendf(&buf, "%-25s", Str_Text(text));

        avail = maxLen - Str_Length(&buf) - 4;
        if(avail > 0)
        {
            ddstring_t tmp;
            Str_Init(&tmp);
            // Look for a short description.
            if(word->type == WT_CCMD || word->type == WT_CVAR)
            {
                const char* desc = DH_GetString(DH_Find(Str_Text(text)), HST_DESCRIPTION);
                if(desc)
                {
                    Str_Set(&tmp, desc);
                }
            }
            else if(word->type == WT_GAME)
            {
                Str_Set(&tmp, Str_Text(Game_Title((Game*)word->data)));
            }
            // Truncate.
            if(Str_Length(&tmp) > avail - 3)
            {
                Str_Truncate(&tmp, avail);
                Str_Append(&tmp, "...");
            }
            Str_Appendf(&buf, " %s", Str_Text(&tmp));
            Str_Free(&tmp);
        }

        Con_Printf("%s\n", Str_Text(&buf));
        Str_Free(&buf);
    }

    Str_Delete(text);
    return 0;
}

static void printApropos(const char* matching)
{
    /// @todo  Extend the search to cover the contents of all help strings (dd_help.c).

    Con_IterateKnownWords(0, WT_ANY, aproposPrinter, (void*)matching);
}

static void printHelpAbout(const char* query)
{
    uint found = 0;

    // Try the console commands first.
    {ccmd_t* ccmd = Con_FindCommand(query);
    if(NULL != ccmd)
    {
        void* helpRecord = DH_Find(ccmd->name);
        char* description = DH_GetString(helpRecord, HST_DESCRIPTION);
        char* info = DH_GetString(helpRecord, HST_INFO);

        if(NULL != description)
            Con_Printf("%s\n", description);

        // Print usage info for each variant.
        do {
            Con_PrintCCmdUsage(ccmd, false);
            ++found;
        } while(NULL != (ccmd = ccmd->nextOverload));

        // Any extra info?
        if(NULL != info)
        {
            // Lets indent for neatness.
            const size_t infoLen = strlen(info);
            char* line, *infoCopy, *infoCopyBuf = (char*) malloc(infoLen+1);
            if(NULL == infoCopyBuf)
                Con_Error("CCmdHelpWhat: Failed on allocation of %lu bytes for "
                    "info copy buffer.", (unsigned long) (infoLen+1));
            memcpy(infoCopyBuf, info, infoLen+1);

            infoCopy = infoCopyBuf;
            while(*(line = M_StrTok(&infoCopy, "\n")))
            {
                Con_FPrintf(CPF_LIGHT, "  %s\n", line);
            }
            free(infoCopyBuf);
        }
    }}

    if(found == 0) // Perhaps its a cvar then?
    {
        cvar_t* var = Con_FindVariable(query);
        if(var)
        {
            ddstring_t* path = CVar_ComposePath(var);
            char* str = DH_GetString(DH_Find(Str_Text(path)), HST_DESCRIPTION);
            if(str)
            {
                Con_Printf("%s\n", str);
                found = true;
            }
            Str_Delete(path);
        }
    }

    if(found == 0) // Maybe an alias?
    {
        calias_t* calias = Con_FindAlias(query);
        if(NULL != calias)
        {
            Con_Printf("An alias for:\n%s\n", calias->command);
            found = true;
        }
    }

    if(found == 0) // Perhaps a game?
    {
        Game* game = DD_GameByIdentityKey(query);
        if(game)
        {
            DD_PrintGame(game, PGF_EVERYTHING);
            found = true;
        }
    }

    if(found == 0) // Still not found?
        Con_Printf("There is no help about '%s'.\n", query);
}

D_CMD(HelpApropos)
{
    printApropos(argv[1]);
    return true;
}

D_CMD(HelpWhat)
{
    if(!stricmp(argv[1], "(what)"))
    {
        Con_Printf("You've got to be kidding!\n");
        return true;
    }
    printHelpAbout(argv[1]);
    return true;
}

static int printKnownWordWorker(const knownword_t* word, void* parameters)
{
    assert(word);
    {
    uint* numPrinted = (void*)parameters;
    switch(word->type)
    {
    case WT_CCMD: {
        ccmd_t* ccmd = (ccmd_t*) word->data;
        char* str;

        if(ccmd->prevOverload)
            return 0; // Skip overloaded variants.

        if((str = DH_GetString(DH_Find(ccmd->name), HST_DESCRIPTION)))
            Con_FPrintf(CPF_LIGHT|CPF_YELLOW, "  %s (%s)\n", ccmd->name, str);
        else
            Con_FPrintf(CPF_LIGHT|CPF_YELLOW, "  %s\n", ccmd->name);
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
        Con_FPrintf(CPF_LIGHT|CPF_YELLOW, "  %s == %s\n", cal->name, cal->command);
        break;
      }
    case WT_GAME: {
        Game* game = (Game*) word->data;
        Con_FPrintf(CPF_LIGHT|CPF_BLUE, "  %s\n", Str_Text(Game_IdentityKey(game)));
        break;
      }
    default:
        Con_Error("printKnownWordWorker: Invalid word type %i.", (int)word->type);
        exit(1); // Unreachable.
    }
    if(numPrinted) ++(*numPrinted);
    return 0; // Continue iteration.
    }
}

/// \note Part of the Doomsday public API.
void Con_SetUri2(const char* path, const Uri* uri, int svFlags)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetUri2(var, uri, svFlags);
}

/// \note Part of the Doomsday public API.
void Con_SetUri(const char* path, const Uri* uri)
{
    Con_SetUri2(path, uri, 0);
}

/// \note Part of the Doomsday public API.
void Con_SetString2(const char* path, const char* text, int svFlags)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetString2(var, text, svFlags);
}

/// \note Part of the Doomsday public API.
void Con_SetString(const char* path, const char* text)
{
    Con_SetString2(path, text, 0);
}

/// \note Part of the Doomsday public API.
void Con_SetInteger2(const char* path, int value, int svFlags)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetInteger2(var, value, svFlags);
}

/// \note Part of the Doomsday public API.
void Con_SetInteger(const char* path, int value)
{
    Con_SetInteger2(path, value, 0);
}

/// \note Part of the Doomsday public API.
void Con_SetFloat2(const char* path, float value, int svFlags)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetFloat2(var, value, svFlags);
}

/// \note Part of the Doomsday public API.
void Con_SetFloat(const char* path, float value)
{
    Con_SetFloat2(path, value, 0);
}

/// \note Part of the Doomsday public API.
int Con_GetInteger(const char* path)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return 0;
    return CVar_Integer(var);
}

/// \note Part of the Doomsday public API.
float Con_GetFloat(const char* path)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return 0;
    return CVar_Float(var);
}

/// \note Part of the Doomsday public API.
byte Con_GetByte(const char* path)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return 0;
    return CVar_Byte(var);
}

/// \note Part of the Doomsday public API.
char* Con_GetString(const char* path)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return emptyString;
    return CVar_String(var);
}

/// \note Part of the Doomsday public API.
Uri* Con_GetUri(const char* path)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return emptyUri;
    return CVar_Uri(var);
}

D_CMD(ListCmds)
{
    uint numPrinted = 0;
    Con_Printf("Console commands:\n");
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CCMD, printKnownWordWorker, &numPrinted);
    Con_Printf("Found %u console commands.\n", numPrinted);
    return true;
}

D_CMD(ListVars)
{
    uint numPrinted = 0;
    Con_Printf("Console variables:\n");
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CVAR, printKnownWordWorker, &numPrinted);
    Con_Printf("Found %u console variables.\n", numPrinted);
    return true;
}

#if _DEBUG
D_CMD(PrintVarStats)
{
    cvartype_t type;
    countvariableparams_t p;
    Con_FPrintf(CPF_YELLOW, "Console Variable Statistics:\n");
    p.hidden = false;
    p.ignoreHidden = false;
    for(type = CVT_BYTE; type < CVARTYPE_COUNT; ++type)
    {
        p.count = 0;
        p.type = type;
        PathDirectory_Iterate2_Const(cvarDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, countVariable, &p);
        Con_Printf("%12s: %u\n", Str_Text(CVar_TypeName(type)), p.count);
    }
    p.count = 0;
    p.type = -1;
    p.hidden = true;
    PathDirectory_Iterate2_Const(cvarDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, countVariable, &p);
    Con_Printf("       Total: %u\n      Hidden: %u\n\n", PathDirectory_Size(cvarDirectory), p.count);
    PathDirectory_DebugPrintHashDistribution(cvarDirectory);
    PathDirectory_DebugPrint(cvarDirectory, CVARDIRECTORY_DELIMITER);
    return true;
}
#endif

D_CMD(ListAliases)
{
    uint numPrinted = 0;
    Con_Printf("Aliases:\n");
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CALIAS, printKnownWordWorker, &numPrinted);
    Con_Printf("Found %u aliases.\n", numPrinted);
    return true;
}
