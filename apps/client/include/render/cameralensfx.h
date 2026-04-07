/** @file cameralensfx.h  Camera lens effects.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_CAMERALENSFX_H
#define DE_CLIENT_CAMERALENSFX_H

#include "ilightsource.h"

void LensFx_Register();
void LensFx_Init();
void LensFx_Shutdown();

/**
 * Deinitializes all console effects. This is called when the viewport
 * configuration changes so that GL resources for unnecessary consoles are not
 * retained.
 */
void LensFx_GLRelease();

/**
 * Draws all camera lens effects into the player's latest game view texture.
 *
 * The order of operations:
 * - All effects are notified of the start of the draw operation.
 * - All effects are drawn.
 * - All effects are notified of the end of the draw operation in reverse order.
 *
 * @param playerNum  Player/console number.
 */
void LensFx_Draw(int playerNum);

/*
 * Finishes camera lens FX rendering of a frame. The drawn frame may be
 * post-processed, and any additional effects (vignette, flares) are added on
 * top.
 */
//void LensFx_EndFrame();

/**
 * Marks a light potentially visible in the current frame.
 */
void LensFx_MarkLightVisible(const IPointLightSource &lightSource);

#endif // DE_CLIENT_CAMERALENSFX_H
