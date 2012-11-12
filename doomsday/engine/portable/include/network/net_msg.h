/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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
 * net_msg.h: Network Messaging
 */

#ifndef __DOOMSDAY_NETMESSAGE_H__
#define __DOOMSDAY_NETMESSAGE_H__

#include <de/reader.h>
#include <de/writer.h>

extern Writer* msgWriter;
extern Reader* msgReader;

/**
 * Begin writing a message to netBuffer. If a message is currently being
 * read, the reading will be ended.
 */
void Msg_Begin(int type);
void Msg_End(void);

boolean Msg_BeingWritten(void);

/**
 * Begin reading a message from netBuffer. If a message is currently being
 * written, the writing will be ended.
 */
void Msg_BeginRead(void);
void Msg_EndRead(void);

#endif
