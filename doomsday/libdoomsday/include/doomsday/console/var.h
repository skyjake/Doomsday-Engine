/** @file var.h
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_CONSOLE_VAR_H
#define LIBDOOMSDAY_CONSOLE_VAR_H

#include "dd_share.h"
#include <de/String>
#include "../uri.h"

typedef struct cvar_s {
    /// @ref consoleVariableFlags
    int flags;

    /// Type of this variable.
    cvartype_t type;

    /// Pointer to this variable's node in the directory.
    void *directoryNode;

    /// Pointer to the user data.
    void *ptr;

    /// Minimum and maximum values (for ints and floats).
    float min, max;

    /// On-change notification callback.
    void (*notifyChanged)();
} cvar_t;

void Con_InitVariableDirectory();
void Con_DeinitVariableDirectory();
void Con_ClearVariables();
void Con_AddKnownWordsForVariables();

ddstring_t const *CVar_TypeName(cvartype_t type);

/// @return  @ref consoleVariableFlags
int CVar_Flags(cvar_t const *var);

/// @return  Type of the variable.
cvartype_t CVar_Type(cvar_t const *var);

/// @return  Symbolic name/path-to the variable.
AutoStr *CVar_ComposePath(cvar_t const *var);

int CVar_Integer(cvar_t const *var);
float CVar_Float(cvar_t const *var);
byte CVar_Byte(cvar_t const *var);
char const *CVar_String(cvar_t const *var);
de::Uri const &CVar_Uri(cvar_t const *var);

void CVar_SetUri(cvar_t *var, de::Uri const &uri);
void CVar_SetUri2(cvar_t *var, de::Uri const &uri, int svFlags);
void CVar_SetString(cvar_t* var, char const* text);
void CVar_SetString2(cvar_t *var, char const *text, int svFlags);
void CVar_SetInteger(cvar_t* var, int value);
void CVar_SetInteger2(cvar_t* var, int value, int svFlags);
void CVar_SetFloat(cvar_t* var, float value);
void CVar_SetFloat2(cvar_t* var, float value, int svFlags);

void CVar_PrintReadOnlyWarning(cvar_t const *var);

void Con_AddVariable(cvartemplate_t const *tpl);
void Con_AddVariableList(cvartemplate_t const *tplList);
cvar_t *Con_FindVariable(char const *path);

void Con_PrintCVar(cvar_t *cvar, char const *prefix);

de::String Con_VarAsStyledText(cvar_t *var, char const *prefix);

#endif // LIBDOOMSDAY_CONSOLE_VAR_H
