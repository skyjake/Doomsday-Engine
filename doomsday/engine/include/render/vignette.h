/**
 * @file vignette.h
 * Renders a vignette for the player view. @ingroup render
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_RENDER_VIGNETTE_H
#define LIBDENG_RENDER_VIGNETTE_H

#include <de/rect.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the vignette rendering module. This includes registering console
 * commands.
 */
void Vignette_Register(void);

/**
 * Renders the vignette for the player's view.
 *
 * @param viewRect  View window where to draw the vignette.
 * @param fov       Current horizontal FOV angle (degrees).
 */
void Vignette_Render(const RectRaw* viewRect, float fov);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_RENDER_VIGNETTE_H
