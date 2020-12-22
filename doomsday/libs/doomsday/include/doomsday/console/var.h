/** @file var.h
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

#ifndef LIBDOOMSDAY_CONSOLE_VAR_H
#define LIBDOOMSDAY_CONSOLE_VAR_H

#include "../libdoomsday.h"
#include <de/legacy/types.h>
#include <de/legacy/str.h>
#include "../uri.h"

#ifdef __cplusplus
#  include <de/string.h>
#  include <de/path.h>
#endif

/// Console variable types. @ingroup console
typedef enum {
    CVT_NULL,
    CVT_BYTE,
    CVT_INT,
    CVT_FLOAT,
    CVT_CHARPTR, ///< ptr points to a char*, which points to the string.
    CVT_URIPTR, ///< ptr points to a Uri*, which points to the uri.
    CVARTYPE_COUNT
} cvartype_t;

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

#define VALID_CVARTYPE(val) ((val) >= CVT_NULL && (val) < CVARTYPE_COUNT)

/**
 * Console variable template. Used with Con_AddVariable().
 * @ingroup cvar
 */
typedef struct cvartemplate_s {
    /// Path of the variable.
    const char* path;

    /// @ref consoleVariableFlags
    int flags;

    /// Type of variable.
    cvartype_t type;

    /// Pointer to the user data.
    void* ptr;

    /// Minimum and maximum values (for ints and floats).
    float min, max;

    /// On-change notification callback.
    void (*notifyChanged)(void);
} cvartemplate_t;

/**
 * @defgroup consoleVariableFlags Console Variable Flags
 * @ingroup apiFlags cvar
 */
///@{
#define CVF_NO_ARCHIVE      0x1 ///< Not written in/read from the defaults file.
#define CVF_PROTECTED       0x2 ///< Can't be changed unless forced.
#define CVF_NO_MIN          0x4 ///< Minimum is not in affect.
#define CVF_NO_MAX          0x8 ///< Maximum is not in affect.
#define CVF_CAN_FREE        0x10 ///< The string can be freed.
#define CVF_HIDE            0x20 ///< Do not include in listings or add to known words.
#define CVF_READ_ONLY       0x40 ///< Can't be changed manually at all.
///@}

/**
 * @defgroup setVariableFlags Console Set Variable Flags
 * @ingroup apiFlags cvar
 * Use with the various Con_Set* routines (e.g., Con_SetInteger2()).
 */
///@{
#define SVF_WRITE_OVERRIDE  0x1 ///< Override a read-only restriction.
///@}

#define C_VAR(path, ptr, type, flags, min, max, notifyChanged)            \
    { cvartemplate_t _template = { path, flags, type, ptr, min, max, notifyChanged };    \
        Con_AddVariable(&_template); }

/// Helper macro for registering a new byte console variable. @ingroup cvar
#define C_VAR_BYTE(path, ptr, flags, min, max)    \
    C_VAR(path, ptr, CVT_BYTE, flags, min, max, NULL)

/// Helper macro for registering a new integer console variable. @ingroup cvar
#define C_VAR_INT(path, ptr, flags, min, max)     \
    C_VAR(path, ptr, CVT_INT, flags, min, max, NULL)

/// Helper macro for registering a new float console variable. @ingroup cvar
#define C_VAR_FLOAT(path, ptr, flags, min, max) \
    C_VAR(path, ptr, CVT_FLOAT, flags, min, max, NULL)

/// Helper macro for registering a new text console variable. @ingroup cvar
#define C_VAR_CHARPTR(path, ptr, flags, min, max) \
    C_VAR(path, ptr, CVT_CHARPTR, flags, min, max, NULL)

/// Helper macro for registering a new Uri console variable. @ingroup cvar
#define C_VAR_URIPTR(path, ptr, flags, min, max) \
    C_VAR(path, ptr, CVT_URIPTR, flags, min, max, NULL)

/// Same as C_VAR_BYTE() except allows specifying a callback function for
/// change notification. @ingroup cvar
#define C_VAR_BYTE2(path, ptr, flags, min, max, notifyChanged)    \
    C_VAR(path, ptr, CVT_BYTE, flags, min, max, notifyChanged)

/// Same as C_VAR_INT() except allows specifying a callback function for
/// change notification. @ingroup cvar
#define C_VAR_INT2(path, ptr, flags, min, max, notifyChanged)     \
    C_VAR(path, ptr, CVT_INT, flags, min, max, notifyChanged)

/// Same as C_VAR_FLOAT() except allows specifying a callback function for
/// change notification. @ingroup cvar
#define C_VAR_FLOAT2(path, ptr, flags, min, max, notifyChanged) \
    C_VAR(path, ptr, CVT_FLOAT, flags, min, max, notifyChanged)

/// Same as C_VAR_CHARPTR() except allows specifying a callback function for
/// change notification. @ingroup cvar
#define C_VAR_CHARPTR2(path, ptr, flags, min, max, notifyChanged) \
    C_VAR(path, ptr, CVT_CHARPTR, flags, min, max, notifyChanged)

/// Same as C_VAR_URIPTR() except allows specifying a callback function for
/// change notification. @ingroup cvar
#define C_VAR_URIPTR2(path, ptr, flags, min, max, notifyChanged) \
    C_VAR(path, ptr, CVT_URIPTR, flags, min, max, notifyChanged)

#ifdef __cplusplus

void Con_InitVariableDirectory();
void Con_DeinitVariableDirectory();
void Con_ClearVariables();
void Con_AddKnownWordsForVariables();

LIBDOOMSDAY_PUBLIC void Con_AddVariable(const cvartemplate_t *tpl);
LIBDOOMSDAY_PUBLIC void Con_AddVariableList(const cvartemplate_t *tplList);
LIBDOOMSDAY_PUBLIC cvar_t *Con_FindVariable(const char *path);

LIBDOOMSDAY_PUBLIC const ddstring_t *CVar_TypeName(cvartype_t type);

/// @return  @ref consoleVariableFlags
LIBDOOMSDAY_PUBLIC int CVar_Flags(const cvar_t *var);

/// @return  Type of the variable.
LIBDOOMSDAY_PUBLIC cvartype_t CVar_Type(const cvar_t *var);

/// @return  Symbolic name/path-to the variable.
LIBDOOMSDAY_PUBLIC AutoStr *CVar_ComposePath(const cvar_t *var);

LIBDOOMSDAY_PUBLIC int CVar_Integer(const cvar_t *var);
LIBDOOMSDAY_PUBLIC float CVar_Float(const cvar_t *var);
LIBDOOMSDAY_PUBLIC byte CVar_Byte(const cvar_t *var);
LIBDOOMSDAY_PUBLIC const char *CVar_String(const cvar_t *var);
LIBDOOMSDAY_PUBLIC const res::Uri &CVar_Uri(const cvar_t *var);

LIBDOOMSDAY_PUBLIC void CVar_SetUri(cvar_t *var, const res::Uri &uri);
LIBDOOMSDAY_PUBLIC void CVar_SetUri2(cvar_t *var, const res::Uri &uri, int svFlags);
LIBDOOMSDAY_PUBLIC void CVar_SetString(cvar_t* var, char const* text);
LIBDOOMSDAY_PUBLIC void CVar_SetString2(cvar_t *var, const char *text, int svFlags);
LIBDOOMSDAY_PUBLIC void CVar_SetInteger(cvar_t* var, int value);
LIBDOOMSDAY_PUBLIC void CVar_SetInteger2(cvar_t* var, int value, int svFlags);
LIBDOOMSDAY_PUBLIC void CVar_SetFloat(cvar_t* var, float value);
LIBDOOMSDAY_PUBLIC void CVar_SetFloat2(cvar_t* var, float value, int svFlags);

LIBDOOMSDAY_PUBLIC void Con_SetVariable(const de::Path &varPath, int value, int svFlags = 0);
LIBDOOMSDAY_PUBLIC int Con_GetVariableInteger(const de::Path &varPath);

LIBDOOMSDAY_PUBLIC void Con_PrintCVar(cvar_t *cvar, const char *prefix);
LIBDOOMSDAY_PUBLIC void CVar_PrintReadOnlyWarning(const cvar_t *var);
LIBDOOMSDAY_PUBLIC de::String Con_VarAsStyledText(cvar_t *var, const char *prefix);

#endif // __cplusplus

#endif // LIBDOOMSDAY_CONSOLE_VAR_H
