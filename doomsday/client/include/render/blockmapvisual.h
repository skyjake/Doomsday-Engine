/** @file render/blockmapvisual.h Graphical Blockmap Visual.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_BLOCKMAPVISUAL_H
#define DENG_RENDER_BLOCKMAPVISUAL_H

#include <de/libdeng1.h>

DENG_EXTERN_C byte bmapShowDebug; ///< cvar
DENG_EXTERN_C float bmapDebugSize; ///< cvar

/**
 * Render the Blockmap debugging visual.
 */
DENG_EXTERN_C void Rend_BlockmapDebug();

#endif // DENG_RENDER_BLOCKMAPVISUAL_H
