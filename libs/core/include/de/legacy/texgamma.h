/** @file texgamma.h  Texture color gamma mapping.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_TEXGAMMA_H
#define DE_TEXGAMMA_H

#include "../liblegacy.h"
#include <de/legacy/types.h>

#ifdef __cplusplus
extern "C" {
#endif

DE_PUBLIC void R_BuildTexGammaLut(float texGamma);

DE_PUBLIC byte R_TexGammaLut(byte colorValue);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_TEXGAMMA_H
