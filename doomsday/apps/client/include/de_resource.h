/** @file de_resource.h  Resource Subsystem.
 * @ingroup resource
 *
 * @author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef RESOURCE_SUBSYSTEM_H
#define RESOURCE_SUBSYSTEM_H

#include <doomsday/resource/resourceclass.h>

#include "resource/abstractfont.h"
#include "resource/compositetexture.h"
#include "resource/rawtexture.h"

#ifdef __CLIENT__
#  include "resource/materialvariantspec.h"
#endif

#ifdef __cplusplus
#  include "resource/patch.h"
#  include "resource/texturemanifest.h"
#  include <doomsday/filesys/wad.h>
#  include <doomsday/filesys/zip.h>
#endif

#include "api_resource.h"

#endif  // RESOURCE_SUBSYSTEM_H
