/** @file cameralensfx.h  Camera lens effects.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_CAMERALENSFX_H
#define DENG_CLIENT_CAMERALENSFX_H

void LensFx_Init();
void LensFx_Shutdown();

/**
 * Deinitializes all console effects. This is called when the viewport
 * configuration changes so that GL resources for unnecessary consoles are not
 * retained.
 */
void LensFx_GLRelease();

/**
 * Notifies camera lens FX that the rendering of a world view frame will begin.
 * All graphics until LensFx_EndFrame() are considered part of the the frame.
 * The render target may change during this call if additional post-procesing
 * effects will require it.
 *
 * @param playerNum  Player/console number.
 */
void LensFx_BeginFrame(int playerNum);

/**
 * Finishes camera lens FX rendering of a frame. The drawn frame may be
 * post-processed, and any additional effects (vignette, flares) are added on
 * top.
 */
void LensFx_EndFrame();

#endif // DENG_CLIENT_CAMERALENSFX_H
