/** @file biassurface.cpp Shadow Bias surface.
 *
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

#include "de_console.h"

#include "render/biassurface.h"

static int devUpdateBiasContributors = true; //cvar

void BiasSurface::consoleRegister() // static
{
    // Development variables.
    C_VAR_INT("rend-dev-bias-affected", &devUpdateBiasContributors, CVF_NO_ARCHIVE, 0, 1);
}
