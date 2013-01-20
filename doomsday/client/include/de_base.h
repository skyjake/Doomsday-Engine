/** @file
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * Engine Core.
 */

#ifndef LIBDENG_BASE_H
#define LIBDENG_BASE_H

// System headers needed everywhere.
#include <assert.h>

#include "de_platform.h"

#include <de/memory.h>
#include <de/memoryzone.h>
#include <de/garbage.h>
#include <de/reader.h>
#include <de/writer.h>
#include <de/c_wrapper.h>

#include "dd_def.h"
#include "dd_share.h"
#include "dd_games.h"
#include "api_gameexport.h"
#include "api_plugin.h"
#include "dd_main.h"
#include "dd_loop.h"
#include "dd_help.h"
#include "library.h"
#include "busymode.h"
#include "ui/dd_input.h"
#include "ui/nativeui.h"
#include "ui/zonedebug.h"
#include "uri.hh"

#ifdef __SERVER__
// Many subsystems do not exist on the server. This is a temporary measure
// to allow compilation without pulling everything apart just yet.
#  include "server_dummies.h"
#endif

#endif /* LIBDENG_BASE_H */
