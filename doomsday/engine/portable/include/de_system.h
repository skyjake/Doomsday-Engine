/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_SYSTEM_H
#define LIBDENG_SYSTEM_H

#include <de/concurrency.h>

#include "sys_system.h"
#include "sys_console.h"
#include "sys_input.h"
#include "sys_network.h"
#include "masterserver.h"
#include "timer.h"
#include "sys_opengl.h"

// Use SDL for window management under *nix
#ifdef UNIX
#  define USING_SDL_WINDOW
#  include "../../unix/include/sys_path.h"
#endif

#include "window.h"

#endif /* LIBDENG_SYSTEM_H */
