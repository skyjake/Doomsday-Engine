/**
 * @file con_data.cpp
 *
 * Console databases for cvars, ccmds, aliases and known words.
 *
 * @ingroup console
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "de_platform.h"
#include "de_console.h"

#include "cbuffer.h"
#include "m_misc.h"
#include "pathtree.h"

#include <de/memoryblockset.h>

// Substrings in CVar names are delimited by this character.
#define CVARDIRECTORY_DELIMITER         '-'

typedef de::PathTree CVarDirectory;

D_CMD(HelpWhat);
D_CMD(HelpApropos);
D_CMD(ListAliases);
D_CMD(ListCmds);
D_CMD(ListVars);
#if _DEBUG
D_CMD(PrintVarStats);
#endif

static bool inited = false;

/// Console variable directory.
static CVarDirectory* cvarDirectory;

static ccmd_t* ccmdListHead;
/// @todo Replace with a data structure that allows for deletion of elements.
static blockset_t* ccmdBlockSet;
/// Running total of the number of uniquely-named commands.
static uint numUniqueNamedCCmds;

/// @todo Replace with a BST.
static uint numCAliases;
static calias_t** caliases;

/// The list of known words (for completion).
/// @todo Replace with a persistent self-balancing BST (Treap?)?
static knownword_t* knownWords;
static uint numKnownWords;
static boolean knownWordsNeedUpdate;

static Str* emptyStr;
static Uri* emptyUri;

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

static int markVariableUserDataFreed(CVarDirectory::Node& node, void* parameters)
{
    DENG_ASSERT(parameters);

    cvar_t* var = reinterpret_cast<cvar_t*>(node.userPointer());
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

static int clearVariable(CVarDirectory::Node& node, void* parameters)
{
    DENG_UNUSED(parameters);

    cvar_t* var = reinterpret_cast<cvar_t*>(node.userPointer());
    if(var)
    {
        // Detach our user data from this node.
        node.setUserPointer(0);

        if(CVar_Flags(var) & CVF_CAN_FREE)
        {
            void** ptr = NULL;
            switch(CVar_Type(var))
            {
            case CVT_CHARPTR:
                if(!CV_CHARPTR(var)) break;

                ptr = (void**)var->ptr;
                /// @note Multiple vars could be using the same pointer (so only free once).
                cvarDirectory->traverse(PCF_NO_BRANCH, NULL, CVarDirectory::no_hash, markVariableUserDataFreed, ptr);
                M_Free(*ptr); *ptr = Str_Text(emptyStr);
                break;

            case CVT_URIPTR:
                if(!CV_URIPTR(var)) break;

                ptr = (void**)var->ptr;
                /// @note Multiple vars could be using the same pointer (so only free once).
                cvarDirectory->traverse(PCF_NO_BRANCH, NULL, CVarDirectory::no_hash, markVariableUserDataFreed, ptr);
                Uri_Delete((Uri*)*ptr); *ptr = emptyUri;
                break;

            default: {
#if _DEBUG
                AutoStr* path = CVar_ComposePath(var);
                Con_Message("Warning: clearVariable: Attempt to free user data for non-pointer type variable %s [%p], ignoring.\n",
                            Str_Text(path), (void*)var);
#endif
                break; }
            }
        }
        M_Free(var);
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
    if(!cvarDirectory) return;

    cvarDirectory->traverse(flags, NULL, CVarDirectory::no_hash, clearVariable);
    cvarDirectory->clear();
}

/// Construct a new variable from the specified template and add it to the database.
static cvar_t* addVariable(cvartemplate_t const& tpl)
{
    de::Uri path = de::Uri(tpl.path, FC_NONE, CVARDIRECTORY_DELIMITER);
    CVarDirectory::Node* node = cvarDirectory->insert(path);
    cvar_t* newVar;

    if(node->userPointer())
    {
        throw de::Error("Con_AddVariable", "A variable with path '" + de::String(tpl.path) + "' is already known!");
    }

    newVar = (cvar_t*) M_Malloc(sizeof *newVar);
    if(!newVar) throw de::Error("Con_AddVariable", QString("Failed on allocation of %u bytes for new CVar.").arg(sizeof *newVar));

    newVar->flags = tpl.flags;
    newVar->type = tpl.type;
    newVar->ptr = tpl.ptr;
    newVar->min = tpl.min;
    newVar->max = tpl.max;
    newVar->notifyChanged = tpl.notifyChanged;
    newVar->directoryNode = node;
    node->setUserPointer(newVar);

    knownWordsNeedUpdate = true;
    return newVar;
}

static void clearAliases(void)
{
    if(caliases)
    {
        // Free the alias data.
        calias_t** cal = caliases;
        for(uint i = 0; i < numCAliases; ++i, ++cal)
        {
            M_Free((*cal)->name);
            M_Free((*cal)->command);
            M_Free(*cal);
        }
        M_Free(caliases);
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
        M_Free(knownWords);
    knownWords = 0;
    numKnownWords = 0;
    knownWordsNeedUpdate = false;
}

static int compareKnownWordByName(void const* a, void const* b)
{
    knownword_t const* wA = (knownword_t const*)a;
    knownword_t const* wB = (knownword_t const*)b;
    AutoStr* textAString = NULL, *textBString = NULL;
    char const* textA, *textB;

    switch(wA->type)
    {
    case WT_CALIAS:   textA = ((calias_t*)wA->data)->name; break;
    case WT_CCMD:     textA = ((ccmd_t*)wA->data)->name; break;
    case WT_CVAR:
        textAString = CVar_ComposePath((cvar_t*)wA->data);
        textA = Str_Text(textAString);
        break;
    case WT_GAME: textA = Str_Text(&reinterpret_cast<de::Game*>(wA->data)->identityKey()); break;
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
    case WT_GAME: textB = Str_Text(&reinterpret_cast<de::Game*>(wB->data)->identityKey()); break;
    default:
        Con_Error("compareKnownWordByName: Invalid type %i for word B.", wB->type);
        exit(1); // Unreachable
    }

    return stricmp(textA, textB);
}

static boolean removeFromKnownWords(knownwordtype_t type, void* data)
{
    DENG_ASSERT(VALID_KNOWNWORDTYPE(type) && data != 0);

    for(uint i = 0; i < numKnownWords; ++i)
    {
        knownword_t* word = &knownWords[i];

        if(word->type != type || word->data != data)
            continue;

        if(i != numKnownWords-1)
            memmove(knownWords + i, knownWords + i + 1, sizeof(knownword_t) * (numKnownWords - 1 - i));

        --numKnownWords;
        if(numKnownWords != 0)
        {
            knownWords = (knownword_t*) M_Realloc(knownWords, sizeof(knownword_t) * numKnownWords);
        }
        else
        {
            M_Free(knownWords); knownWords = 0;
        }
        return true;
    }
    return false;
}

typedef struct {
    uint count;
    cvartype_t type;
    boolean hidden;
    boolean ignoreHidden;
} countvariableparams_t;

static int countVariable(CVarDirectory::Node& node, void* parameters)
{
    DENG_ASSERT(parameters);

    countvariableparams_t* p = (countvariableparams_t*) parameters;
    cvar_t* var = reinterpret_cast<cvar_t*>( node.userPointer() );

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

static int addVariableToKnownWords(CVarDirectory::Node& node, void* parameters)
{
    DENG_ASSERT(parameters);

    cvar_t* var = reinterpret_cast<cvar_t*>( node.userPointer() );
    uint* index = (uint*) parameters;
    if(var && !(var->flags & CVF_HIDE))
    {
        knownWords[*index].type = WT_CVAR;
        knownWords[*index].data = var;
        ++(*index);
    }
    return 0; // Continue iteration.
}

/**
 * Collate all known words and sort them alphabetically.
 * Commands, variables (except those hidden) and aliases are known words.
 */
static void updateKnownWords(void)
{
    if(!knownWordsNeedUpdate) return;

    // Count the number of visible console variables.
    countvariableparams_t countCVarParams;
    countCVarParams.count = 0;
    countCVarParams.type = cvartype_t(-1);
    countCVarParams.hidden = false;
    countCVarParams.ignoreHidden = true;
    if(cvarDirectory)
    {
        cvarDirectory->traverse(PCF_NO_BRANCH, NULL, CVarDirectory::no_hash, countVariable, &countCVarParams);
    }

    // Build the known words table.
    numKnownWords = numUniqueNamedCCmds + countCVarParams.count + numCAliases + GameCollection_Count(App_GameCollection());
    size_t len = sizeof(knownword_t) * numKnownWords;
    knownWords = (knownword_t*) M_Realloc(knownWords, len);
    memset(knownWords, 0, len);

    uint knownWordIdx = 0;

    // Add commands?
    /// @note ccmd list is NOT yet sorted.
    for(ccmd_t* ccmd = ccmdListHead; ccmd; ccmd = ccmd->next)
    {
        // Skip overloaded variants.
        if(ccmd->prevOverload) continue;

        knownWords[knownWordIdx].type = WT_CCMD;
        knownWords[knownWordIdx].data = ccmd;
        ++knownWordIdx;
    }

    // Add variables?
    if(0 != countCVarParams.count)
    {
        /// @note cvars are NOT sorted.
        cvarDirectory->traverse(PCF_NO_BRANCH, NULL, CVarDirectory::no_hash, addVariableToKnownWords, &knownWordIdx);
    }

    // Add aliases?
    if(0 != numCAliases)
    {
        /// @note caliases array is already sorted.
        calias_t** cal = caliases;
        for(uint i = 0; i < numCAliases; ++i, ++cal)
        {
            knownWords[knownWordIdx].type = WT_CALIAS;
            knownWords[knownWordIdx].data = *cal;
            ++knownWordIdx;
        }
    }

    // Add games?
    de::GameCollection& gameCollection = reinterpret_cast<de::GameCollection&>( *App_GameCollection() );
    DENG2_FOR_EACH_CONST(de::GameCollection::Games, i, gameCollection.games())
    {
        knownWords[knownWordIdx].type = WT_GAME;
        knownWords[knownWordIdx].data = *i;
        ++knownWordIdx;
    }

    // Sort it so we get nice alphabetical word completions.
    qsort(knownWords, numKnownWords, sizeof(knownword_t), compareKnownWordByName);
    knownWordsNeedUpdate = false;
}

ddstring_t const* CVar_TypeName(cvartype_t type)
{
    static de::Str const names[CVARTYPE_COUNT] = {
        "invalid",
        "CVT_BYTE",
        "CVT_INT",
        "CVT_FLOAT",
        "CVT_CHARPTR",
        "CVT_URIPTR"
    };
    return names[(VALID_CVARTYPE(type)? type : 0)];
}

cvartype_t CVar_Type(cvar_t const* var)
{
    DENG_ASSERT(var);
    return var->type;
}

int CVar_Flags(const cvar_t* var)
{
    DENG_ASSERT(var);
    return var->flags;
}

AutoStr* CVar_ComposePath(cvar_t const* var)
{
    DENG_ASSERT(var);
    CVarDirectory::Node& node = *reinterpret_cast<CVarDirectory::Node*>(var->directoryNode);
    QByteArray path = node.composePath(CVARDIRECTORY_DELIMITER).toUtf8();
    return AutoStr_FromTextStd(path.constData());
}

void CVar_SetUri2(cvar_t* var, Uri const* uri, int svFlags)
{
    DENG_ASSERT(var);

    Uri* newUri;
    bool changed = false;

    if((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        AutoStr* path = CVar_ComposePath(var);
        Con_Printf("%s (var) is read-only. It can't be changed (not even with force)\n", Str_Text(path));
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
    newUri = Uri_Dup(uri);

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

void CVar_SetUri(cvar_t* var, Uri const* uri)
{
    CVar_SetUri2(var, uri, 0);
}

void CVar_SetString2(cvar_t* var, char const* text, int svFlags)
{
    DENG_ASSERT(var);

    bool changed = false;
    size_t oldLen, newLen;

    if((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        AutoStr* path = CVar_ComposePath(var);
        Con_Printf("%s (var) is read-only. It can't be changed (not even with force)\n", Str_Text(path));
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

    if(oldLen != newLen || qstricmp(text, CV_CHARPTR(var)))
        changed = true;

    // Free the old string, if one exists.
    if((var->flags & CVF_CAN_FREE) && CV_CHARPTR(var))
        free(CV_CHARPTR(var));

    // Allocate a new string.
    var->flags |= CVF_CAN_FREE;
    CV_CHARPTR(var) = (char*) M_Malloc(newLen + 1);
    if(!CV_CHARPTR(var))
        Con_Error("CVar::SetString: Failed on allocation of %lu bytes for new string value.", (unsigned long) (newLen + 1));
    qstrcpy(CV_CHARPTR(var), text);

    // Make the change notification callback
    if(var->notifyChanged != NULL && changed)
        var->notifyChanged();
}

void CVar_SetString(cvar_t* var, char const* text)
{
    CVar_SetString2(var, text, 0);
}

void CVar_SetInteger2(cvar_t* var, int value, int svFlags)
{
    DENG_ASSERT(var);

    bool changed = false;

    if((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        AutoStr* path = CVar_ComposePath(var);
        Con_Printf("%s (var) is read-only. It can't be changed (not even with force).\n", Str_Text(path));
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
        AutoStr* path = CVar_ComposePath(var);
        Con_Message("Warning: CVar::SetInteger: Attempt to set incompatible var %s to %i, ignoring.\n", Str_Text(path), value);
        return; }
    }

    // Make a change notification callback?
    if(var->notifyChanged != 0 && changed)
        var->notifyChanged();
}

void CVar_SetInteger(cvar_t* var, int value)
{
    CVar_SetInteger2(var, value, 0);
}

void CVar_SetFloat2(cvar_t* var, float value, int svFlags)
{
    DENG_ASSERT(var);

    bool changed = false;

    if((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        AutoStr* path = CVar_ComposePath(var);
        Con_Printf("%s (cvar) is read-only. It can't be changed (not even with force).\n", Str_Text(path));
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
        AutoStr* path = CVar_ComposePath(var);
        Con_Message("Warning: CVar::SetFloat: Attempt to set incompatible cvar %s to %g, ignoring.\n", Str_Text(path), value);
        return; }
    }

    // Make a change notification callback?
    if(var->notifyChanged != 0 && changed)
        var->notifyChanged();
}

void CVar_SetFloat(cvar_t* var, float value)
{
    CVar_SetFloat2(var, value, 0);
}

int CVar_Integer(cvar_t const* var)
{
    DENG_ASSERT(var);
    switch(var->type)
    {
    case CVT_BYTE:      return CV_BYTE(var);
    case CVT_INT:       return CV_INT(var);
    case CVT_FLOAT:     return CV_FLOAT(var);
    case CVT_CHARPTR:   return strtol(CV_CHARPTR(var), 0, 0);
    default: {
#if _DEBUG
        AutoStr* path = CVar_ComposePath(var);
        Con_Message("Warning: CVar::Integer: Attempted on incompatible variable %s [%p type:%s], returning 0\n",
                    Str_Text(path), (void*)var, Str_Text(CVar_TypeName(CVar_Type(var))));
#endif
        return 0; }
    }
}

float CVar_Float(cvar_t const* var)
{
    DENG_ASSERT(var);
    switch(var->type)
    {
    case CVT_BYTE:      return CV_BYTE(var);
    case CVT_INT:       return CV_INT(var);
    case CVT_FLOAT:     return CV_FLOAT(var);
    case CVT_CHARPTR:   return strtod(CV_CHARPTR(var), 0);
    default: {
#if _DEBUG
        AutoStr* path = CVar_ComposePath(var);
        Con_Message("Warning: CVar::Float: Attempted on incompatible variable %s [%p type:%s], returning 0\n",
                    Str_Text(path), (void*)var, Str_Text(CVar_TypeName(CVar_Type(var))));
#endif
        return 0;
      }
    }
}

byte CVar_Byte(cvar_t const* var)
{
    DENG_ASSERT(var);
    switch(var->type)
    {
    case CVT_BYTE:      return CV_BYTE(var);
    case CVT_INT:       return CV_INT(var);
    case CVT_FLOAT:     return CV_FLOAT(var);
    case CVT_CHARPTR:   return strtol(CV_CHARPTR(var), 0, 0);
    default: {
#if _DEBUG
        AutoStr* path = CVar_ComposePath(var);
        Con_Message("Warning: CVar::Byte: Attempted on incompatible variable %s [%p type:%s], returning 0\n",
                    Str_Text(path), (void*)var, Str_Text(CVar_TypeName(CVar_Type(var))));
#endif
        return 0;
      }
    }
}

char const* CVar_String(cvar_t const* var)
{
    DENG_ASSERT(var);
    /// @todo Why not implement in-place value to string conversion?
    switch(var->type)
    {
    case CVT_CHARPTR:   return CV_CHARPTR(var);
    default: {
#if _DEBUG
        AutoStr* path = CVar_ComposePath(var);
        Con_Message("Warning: CVar::String: Attempted on incompatible variable %s [%p type:%s], returning emptyString\n",
                    Str_Text(path), (void*)var, Str_Text(CVar_TypeName(CVar_Type(var))));
#endif
        return Str_Text(emptyStr); }
    }
}

Uri const* CVar_Uri(cvar_t const* var)
{
    DENG_ASSERT(var);
    /// @todo Why not implement in-place string to uri conversion?
    switch(var->type)
    {
    case CVT_URIPTR:   return CV_URIPTR(var);
    default: {
#if _DEBUG
        AutoStr* path = CVar_ComposePath(var);
        Con_Message("Warning: CVar::String: Attempted on incompatible variable %s [%p type:%s], returning emptyUri\n",
                    Str_Text(path), (void*)var, Str_Text(CVar_TypeName(CVar_Type(var))));
#endif
        return emptyUri; }
    }
}

void Con_AddVariable(cvartemplate_t const* tpl)
{
    LOG_AS("Con_AddVariable");

    DENG_ASSERT(inited);
    if(!tpl)
    {
        LOG_WARNING("Passed invalid value for argument 'tpl', ignoring.");
        return;
    }
    if(CVT_NULL == tpl->type)
    {
        LOG_WARNING("Attempt to register variable '%s' as type %s, ignoring.")
            << tpl->path << Str_Text(CVar_TypeName(CVT_NULL));
        return;
    }

    addVariable(*tpl);
}

void Con_AddVariableList(cvartemplate_t const* tplList)
{
    DENG_ASSERT(inited);
    if(!tplList)
    {
        Con_Message("Warning: Con_AddVariableList: Passed invalid value for "
                    "argument 'tplList', ignoring.\n");
        return;
    }
    for(; tplList->path; ++tplList)
    {
        if(Con_FindVariable(tplList->path))
            Con_Error("A CVAR with the name '%s' is already registered.", tplList->path);

        addVariable(*tplList);
    }
}

cvar_t* Con_FindVariable(char const* path)
{
    DENG_ASSERT(inited);
    if(!path || !path[0]) return 0;

    try
    {
        CVarDirectory::Node& node = cvarDirectory->find(de::Uri(path, FC_NONE, CVARDIRECTORY_DELIMITER),
                                                        PCF_NO_BRANCH | PCF_MATCH_FULL);
        return (cvar_t*) node.userPointer();
    }
    catch(CVarDirectory::NotFoundError const&)
    {} // Ignore this error.
    return 0;
}

/// @note Part of the Doomsday public API
cvartype_t Con_GetVariableType(char const* path)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return CVT_NULL;
    return var->type;
}

void Con_PrintCVar(cvar_t* var, const char* prefix)
{
    DENG_ASSERT(inited);

    char equals = '=';
    AutoStr* path;

    if(!var) return;

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
        AutoStr* valPath = (CV_URIPTR(var)? Uri_ToString(CV_URIPTR(var)) : NULL);
        Con_Printf("%s %c \"%s\"",   Str_Text(path), equals, (CV_URIPTR(var)? Str_Text(valPath) : ""));
        break; }

    default:            Con_Printf("%s (bad type!)", Str_Text(path)); break;
    }
    Con_Printf("\n");
}

void Con_AddCommand(ccmdtemplate_t const* ccmd)
{
    DENG_ASSERT(inited);

    int minArgs, maxArgs;
    cvartype_t args[MAX_ARGS];
    ccmd_t* newCCmd, *overloaded = 0;

    if(!ccmd) return;

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
        {
            maxArgs = minArgs;
        }

/*#if _DEBUG
        Con_Message("Con_AddCommand: CCmd '%s': minArgs %i, maxArgs %i: \"",
                    ccmd->name, minArgs, maxArgs);
        for(int i = 0; i < minArgs; ++i)
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
                    Con_Error("Con_AddCommand: A CCmd by the name '%s' is already registered and the callback funcs are "
                              "the same, is this really what you wanted?", ccmd->name);
            } while((variant = variant->nextOverload) != 0);
        }

        if(!unique)
            Con_Error("Con_AddCommand: A CCmd by the name '%s' is already registered. Their parameter lists would be ambiguant.", ccmd->name);

        overloaded = other;
    }}

    if(!ccmdBlockSet)
        ccmdBlockSet = BlockSet_New(sizeof(ccmd_t), 32);

    newCCmd = (ccmd_t*) BlockSet_Allocate(ccmdBlockSet);

    // Make a static copy of the name in the zone (this allows the source
    // data to change in case of dynamic registrations).
    char* nameCopy = (char*) Z_Malloc(strlen(ccmd->name) + 1, PU_APPSTATIC, NULL);
    if(!nameCopy) Con_Error("Con_AddCommand: Failed on allocation of %lu bytes for command name.", (unsigned long) (strlen(ccmd->name) + 1));

    strcpy(nameCopy, ccmd->name);
    newCCmd->name = nameCopy;
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

void Con_AddCommandList(ccmdtemplate_t const* cmdList)
{
    if(!cmdList) return;
    for(; cmdList->name; ++cmdList)
    {
        Con_AddCommand(cmdList);
    }
}

ccmd_t* Con_FindCommand(char const* name)
{
    DENG_ASSERT(inited);
    /// @todo Use a faster than O(n) linear search.
    if(name && name[0])
    {
        ccmd_t* ccmd;
        for(ccmd = ccmdListHead; ccmd; ccmd = ccmd->next)
        {
            if(qstricmp(name, ccmd->name)) continue;

            // Locate the head of the overload list.
            while(ccmd->prevOverload) { ccmd = ccmd->prevOverload; }
            return ccmd;
        }
    }
    return 0;
}

ccmd_t* Con_FindCommandMatchArgs(cmdargs_t* args)
{
    DENG_ASSERT(inited);

    if(!args) return 0;

    ccmd_t* ccmd;
    if((ccmd = Con_FindCommand(args->argv[0])) != 0)
    {
        // Check each variant.
        ccmd_t* variant = ccmd;
        do
        {
            boolean invalidArgs = false;

            // Are we validating the arguments?
            // Note that strings are considered always valid.
            if(!(variant->minArgs == -1 && variant->maxArgs == -1))
            {
                // Do we have the right number of arguments?
                if(args->argc-1 < variant->minArgs)
                {
                    invalidArgs = true;
                }
                else if(variant->maxArgs != -1 && args->argc-1 > variant->maxArgs)
                {
                    invalidArgs = true;
                }
                else
                {
                    // Presently we only validate upto the minimum number of args.
                    /// @todo Validate non-required args.
                    for(int j = 0; j < variant->minArgs && !invalidArgs; ++j)
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
    }

    // No command found, or none with matching arguments.
    return 0;
}

boolean Con_IsValidCommand(char const* name)
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
    DENG_ASSERT(inited);

    if(!ccmd || (ccmd->minArgs == -1 && ccmd->maxArgs == -1))
        return;

    // Print the expected form for this ccmd.
    Con_Printf("Usage: %s", ccmd->name);
    for(int i = 0; i < ccmd->minArgs; ++i)
    {
        switch(ccmd->args[i])
        {
        case CVT_BYTE:      Con_Printf(" (byte)");      break;
        case CVT_INT:       Con_Printf(" (int)");       break;
        case CVT_FLOAT:     Con_Printf(" (float)");     break;
        case CVT_CHARPTR:   Con_Printf(" (string)");    break;
        default: break;
        }
    }
    if(ccmd->maxArgs == -1)
        Con_Printf(" ...");
    Con_Printf("\n");

    if(printInfo)
    {
        // Check for extra info about this ccmd's usage.
        char* info = DH_GetString(DH_Find(ccmd->name), HST_INFO);
        if(info)
        {
            // Lets indent for neatness.
            size_t const infoLen = strlen(info);
            char* line, *infoCopy, *infoCopyBuf = (char*) M_Malloc(infoLen+1);
            if(!infoCopyBuf) Con_Error("Con_PrintCCmdUsage: Failed on allocation of %lu bytes for info copy buffer.", (unsigned long) (infoLen+1));

            memcpy(infoCopyBuf, info, infoLen+1);

            infoCopy = infoCopyBuf;
            while(*(line = M_StrTok(&infoCopy, "\n")))
            {
                Con_FPrintf(CPF_LIGHT, "  %s\n", line);
            }
            M_Free(infoCopyBuf);
        }
    }
}

calias_t* Con_FindAlias(char const* name)
{
    DENG_ASSERT(inited);

    uint bottomIdx, topIdx, pivot;
    calias_t* cal;
    boolean isDone;
    int result;

    if(numCAliases == 0) return 0;
    if(!name || !name[0]) return 0;

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

calias_t* Con_AddAlias(char const* name, char const* command)
{
    DENG_ASSERT(inited);

    if(!name || !name[0] || !command || !command[0]) return 0;

    caliases = (calias_t**) M_Realloc(caliases, sizeof(*caliases) * ++numCAliases);

    // Find the insertion point.
    uint idx;
    for(idx = 0; idx < numCAliases-1; ++idx)
    {
        if(qstricmp(caliases[idx]->name, name) > 0)
            break;
    }

    // Make room for the new alias.
    if(idx != numCAliases-1)
        memmove(caliases + idx + 1, caliases + idx, sizeof(*caliases) * (numCAliases - 1 - idx));

    // Add the new variable, making a static copy of the name in the zone (this allows
    // the source data to change in case of dynamic registrations).
    calias_t* newAlias = caliases[idx] = (calias_t*) M_Malloc(sizeof(*newAlias));
    newAlias->name = (char*) M_Malloc(strlen(name) + 1);
    strcpy(newAlias->name, name);
    newAlias->command = (char*) M_Malloc(strlen(command) + 1);
    strcpy(newAlias->command, command);

    knownWordsNeedUpdate = true;
    return newAlias;
}

void Con_DeleteAlias(calias_t* cal)
{
    DENG_ASSERT(inited && cal);

    uint idx;
    for(idx = 0; idx < numCAliases; ++idx)
    {
        if(caliases[idx] == cal)
            break;
    }
    if(idx == numCAliases) return;

    // Try to avoid rebuilding known words by simply removing ourself.
    if(!knownWordsNeedUpdate)
        removeFromKnownWords(WT_CALIAS, (void*)cal);

    M_Free(cal->name);
    M_Free(cal->command);
    M_Free(cal);

    if(idx < numCAliases - 1)
    {
        memmove(caliases + idx, caliases + idx + 1, sizeof(*caliases) * (numCAliases - idx - 1));
    }
    --numCAliases;
}

/**
 * @return New AutoStr with the text of the known word. Caller gets ownership.
 */
static AutoStr* textForKnownWord(knownword_t const* word)
{
    AutoStr* text = 0;

    switch(word->type)
    {
    case WT_CALIAS:   Str_Set(text = AutoStr_NewStd(), ((calias_t*)word->data)->name); break;
    case WT_CCMD:     Str_Set(text = AutoStr_NewStd(), ((ccmd_t*)word->data)->name); break;
    case WT_CVAR:     text = CVar_ComposePath((cvar_t*)word->data); break;
    case WT_GAME:     Str_Set(text = AutoStr_NewStd(), Str_Text(&reinterpret_cast<de::Game*>(word->data)->identityKey())); break;
    default:
        Con_Error("textForKnownWord: Invalid type %i for word.", word->type);
        exit(1); // Unreachable
    }

    return text;
}

int Con_IterateKnownWords(char const* pattern, knownwordtype_t type,
    int (*callback) (knownword_t const* word, void* parameters), void* parameters)
{
    DENG_ASSERT(inited && callback);

    knownwordtype_t matchType = (VALID_KNOWNWORDTYPE(type)? type : WT_ANY);
    size_t patternLength = (pattern? strlen(pattern) : 0);
    int result = 0;

    updateKnownWords();

    for(uint i = 0; i < numKnownWords; ++i)
    {
        knownword_t const* word = &knownWords[i];
        if(matchType != WT_ANY && word->type != type) continue;

        if(patternLength)
        {
            int compareResult;
            AutoStr* textString = textForKnownWord(word);
            compareResult = strnicmp(Str_Text(textString), pattern, patternLength);

            if(compareResult) continue; // Didn't match.
        }

        result = callback(word, parameters);
        if(result) break;
    }

    return result;
}

static int countMatchedWordWorker(knownword_t const* /*word*/, void* parameters)
{
    DENG_ASSERT(parameters);
    uint* count = (uint*) parameters;
    ++(*count);
    return 0; // Continue iteration.
}

typedef struct {
    knownword_t const** matches; /// Matched word array.
    uint index; /// Current position in the collection.
} collectmatchedwordworker_paramaters_t;

static int collectMatchedWordWorker(knownword_t const* word, void* parameters)
{
    DENG_ASSERT(word && parameters);
    collectmatchedwordworker_paramaters_t* p = (collectmatchedwordworker_paramaters_t*) parameters;
    p->matches[p->index++] = word;
    return 0; // Continue iteration.
}

knownword_t const** Con_CollectKnownWordsMatchingWord(char const* word,
    knownwordtype_t type, uint* count)
{
    DENG_ASSERT(inited);

    uint localCount = 0;
    Con_IterateKnownWords(word, type, countMatchedWordWorker, &localCount);

    if(count) *count = localCount;

    if(localCount != 0)
    {
        // Collect the pointers.
        collectmatchedwordworker_paramaters_t p;
        p.matches = (knownword_t const**) M_Malloc(sizeof(*p.matches) * (localCount + 1));
        p.index = 0;

        Con_IterateKnownWords(word, type, collectMatchedWordWorker, &p);
        p.matches[localCount] = 0; // Terminate.

        return p.matches;
    }

    return 0; // No matches.
}

void Con_InitDatabases(void)
{
    if(inited) return;

    // Create the empty variable directory now.
    cvarDirectory = new CVarDirectory();

    ccmdListHead = 0;
    ccmdBlockSet = 0;
    numUniqueNamedCCmds = 0;

    numCAliases = 0;
    caliases = 0;

    knownWords = 0;
    numKnownWords = 0;
    knownWordsNeedUpdate = false;

    emptyStr = Str_NewStd();
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
    delete cvarDirectory; cvarDirectory = 0;
    Str_Delete(emptyStr); emptyStr = 0;
    Uri_Delete(emptyUri); emptyUri = 0;
    inited = false;
}

static int aproposPrinter(knownword_t const* word, void* matching)
{
    AutoStr* text = textForKnownWord(word);

    // See if 'matching' is anywhere in the known word.
    if(strcasestr(Str_Text(text), (const char*)matching))
    {
        int const maxLen = CBuffer_MaxLineLength(Con_HistoryBuffer());
        int avail;
        ddstring_t buf;
        char const* wType[KNOWNWORDTYPE_COUNT] = {
            "[cmd]", "[var]", "[alias]", "[game]"
        };

        Str_Init(&buf);
        Str_Appendf(&buf, "%7s ", wType[word->type]);
        Str_Appendf(&buf, "%-25s", Str_Text(text));

        avail = maxLen - Str_Length(&buf) - 4;
        if(avail > 0)
        {
            ddstring_t tmp; Str_Init(&tmp);

            // Look for a short description.
            if(word->type == WT_CCMD || word->type == WT_CVAR)
            {
                char const* desc = DH_GetString(DH_Find(Str_Text(text)), HST_DESCRIPTION);
                if(desc)
                {
                    Str_Set(&tmp, desc);
                }
            }
            else if(word->type == WT_GAME)
            {
                Str_Set(&tmp, Str_Text(&reinterpret_cast<de::Game*>(word->data)->title()));
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

    return 0;
}

static void printApropos(char const* matching)
{
    /// @todo  Extend the search to cover the contents of all help strings (dd_help.c).
    Con_IterateKnownWords(0, WT_ANY, aproposPrinter, (void*)matching);
}

static void printHelpAbout(char const* query)
{
    uint found = 0;

    // Try the console commands first.
    if(ccmd_t* ccmd = Con_FindCommand(query))
    {
        void* helpRecord = DH_Find(ccmd->name);
        char const* description = DH_GetString(helpRecord, HST_DESCRIPTION);
        char const* info = DH_GetString(helpRecord, HST_INFO);

        if(description)
            Con_Printf("%s\n", description);

        // Print usage info for each variant.
        do
        {
            Con_PrintCCmdUsage(ccmd, false);
            ++found;
        } while(NULL != (ccmd = ccmd->nextOverload));

        // Any extra info?
        if(info)
        {
            // Lets indent for neatness.
            size_t const infoLen = strlen(info);
            char* line, *infoCopy, *infoCopyBuf = (char*) M_Malloc(infoLen+1);
            if(!infoCopyBuf) Con_Error("CCmdHelpWhat: Failed on allocation of %lu bytes for info copy buffer.", (unsigned long) (infoLen+1));

            memcpy(infoCopyBuf, info, infoLen+1);

            infoCopy = infoCopyBuf;
            while(*(line = M_StrTok(&infoCopy, "\n")))
            {
                Con_FPrintf(CPF_LIGHT, "  %s\n", line);
            }
            M_Free(infoCopyBuf);
        }
    }

    if(found == 0) // Perhaps its a cvar then?
    {
        if(cvar_t* var = Con_FindVariable(query))
        {
            AutoStr* path = CVar_ComposePath(var);
            char const* description = DH_GetString(DH_Find(Str_Text(path)), HST_DESCRIPTION);
            if(description)
            {
                Con_Printf("%s\n", description);
                found = true;
            }
        }
    }

    if(found == 0) // Maybe an alias?
    {
        if(calias_t* calias = Con_FindAlias(query))
        {
            Con_Printf("An alias for:\n%s\n", calias->command);
            found = true;
        }
    }

    if(found == 0) // Perhaps a game?
    {
        try
        {
            de::Game& game = reinterpret_cast<de::GameCollection&>( *App_GameCollection() ).byIdentityKey(query);
            de::Game::print(game, PGF_EVERYTHING);
            found = true;
        }
        catch(de::GameCollection::NotFoundError const&)
        {} // Ignore this error.
    }

    if(found == 0) // Still not found?
        Con_Printf("There is no help about '%s'.\n", query);
}

D_CMD(HelpApropos)
{
    DENG_UNUSED(argc); DENG_UNUSED(src);

    printApropos(argv[1]);
    return true;
}

D_CMD(HelpWhat)
{
    DENG_UNUSED(argc); DENG_UNUSED(src);

    if(!stricmp(argv[1], "(what)"))
    {
        Con_Printf("You've got to be kidding!\n");
        return true;
    }
    printHelpAbout(argv[1]);
    return true;
}

static int printKnownWordWorker(knownword_t const* word, void* parameters)
{
    DENG_ASSERT(word);
    uint* numPrinted = (uint*)parameters;

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
        break; }

    case WT_CVAR: {
        cvar_t* cvar = (cvar_t*) word->data;

        if(cvar->flags & CVF_HIDE)
            return 0; // Skip hidden variables.

        Con_PrintCVar(cvar, "  ");
        break; }

    case WT_CALIAS: {
        calias_t* cal = (calias_t*) word->data;
        Con_FPrintf(CPF_LIGHT|CPF_YELLOW, "  %s == %s\n", cal->name, cal->command);
        break; }

    case WT_GAME: {
        de::Game* game = (de::Game*) word->data;
        Con_FPrintf(CPF_LIGHT|CPF_BLUE, "  %s\n", Str_Text(&game->identityKey()));
        break; }

    default:
        Con_Error("printKnownWordWorker: Invalid word type %i.", (int)word->type);
        exit(1); // Unreachable.
    }

    if(numPrinted) ++(*numPrinted);
    return 0; // Continue iteration.
}

/// @note Part of the Doomsday public API.
void Con_SetUri2(char const* path, Uri const* uri, int svFlags)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetUri2(var, uri, svFlags);
}

/// @note Part of the Doomsday public API.
void Con_SetUri(char const* path, Uri const* uri)
{
    Con_SetUri2(path, uri, 0);
}

/// @note Part of the Doomsday public API.
void Con_SetString2(char const* path, char const* text, int svFlags)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetString2(var, text, svFlags);
}

/// @note Part of the Doomsday public API.
void Con_SetString(char const* path, char const* text)
{
    Con_SetString2(path, text, 0);
}

/// @note Part of the Doomsday public API.
void Con_SetInteger2(char const* path, int value, int svFlags)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetInteger2(var, value, svFlags);
}

/// @note Part of the Doomsday public API.
void Con_SetInteger(char const* path, int value)
{
    Con_SetInteger2(path, value, 0);
}

/// @note Part of the Doomsday public API.
void Con_SetFloat2(char const* path, float value, int svFlags)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetFloat2(var, value, svFlags);
}

/// @note Part of the Doomsday public API.
void Con_SetFloat(char const* path, float value)
{
    Con_SetFloat2(path, value, 0);
}

/// @note Part of the Doomsday public API.
int Con_GetInteger(char const* path)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return 0;
    return CVar_Integer(var);
}

/// @note Part of the Doomsday public API.
float Con_GetFloat(char const* path)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return 0;
    return CVar_Float(var);
}

/// @note Part of the Doomsday public API.
byte Con_GetByte(char const* path)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return 0;
    return CVar_Byte(var);
}

/// @note Part of the Doomsday public API.
char const* Con_GetString(char const* path)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return Str_Text(emptyStr);
    return CVar_String(var);
}

/// @note Part of the Doomsday public API.
Uri const* Con_GetUri(char const* path)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return emptyUri;
    return CVar_Uri(var);
}

D_CMD(ListCmds)
{
    DENG_UNUSED(src);

    Con_Printf("Console commands:\n");
    uint numPrinted = 0;
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CCMD, printKnownWordWorker, &numPrinted);
    Con_Printf("Found %u console commands.\n", numPrinted);
    return true;
}

D_CMD(ListVars)
{
    DENG_UNUSED(src);

    uint numPrinted = 0;
    Con_Printf("Console variables:\n");
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CVAR, printKnownWordWorker, &numPrinted);
    Con_Printf("Found %u console variables.\n", numPrinted);
    return true;
}

#if _DEBUG
D_CMD(PrintVarStats)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    uint numCVars = 0, numCVarsHidden = 0;

    Con_FPrintf(CPF_YELLOW, "Console Variable Statistics:\n");
    if(cvarDirectory)
    {
        countvariableparams_t p;
        p.hidden = false;
        p.ignoreHidden = false;
        for(uint i = uint(CVT_BYTE); i < uint(CVARTYPE_COUNT); ++i)
        {
            p.count = 0;
            p.type = cvartype_t(i);
            cvarDirectory->traverse(PCF_NO_BRANCH, NULL, CVarDirectory::no_hash, countVariable, &p);
            Con_Printf("%12s: %u\n", Str_Text(CVar_TypeName(p.type)), p.count);
        }
        p.count = 0;
        p.type = cvartype_t(-1);
        p.hidden = true;

        cvarDirectory->traverse(PCF_NO_BRANCH, NULL, CVarDirectory::no_hash, countVariable, &p);
        numCVars = cvarDirectory->size();
        numCVarsHidden = p.count;
    }
    Con_Printf("       Total: %u\n      Hidden: %u\n\n", numCVars, numCVarsHidden);

    if(cvarDirectory)
    {
        CVarDirectory::debugPrintHashDistribution(*cvarDirectory);
        CVarDirectory::debugPrint(*cvarDirectory, CVARDIRECTORY_DELIMITER);
    }
    return true;
}
#endif

D_CMD(ListAliases)
{
    DENG_UNUSED(src);

    Con_Printf("Aliases:\n");
    uint numPrinted = 0;
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CALIAS, printKnownWordWorker, &numPrinted);
    Con_Printf("Found %u aliases.\n", numPrinted);
    return true;
}
