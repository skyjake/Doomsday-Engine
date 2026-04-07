/** @file
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * net_msg.h: Network Messaging
 */

#ifndef __DOOMSDAY_NETMESSAGE_H__
#define __DOOMSDAY_NETMESSAGE_H__

#include <de/legacy/reader.h>
#include <de/legacy/writer.h>

DE_EXTERN_C Writer1 *msgWriter;
DE_EXTERN_C Reader1 *msgReader;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Begin writing a message to netBuffer. If a message is currently being
 * read, the reading will be ended.
 */
void Msg_Begin(int type);
void Msg_End(void);

dd_bool Msg_BeingWritten(void);

/**
 * Begin reading a message from netBuffer. If a message is currently being
 * written, the writing will be ended.
 */
void Msg_BeginRead(void);
void Msg_EndRead(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
