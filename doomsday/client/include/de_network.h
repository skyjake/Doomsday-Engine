/** @file
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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
 * Network Subsystem.
 */

#ifndef LIBDENG_NETWORK
#define LIBDENG_NETWORK

#include "network/net_main.h"
#include "network/net_event.h"
#include "network/net_msg.h"
#include "network/net_buf.h"
#include "network/protocol.h"
#ifdef __CLIENT__
#  include "network/net_demo.h"
#  include "api_client.h"
#endif

#ifdef __SERVER__
#  include "server/sv_def.h"
#  include "server/sv_frame.h"
#  include "server/sv_pool.h"
#  include "server/sv_sound.h"
#  include "server/sv_missile.h"
#  include "server/sv_infine.h"
#endif

#ifdef __CLIENT__
#  include "client/cl_def.h"
#  include "client/cl_player.h"
#  include "client/cl_mobj.h"
#  include "client/cl_frame.h"
#  include "client/cl_sound.h"
#  include "client/cl_world.h"
#  include "client/cl_infine.h"
#endif

#endif /* LIBDENG_NETWORK */
