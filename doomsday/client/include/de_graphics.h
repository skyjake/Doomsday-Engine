/** @file de_graphics.h
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * Graphics Subsystem
 */

#ifndef LIBDENG_GRAPHICS
#define LIBDENG_GRAPHICS

#ifdef __CLIENT__
#  include "gl/gl_main.h"
#  include "gl/gl_draw.h"
#  include "gl/texturecontent.h"
#  include "gl/gl_tex.h"
#  include "gl/gl_model.h"
#  include "gl/gl_defer.h"
#  include "gl/gl_texmanager.h"
#endif

#include "resource/pcx.h"
#include "resource/tga.h"
#include "resource/hq2x.h"

#endif /* LIBDENG_GRAPHICS */
