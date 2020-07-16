/** @file
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#define DE_NO_API_MACROS_URI
#include "doomsday/console/var.h"
#include "doomsday/console/exec.h"
#include "doomsday/console/knownword.h"
#include "doomsday/uri.h"

#include <de/c_wrapper.h>
#include <de/legacy/memory.h>
#include <de/pathtree.h>
#include <de/scripting/function.h>
#include <de/textvalue.h>

using namespace de;

// Substrings in CVar names are delimited by this character.
#define CVARDIRECTORY_DELIMITER         '-'

typedef UserDataPathTree CVarDirectory;

/// Console variable directory.
static CVarDirectory *cvarDirectory;

static ddstring_s *emptyStr;
static res::Uri *emptyUri;

void Con_InitVariableDirectory()
{
    cvarDirectory = new CVarDirectory;
    emptyStr = Str_NewStd();
    emptyUri = new res::Uri;
}

void Con_DeinitVariableDirectory()
{
    delete cvarDirectory; cvarDirectory = 0;
    Str_Delete(emptyStr); emptyStr = 0;
    delete emptyUri; emptyUri = 0;
}

static int markVariableUserDataFreed(CVarDirectory::Node &node, void *context)
{
    DE_ASSERT(context);

    cvar_t *var = reinterpret_cast<cvar_t *>(node.userPointer());
    void **ptr = (void **) context;
    if (var)
    switch (CVar_Type(var))
    {
    case CVT_CHARPTR:
        if (*ptr == CV_CHARPTR(var)) var->flags &= ~CVF_CAN_FREE;
        break;
    case CVT_URIPTR:
        if (*ptr == CV_URIPTR(var)) var->flags &= ~CVF_CAN_FREE;
        break;
    default: break;
    }
    return 0; // Continue iteration.
}

static int clearVariable(CVarDirectory::Node& node, void * /*context*/)
{
    cvar_t *var = reinterpret_cast<cvar_t*>(node.userPointer());
    if (var)
    {
        // Detach our user data from this node.
        node.setUserPointer(0);

        if (CVar_Flags(var) & CVF_CAN_FREE)
        {
            void** ptr = NULL;
            switch (CVar_Type(var))
            {
            case CVT_CHARPTR:
                if (!CV_CHARPTR(var)) break;

                ptr = (void**)var->ptr;
                /// @note Multiple vars could be using the same pointer (so only free once).
                cvarDirectory->traverse(PathTree::NoBranch, NULL, /*CVarDirectory::no_hash, */markVariableUserDataFreed, ptr);
                M_Free(*ptr); *ptr = Str_Text(emptyStr);
                break;

            case CVT_URIPTR:
                if (!CV_URIPTR(var)) break;

                ptr = (void**)var->ptr;
                /// @note Multiple vars could be using the same pointer (so only free once).
                cvarDirectory->traverse(PathTree::NoBranch, NULL, /*CVarDirectory::no_hash, */markVariableUserDataFreed, ptr);
                delete reinterpret_cast<res::Uri *>(*ptr); *ptr = emptyUri;
                break;

            default:
                LOGDEV_SCR_WARNING("Attempt to free user data for non-pointer type variable %s [%p]")
                        << Str_Text(CVar_ComposePath(var)) << var;
                break;
            }
        }
        M_Free(var);
    }
    return 0; // Continue iteration.
}

void Con_ClearVariables()
{
    /// If _DEBUG we'll traverse all nodes and verify our clear logic.
#if _DEBUG
    PathTree::ComparisonFlags flags;
#else
    PathTree::ComparisonFlags flags = PathTree::NoBranch;
#endif
    if (!cvarDirectory) return;

    cvarDirectory->traverse(flags, NULL, /*CVarDirectory::no_hash, */clearVariable);
    cvarDirectory->clear();
}

/// Construct a new variable from the specified template and add it to the database.
static cvar_t* addVariable(cvartemplate_t const& tpl)
{
    Path path(tpl.path, CVARDIRECTORY_DELIMITER);
    CVarDirectory::Node* node = &cvarDirectory->insert(path);
    cvar_t* newVar;

    DE_ASSERT(!node->userPointer());
    if (node->userPointer())
    {
        throw Error("Con_AddVariable", "A variable with path '" + String(tpl.path) + "' is already known!");
    }

    newVar = (cvar_t*) M_Malloc(sizeof *newVar);

    newVar->flags = tpl.flags;
    newVar->type = tpl.type;
    newVar->ptr = tpl.ptr;
    newVar->min = tpl.min;
    newVar->max = tpl.max;
    newVar->notifyChanged = tpl.notifyChanged;
    newVar->directoryNode = node;
    node->setUserPointer(newVar);

    Con_UpdateKnownWords();
    return newVar;
}

String CVar_TypeAsText(const cvar_t *var)
{
    // Human-readable type name.
    DE_ASSERT(var);
    switch (var->type)
    {
    case CVT_BYTE:
        return "byte";
    case CVT_CHARPTR:
        return "text";
    case CVT_FLOAT:
        return "float";
    case CVT_INT:
        return "integer";
    case CVT_NULL:
        return "null";
    case CVT_URIPTR:
        return "uri";
    default:
        DE_ASSERT_FAIL("Con_VarTypeAsText: Unknown variable type");
        break;
    }
    return "";
}

template <typename ValueType>
void printTypeWarning(const cvar_t *var, const String &attemptedType, ValueType value)
{
    AutoStr* path = CVar_ComposePath(var);
    LOG_SCR_WARNING("Variable %s (of type '%s') is incompatible with %s ")
            << Str_Text(path) << CVar_TypeAsText(var)
            << attemptedType << value;
}

void CVar_PrintReadOnlyWarning(const cvar_t *var)
{
    AutoStr* path = CVar_ComposePath(var);
    LOG_SCR_WARNING("%s (%s cvar) is read-only; it cannot be changed (even with force)")
            << CVar_TypeAsText(var) << Str_Text(path);
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
    DE_ASSERT(var);
    return var->type;
}


int CVar_Flags(const cvar_t* var)
{
    DE_ASSERT(var);
    return var->flags;
}

AutoStr *CVar_ComposePath(const cvar_t *var)
{
    DE_ASSERT(var != 0);
    CVarDirectory::Node &node = *reinterpret_cast<CVarDirectory::Node *>(var->directoryNode);
    return AutoStr_FromTextStd(node.path(CVARDIRECTORY_DELIMITER));
}

void CVar_SetUri2(cvar_t *var, const res::Uri &uri, int svFlags)
{
    DE_ASSERT(var);

    res::Uri *newUri;
    bool changed = false;

    if ((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        CVar_PrintReadOnlyWarning(var);
        return;
    }

    if (var->type != CVT_URIPTR)
    {
        App_FatalError("CVar::SetUri: Not of type %s.", Str_Text(CVar_TypeName(CVT_URIPTR)));
        return; // Unreachable.
    }

    /*
    if (!CV_URIPTR(var) && !uri)
    {
        return;
    }
    */

    // Compose the new uri.
    newUri = new res::Uri(uri);

    if (!CV_URIPTR(var) || *CV_URIPTR(var) != *newUri)
    {
        changed = true;
    }

    // Free the old uri, if one exists.
    if ((var->flags & CVF_CAN_FREE) && CV_URIPTR(var))
    {
        delete CV_URIPTR(var);
    }

    var->flags |= CVF_CAN_FREE;
    CV_URIPTR(var) = newUri;

    // Make the change notification callback
    if (var->notifyChanged && changed)
    {
        var->notifyChanged();
    }
}

void CVar_SetUri(cvar_t *var, const res::Uri &uri)
{
    CVar_SetUri2(var, uri, 0);
}

void CVar_SetString2(cvar_t *var, const char *text, int svFlags)
{
    DE_ASSERT(var != 0);

    bool changed = false;
    size_t oldLen, newLen;

    if ((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        CVar_PrintReadOnlyWarning(var);
        return;
    }

    if (var->type != CVT_CHARPTR)
    {
        printTypeWarning(var, "text", text);
        return;
    }

    oldLen = (!CV_CHARPTR(var)? 0 : strlen(CV_CHARPTR(var)));
    newLen = (!text           ? 0 : strlen(text));

    if (oldLen == 0 && newLen == 0)
        return;

    if (oldLen != newLen || iCmpStrCase(text, CV_CHARPTR(var)))
        changed = true;

    // Free the old string, if one exists.
    if ((var->flags & CVF_CAN_FREE) && CV_CHARPTR(var))
        free(CV_CHARPTR(var));

    // Allocate a new string.
    var->flags |= CVF_CAN_FREE;
    CV_CHARPTR(var) = (char*) M_Malloc(newLen + 1);
    strcpy(CV_CHARPTR(var), text);

    // Make the change notification callback
    if (var->notifyChanged != NULL && changed)
        var->notifyChanged();
}

void CVar_SetString(cvar_t* var, char const* text)
{
    CVar_SetString2(var, text, 0);
}

void CVar_SetInteger2(cvar_t* var, int value, int svFlags)
{
    DE_ASSERT(var);

    bool changed = false;

    if ((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        CVar_PrintReadOnlyWarning(var);
        return;
    }

    switch (var->type)
    {
    case CVT_INT:
        if (CV_INT(var) != value)
            changed = true;
        CV_INT(var) = value;
        break;
    case CVT_BYTE:
        if (CV_BYTE(var) != (byte) value)
            changed = true;
        CV_BYTE(var) = (byte) value;
        break;
    case CVT_FLOAT:
        if (CV_FLOAT(var) != (float) value)
            changed = true;
        CV_FLOAT(var) = (float) value;
        break;

    default:
        printTypeWarning(var, "integer", value);
        return;
    }

    // Make a change notification callback?
    if (var->notifyChanged != 0 && changed)
        var->notifyChanged();
}

void CVar_SetInteger(cvar_t* var, int value)
{
    CVar_SetInteger2(var, value, 0);
}

void CVar_SetFloat2(cvar_t* var, float value, int svFlags)
{
    DE_ASSERT(var);

    bool changed = false;

    LOG_AS("CVar_SetFloat2");

    if ((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        CVar_PrintReadOnlyWarning(var);
        return;
    }

    switch (var->type)
    {
    case CVT_INT:
        if (CV_INT(var) != (int) value)
            changed = true;
        CV_INT(var) = (int) value;
        break;
    case CVT_BYTE:
        if (CV_BYTE(var) != (byte) value)
            changed = true;
        CV_BYTE(var) = (byte) value;
        break;
    case CVT_FLOAT:
        if (CV_FLOAT(var) != value)
            changed = true;
        CV_FLOAT(var) = value;
        break;

    default:
        printTypeWarning(var, "float", value);
        return;
    }

    // Make a change notification callback?
    if (var->notifyChanged != 0 && changed)
        var->notifyChanged();
}

void CVar_SetFloat(cvar_t* var, float value)
{
    CVar_SetFloat2(var, value, 0);
}

static void printConversionWarning(const cvar_t *var)
{
    AutoStr* path = CVar_ComposePath(var);
    LOGDEV_SCR_WARNING("Incompatible variable %s [%p type:%s]")
            << Str_Text(path) << var << Str_Text(CVar_TypeName(CVar_Type(var)));
}

int CVar_Integer(cvar_t const* var)
{
    DE_ASSERT(var);
    switch (var->type)
    {
    case CVT_BYTE:      return CV_BYTE(var);
    case CVT_INT:       return CV_INT(var);
    case CVT_FLOAT:     return CV_FLOAT(var);
    case CVT_CHARPTR:   return strtol(CV_CHARPTR(var), 0, 0);
    default: {
        LOG_AS("CVar_Integer");
        printConversionWarning(var);
        return 0; }
    }
}

float CVar_Float(cvar_t const* var)
{
    DE_ASSERT(var);
    switch (var->type)
    {
    case CVT_BYTE:      return CV_BYTE(var);
    case CVT_INT:       return CV_INT(var);
    case CVT_FLOAT:     return CV_FLOAT(var);
    case CVT_CHARPTR:   return strtod(CV_CHARPTR(var), 0);
    default: {
        LOG_AS("CVar_Float");
        printConversionWarning(var);
        return 0; }
    }
}

byte CVar_Byte(cvar_t const* var)
{
    DE_ASSERT(var);
    switch (var->type)
    {
    case CVT_BYTE:      return CV_BYTE(var);
    case CVT_INT:       return CV_INT(var);
    case CVT_FLOAT:     return CV_FLOAT(var);
    case CVT_CHARPTR:   return strtol(CV_CHARPTR(var), 0, 0);
    default: {
        LOG_AS("CVar_Byte");
        printConversionWarning(var);
        return 0; }
    }
}

char const* CVar_String(cvar_t const* var)
{
    DE_ASSERT(var);
    /// @todo Why not implement in-place value to string conversion?
    switch (var->type)
    {
    case CVT_CHARPTR:   return CV_CHARPTR(var);
    default: {
        LOG_AS("CVar_String");
        printConversionWarning(var);
        return Str_Text(emptyStr); }
    }
}

const res::Uri &CVar_Uri(const cvar_t *var)
{
    if (!var) return *emptyUri;

    /// @todo Why not implement in-place string to uri conversion?
    switch (var->type)
    {
    case CVT_URIPTR:   return *CV_URIPTR(var);
    default: {
        LOG_AS("CVar_Uri");
        printConversionWarning(var);
        return *emptyUri; }
    }
}

void Con_AddVariable(const cvartemplate_t *tpl)
{
    LOG_AS("Con_AddVariable");

    if (!tpl) return;

    if (CVT_NULL == tpl->type)
    {
        LOGDEV_SCR_WARNING("Ignored attempt to register variable '%s' as type %s")
            << tpl->path << Str_Text(CVar_TypeName(CVT_NULL));
        return;
    }

    addVariable(*tpl);
}

void Con_AddVariableList(const cvartemplate_t *tplList)
{
    if (!tplList) return;

    for (; tplList->path; ++tplList)
    {
        if (Con_FindVariable(tplList->path))
        {
            App_FatalError("Console variable with the name '%s' is already registered", tplList->path);
        }
        addVariable(*tplList);
    }
}

cvar_t *Con_FindVariable(const Path &path)
{
    if (const CVarDirectory::Node *node =
            cvarDirectory->tryFind(path, PathTree::NoBranch | PathTree::MatchFull))
    {
        return reinterpret_cast<cvar_t *>(node->userPointer());
    }
    return nullptr;
}

cvar_t *Con_FindVariable(const char *path)
{
    return Con_FindVariable(Path(path, CVARDIRECTORY_DELIMITER));
}

String Con_VarAsStyledText(cvar_t *var, const char *prefix)
{
    if (!var) return "";

    char equals = '=';
    if ((var->flags & CVF_PROTECTED) || (var->flags & CVF_READ_ONLY))
        equals = ':';

    std::ostringstream os;

    if (prefix) os << prefix;

    AutoStr* path = CVar_ComposePath(var);

    os << _E(b) << Str_Text(path) << _E(.) << " " << equals << " " << _E(>);

    switch (var->type)
    {
    case CVT_BYTE:      os << int(CV_BYTE(var)); break;
    case CVT_INT:       os << CV_INT(var); break;
    case CVT_FLOAT:     os << CV_FLOAT(var); break;
    case CVT_CHARPTR:   os << "\"" << CV_CHARPTR(var) << "\""; break;
    case CVT_URIPTR: {
        os << "\"" << (CV_URIPTR(var)? CV_URIPTR(var)->asText() : "") << "\"";
        break; }
    default:
        DE_ASSERT_FAIL("Invalid cvar type");
        break;
    }
    os << _E(<);
    return os.str();
}

void Con_PrintCVar(cvar_t* var, const char *prefix)
{
    LOG_SCR_MSG("%s") << Con_VarAsStyledText(var, prefix);
}

static int addVariableToKnownWords(CVarDirectory::Node& node, void* /*parameters*/)
{
    //DE_ASSERT(parameters);

    cvar_t* var = reinterpret_cast<cvar_t*>( node.userPointer() );
    //uint* index = (uint*) parameters;
    if (var && !(var->flags & CVF_HIDE))
    {
        Con_AddKnownWord(WT_CVAR, var);
    }
    return 0; // Continue iteration.
}

void Con_AddKnownWordsForVariables()
{
    if (!cvarDirectory) return;

    cvarDirectory->traverse(PathTree::NoBranch, NULL, //CVarDirectory::no_hash,
                            addVariableToKnownWords);
}

void Con_SetVariable(const Path &varPath, int value, int svFlags)
{
    if (cvar_t *var = Con_FindVariable(varPath))
    {
        CVar_SetInteger2(var, value, svFlags);
    }
    else
    {
        DE_ASSERT_FAIL("Con_SetVariable: Invalid console variable path");
    }
}

int Con_GetVariableInteger(const Path &varPath)
{
    if (cvar_t *var = Con_FindVariable(varPath))
    {
        return CVar_Integer(var);
    }
    return 0;
}

static Value *Function_Console_Get(Context &, const Function::ArgumentValues &args)
{
    const String name = args.at(0)->asText();
    cvar_t *var = Con_FindVariable(name.c_str());
    if (!var)
    {
        throw Error("Function_Console_Get",
                    stringf("Unknown console variable: %s", name.c_str()));
    }
    switch (var->type)
    {
    case CVT_BYTE:
        return new NumberValue(CVar_Byte(var));

    case CVT_INT:
        return new NumberValue(CVar_Integer(var));

    case CVT_FLOAT:
        return new NumberValue(CVar_Float(var));

    case CVT_CHARPTR:  ///< ptr points to a char*, which points to the string.
        return new TextValue(CVar_String(var));

    case CVT_URIPTR:
        return new TextValue(CVar_Uri(var).asText());

    default:
        break;
    }
    return nullptr;
}

static Value *Function_Console_Set(Context &, const Function::ArgumentValues &args)
{
    const String name = args.at(0)->asText();
    cvar_t *var = Con_FindVariable(name.c_str());
    if (!var)
    {
        throw Error("Function_Console_Set",
                    stringf("Unknown console variable: %s", name.c_str()));
    }

    const Value &value = *args.at(1);
    switch (var->type)
    {
    case CVT_BYTE:
    case CVT_INT:
        CVar_SetInteger(var, value.asInt());
        break;

    case CVT_FLOAT:
        CVar_SetFloat(var, value.asNumber());
        break;

    case CVT_CHARPTR:  ///< ptr points to a char*, which points to the string.
        CVar_SetString(var, value.asText());
        break;

    case CVT_URIPTR:
        CVar_SetUri(var, res::Uri(value.asText()));
        break;

    default:
        break;
    }

    return nullptr;
}

void initVariableBindings(Binder &binder)
{
    binder
        << DE_FUNC(Console_Get, "get", "name")
        << DE_FUNC(Console_Set, "set", "name" << "value");
}

#ifdef DE_DEBUG
typedef struct {
    uint count;
    cvartype_t type;
    dd_bool hidden;
    dd_bool ignoreHidden;
} countvariableparams_t;

static int countVariable(CVarDirectory::Node& node, void* parameters)
{
    DE_ASSERT(parameters);

    countvariableparams_t* p = (countvariableparams_t*) parameters;
    cvar_t* var = reinterpret_cast<cvar_t*>( node.userPointer() );

    if (!(p->ignoreHidden && (var->flags & CVF_HIDE)))
    {
        if (!VALID_CVARTYPE(p->type) && !p->hidden)
        {
            if (!p->ignoreHidden || !(var->flags & CVF_HIDE))
                ++(p->count);
        }
        else if ((p->hidden && (var->flags & CVF_HIDE)) ||
                (VALID_CVARTYPE(p->type) && p->type == CVar_Type(var)))
        {
            ++(p->count);
        }
    }
    return 0; // Continue iteration.
}

D_CMD(PrintVarStats)
{
    DE_UNUSED(src, argc, argv);

    uint numCVars = 0, numCVarsHidden = 0;

    LOG_SCR_MSG(_E(b) "Console Variable Statistics:");
    if (cvarDirectory)
    {
        countvariableparams_t p;
        p.hidden = false;
        p.ignoreHidden = false;
        for (uint i = uint(CVT_BYTE); i < uint(CVARTYPE_COUNT); ++i)
        {
            p.count = 0;
            p.type = cvartype_t(i);
            cvarDirectory->traverse(PathTree::NoBranch, NULL, /*CVarDirectory::no_hash, */countVariable, &p);
            LOGDEV_SCR_MSG("%12s: %i") << Str_Text(CVar_TypeName(p.type)) << p.count;
        }
        p.count = 0;
        p.type = cvartype_t(-1);
        p.hidden = true;

        cvarDirectory->traverse(PathTree::NoBranch, NULL, /*CVarDirectory::no_hash, */countVariable, &p);
        numCVars = cvarDirectory->size();
        numCVarsHidden = p.count;
    }
    LOG_SCR_MSG("       Total: %i\n      Hidden: %i") << numCVars << numCVarsHidden;

    if (cvarDirectory)
    {
        cvarDirectory->debugPrintHashDistribution();
        cvarDirectory->debugPrint(CVARDIRECTORY_DELIMITER);
    }
    return true;
}
#endif
