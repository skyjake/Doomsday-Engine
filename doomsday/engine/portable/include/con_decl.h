/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * con_decl.h: Console Utility Macros
 */

#ifndef __DOOMSDAY_CONSOLE_DECL_H__
#define __DOOMSDAY_CONSOLE_DECL_H__

#include "con_main.h"

/*
 * Macros for creating new console commands.
 */
#define C_CMD(name, fn, help, flags) \
    { ccmd_t _c = { name, CCmd##fn, help, flags }; Con_AddCommand(&_c); }

// A handy helper for declaring console commands.
#define D_CMD(x) int CCmd##x(int src, int argc, char **argv)

/*
 * Macros for creating new console variables.
 */
#define C_VAR(name, ptr, type, flags, min, max, help, notifyChanged)            \
    { cvar_t _v = { name, flags, type, ptr, min, max, help, notifyChanged };    \
        Con_AddVariable(&_v); }

#define C_VAR_BYTE(name, ptr, flags, min, max, help)    \
    C_VAR(name, ptr, CVT_BYTE, flags, min, max, help, NULL)

#define C_VAR_INT(name, ptr, flags, min, max, help)     \
    C_VAR(name, ptr, CVT_INT, flags, min, max, help, NULL)

#define C_VAR_FLOAT(name, ptr, flags, min, max, help) \
    C_VAR(name, ptr, CVT_FLOAT, flags, min, max, help, NULL)

#define C_VAR_CHARPTR(name, ptr, flags, min, max, help) \
    C_VAR(name, ptr, CVT_CHARPTR, flags, min, max, help, NULL)

// Same as above but allow for a change notification callback func
#define C_VAR_BYTE2(name, ptr, flags, min, max, help, notifyChanged)    \
    C_VAR(name, ptr, CVT_BYTE, flags, min, max, help, notifyChanged)

#define C_VAR_INT2(name, ptr, flags, min, max, help, notifyChanged)     \
    C_VAR(name, ptr, CVT_INT, flags, min, max, help, notifyChanged)

#define C_VAR_FLOAT2(name, ptr, flags, min, max, help, notifyChanged) \
    C_VAR(name, ptr, CVT_FLOAT, flags, min, max, help, notifyChanged)

#define C_VAR_CHARPTR2(name, ptr, flags, min, max, help, notifyChanged) \
    C_VAR(name, ptr, CVT_CHARPTR, flags, min, max, help, notifyChanged)

#endif
