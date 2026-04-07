/** @file x_console.h  Hexen specific console settings and commands.
 * @ingroup libhexen
 *
 * @authors Copyright © 2009-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHEXEN_CONSOLE_H
#define LIBHEXEN_CONSOLE_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Add the console variables and commands.
 */
void G_ConsoleRegistration(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBHEXEN_CONSOLE_H
