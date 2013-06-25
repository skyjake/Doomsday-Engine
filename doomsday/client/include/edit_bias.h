/** @file edit_bias.h Shadow Bias editor UI.
 *
 * @authors Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_CLIENT_SHADOWBIAS_EDITOR
#define DENG_CLIENT_SHADOWBIAS_EDITOR

#ifdef __CLIENT__

#include <de/libdeng1.h>

class HueCircle;

DENG_EXTERN_C int editHidden;      ///< cvar Is the GUI currently hidden?
DENG_EXTERN_C int editBlink;       ///< cvar Blinking the nearest source (unless grabbed)?
DENG_EXTERN_C int editShowAll;     ///< cvar Show all sources?
DENG_EXTERN_C int editShowIndices; ///< cvar Show source indicies?

/**
 * To be called to register the commands and variables of this module.
 */
void SBE_Register();

/**
 * Returns @c true if the Shadow Bias editor is currently active.
 */
bool SBE_Active();

/**
 * Draw the GUI widgets of the Shadow Bias editor.
 */
void SBE_DrawGui();

/**
 * Returns a pointer to the currently active HueCircle for the console player;
 * otherwise @c 0 if no hue circle is active.
 */
HueCircle *SBE_HueCircle();

#endif // __CLIENT__

#endif // DENG_CLIENT_SHADOWBIAS_EDITOR
