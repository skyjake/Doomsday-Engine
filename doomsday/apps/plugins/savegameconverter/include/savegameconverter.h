/** @file savegameconverter.h  Legacy savegame converter plugin.
 * @ingroup savegameconverter
 *
 * Uses the command line Savegame Tool for conversion of legacy saved game sessions.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

/**
 * @defgroup savegameconverter
 * Legacy savegame converter plugin.
 */

#ifndef LIBSAVEGAMECONVERTER_MAIN_H
#define LIBSAVEGAMECONVERTER_MAIN_H

#include <doomsday.h>
#include <de/libcore.h>
//#include <de/types.h>

DENG_EXTERN_C void DP_Initialize(void);

DENG_USING_API(Base);
DENG_USING_API(Plug);

#endif // LIBSAVEGAMECONVERTER_MAIN_H
