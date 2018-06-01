/** @file api_console.h Public console API.
 * @ingroup console
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_API_CONSOLE_H
#define DOOMSDAY_API_CONSOLE_H

#include "api_base.h"
#include "dd_share.h"

/// @addtogroup console
///@{

DE_API_TYPEDEF(Con)
{
    de_api_t api;

    void (*Open)(int yes);
    void (*AddCommand)(ccmdtemplate_t const* cmd);
    void (*AddVariable)(cvartemplate_t const* var);
    void (*AddCommandList)(ccmdtemplate_t const* cmdList);
    void (*AddVariableList)(cvartemplate_t const* varList);

    /// @return  Type of the variable associated with @a path if found else @c CVT_NULL
    cvartype_t (*GetVariableType)(char const* name);

    byte (*GetByte)(char const* name);
    int (*GetInteger)(char const* name);
    float (*GetFloat)(char const* name);
    char const* (*GetString)(char const* name);
    struct uri_s const* (*GetUri)(char const* name);

    /**
     * @copydoc CVar_SetInteger()
     * @param svflags  @ref setVariableFlags
     */
    void (*SetInteger2)(char const* name, int value, int svflags);

    /**
     * Changes the value of an integer variable.
     * @note Also used with @c CVT_BYTE.
     *
     * @param var    Variable.
     * @param value  New integer value for the variable.
     */
    void (*SetInteger)(char const* name, int value);

    void (*SetFloat2)(char const* name, float value, int svflags);
    void (*SetFloat)(char const* name, float value);

    void (*SetString2)(char const* name, char const* text, int svflags);
    void (*SetString)(char const* name, char const* text);

    void (*SetUri2)(char const* name, struct uri_s const* uri, int svflags);
    void (*SetUri)(char const* name, struct uri_s const* uri);

    void (*Error)(char const* error, ...);

    int (*Execute)(int silent, char const* command);
    int (*Executef)(int silent, char const* command, ...);
}
DE_API_T(Con);

#ifndef DE_NO_API_MACROS_CONSOLE
#define Con_Open                _api_Con.Open
#define Con_AddCommand          _api_Con.AddCommand
#define Con_AddVariable         _api_Con.AddVariable
#define Con_AddCommandList      _api_Con.AddCommandList
#define Con_AddVariableList     _api_Con.AddVariableList

#define Con_GetVariableType     _api_Con.GetVariableType

#define Con_GetByte             _api_Con.GetByte
#define Con_GetInteger          _api_Con.GetInteger
#define Con_GetFloat            _api_Con.GetFloat
#define Con_GetString           _api_Con.GetString
#define Con_GetUri              _api_Con.GetUri

#define Con_SetInteger2         _api_Con.SetInteger2
#define Con_SetInteger          _api_Con.SetInteger

#define Con_SetFloat2           _api_Con.SetFloat2
#define Con_SetFloat            _api_Con.SetFloat

#define Con_SetString2          _api_Con.SetString2
#define Con_SetString           _api_Con.SetString

#define Con_SetUri2             _api_Con.SetUri2
#define Con_SetUri              _api_Con.SetUri

#define Con_Error               _api_Con.Error

#define DD_Execute              _api_Con.Execute
#define DD_Executef             _api_Con.Executef
#endif

#ifdef __DOOMSDAY__
DE_USING_API(Con);
#endif

///@}

#endif // DOOMSDAY_API_CONSOLE_H
