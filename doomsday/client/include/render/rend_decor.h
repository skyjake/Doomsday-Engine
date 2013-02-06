/** @file rend_decor.h Surface Decoration Projection.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RENDER_DECOR_H
#define LIBDENG_RENDER_DECOR_H

#include <de/libdeng1.h>

DENG_EXTERN_C byte useLightDecorations;
DENG_EXTERN_C float decorLightBrightFactor;
DENG_EXTERN_C float decorLightFadeAngle;

#ifdef __cplusplus
extern "C" {
#endif

void Rend_DecorRegister(void);

/**
 * Re-initialize the decoration source tracking (might be called during a map
 * load or othersuch situation).
 */
void Rend_DecorInit(void);

/**
 * Decorations are generated for each frame.
 */
void Rend_DecorBeginFrame(void);

/**
 * Create lumobjs for all decorations who want them.
 */
void Rend_DecorAddLuminous(void);

/**
 * Project all the non-clipped decorations. They become regular vissprites.
 */
void Rend_DecorProject(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RENDER_DECOR_H */
