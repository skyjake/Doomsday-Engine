/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
 *                    (add more authors here)
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
 * dd_gameapi.cc: C Wrappers for Game Library API Calls
 *
 * This is for convenience only, so that all source files need not be
 * converted to C++ within libdeng.
 */

// HEADER FILES ------------------------------------------------------------

#include "dd_gameapi.h"

#include <de/App>
#include <de/Library>

using namespace de;

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

int game_GetInteger(int id)
{
    return GAME_SYMBOL(deng_GetInteger)(id);
}

const char* game_GetString(int id)
{
    return GAME_SYMBOL(deng_GetString)(id);
}

void* game_GetAddress(int id)
{
    return GAME_SYMBOL(deng_GetAddress)(id);
}

void game_Ticker(double tickLength)
{
    GAME_SYMBOL(deng_Ticker)(tickLength);
}

void game_Call(const char* funcName)
{
    typedef void (*func_t)(void);
    func_t ptr = App::game().symbol<func_t>(funcName);
    if(ptr)
    {
        ptr();
    }
}
