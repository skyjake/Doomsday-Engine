/**\file rend_sky.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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
 * Sky Sphere and 3D Models.
 *
 * This version supports only two sky layers.
 * (More would be a waste of resources?)
 */

#ifndef LIBDENG_RENDER_SKY_H
#define LIBDENG_RENDER_SKY_H

/// Register the console commands, variables, etc..., of this module.
void Rend_SkyRegister(void);

/// Initialize this module.
void Rend_SkyInit(void);

/// Shutdown this module.
void Rend_SkyShutdown(void);

/// Render the current sky.
void Rend_RenderSky(void);

#endif /* LIBDENG_RENDER_SKY_H */
