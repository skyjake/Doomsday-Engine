/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * net_msg.h: Network Messaging
 */

#ifndef __DOOMSDAY_NETMESSAGE_H__
#define __DOOMSDAY_NETMESSAGE_H__

void	Msg_Begin(int type);
void	Msg_WriteByte(byte b);
void	Msg_WriteShort(short w);
void	Msg_WritePackedShort(short w);
void	Msg_WriteLong(int l);
void	Msg_Write(const void *src, int len);
byte	Msg_ReadByte(void);
short	Msg_ReadShort(void);
short	Msg_ReadPackedShort(void);
int		Msg_ReadLong(void);
void	Msg_Read(void *dest, int len);
int		Msg_Offset(void);
void	Msg_SetOffset(int offset);
int		Msg_MemoryLeft(void);
boolean	Msg_End(void);

#endif
