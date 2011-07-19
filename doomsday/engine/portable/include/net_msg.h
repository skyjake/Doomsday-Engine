/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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

void            Msg_Begin(int type);
void            Msg_WriteByte(byte b);
void            Msg_WriteShort(short w);
void            Msg_WritePackedShort(short w);
void            Msg_WriteLong(int l);
void            Msg_WritePackedLong(unsigned int l);
void            Msg_Write(const void *src, size_t len);
byte            Msg_ReadByte(void);
short           Msg_ReadShort(void);
short           Msg_ReadPackedShort(void);
int             Msg_ReadLong(void);
unsigned int    Msg_ReadPackedLong(void);
void            Msg_Read(void *dest, size_t len);
size_t          Msg_Offset(void);
void            Msg_SetOffset(size_t offset);
size_t          Msg_MemoryLeft(void);
boolean         Msg_End(void);

#endif
