/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#include "server/sv_def.h"
#include "server/sv_frame.h"
#include "server/sv_pool.h"
#include "server/sv_sound.h"
#include "server/sv_missile.h"
#include "server/sv_infine.h"

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
