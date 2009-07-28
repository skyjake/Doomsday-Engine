/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * m_args.cc: Command Line Arguments
 */

// HEADER FILES ------------------------------------------------------------

#include <de/App>
#include <de/CommandLine>

using namespace de;

#include "dd_export.h"
BEGIN_EXTERN_C
#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"
END_EXTERN_C
#include "doomsday.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int lastMatch = 0;

// CODE --------------------------------------------------------------------

inline CommandLine& appCommandLine()
{
    return App::app().commandLine();
}

/**
 * Registers a short name for a long arg name.
 * The short name can then be used on the command line and CheckParm
 * will know to match occurances of the short name with the long name.
 */
void ArgAbbreviate(const char* longName, const char* shortName)
{
    appCommandLine().alias(longName, shortName);
}

/**
 * @return              The number of arguments on the command line.
 */
int Argc(void)
{
    return appCommandLine().count();
}

/**
 * @return              Pointer to the i'th argument.
 */
const char* Argv(int i)
{
    return appCommandLine().at(i).c_str();
}

/**
 * @return              Pointer to the i'th argument's pointer.
 */
const char** ArgvPtr(int i)
{
    const char** args = const_cast<const char**>(appCommandLine().argv());
    if(i < 0 || i >= Argc())
        Con_Error("ArgvPtr: There is no arg %i.\n", i);
    return &args[i];
}

const char* ArgNext(void)
{
    if(!lastMatch || lastMatch >= Argc() - 1)
        return NULL;
    return Argv(++lastMatch);
}

/**
 * @return              @c true, iff the two parameters are equivalent
 *                      according to the abbreviations.
 */
int ArgRecognize(const char* first, const char* second)
{
    return appCommandLine().matches(first, second);
}

/**
 * Checks for the given parameter in the program's command line arguments.
 *
 * @return              The argument number (1 to argc-1) else
 *                      @c 0 if not present.
 */
int ArgCheck(const char* check)
{
    lastMatch = appCommandLine().check(check);
    return lastMatch;
}

/**
 * Checks for the given parameter in the program's command line arguments
 * and it is followed by N other arguments.
 *
 * @return              The argument number (1 to argc-1) else
 *                      @c 0 if not present.
 */
int ArgCheckWith(const char* check, int num)
{
    lastMatch = appCommandLine().check(check, num);
    return lastMatch;
}

/**
 * @return              @c true, if the given argument begins with a hyphen.
 */
int ArgIsOption(int i)
{
    return appCommandLine().isOption(i);
}

/**
 * Determines if an argument exists on the command line.
 *
 * @return              Number of times the argument is found.
 */
int ArgExists(const char* check)
{
    return appCommandLine().has(check);
}
