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
 * net_msg.c: Network Messaging
 *
 * Buffer overflow checks *ARE NOT* made.
 * The caller must know what it's doing.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_network.h"
#ifdef _DEBUG
# include "de_console.h"
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Msg_Begin(int type)
{
	netbuffer.cursor = netbuffer.msg.data;
	netbuffer.length = 0;
	netbuffer.msg.type = type;
}

void Msg_WriteByte(byte b)
{
	*netbuffer.cursor++ = b;
}

void Msg_WriteShort(short w)
{
	*(short*) netbuffer.cursor = w;
	netbuffer.cursor += 2;
}

//==========================================================================
// Msg_WritePackedShort
//	Only 15 bits can be used for the number because the high bit of the 
//	lower byte is used to determine whether the upper byte follows or not.
//==========================================================================
void Msg_WritePackedShort(short w)
{
	if(w < 0x80) // Can the number be represented with 7 bits?
		Msg_WriteByte(w);
	else
	{
		Msg_WriteByte(0x80 | w);
		Msg_WriteByte(w >> 7); // Highest bit is lost.
	}
}

void Msg_WriteLong(int l)
{
	*(int*) netbuffer.cursor = l;
	netbuffer.cursor += 4;
}

void Msg_Write(const void *src, int len)
{
	memcpy(netbuffer.cursor, src, len);
	netbuffer.cursor += len;
}

byte Msg_ReadByte(void)
{
#ifdef _DEBUG
	if(Msg_Offset() >= netbuffer.length) 
		Con_Error("Packet read overflow!\n");
#endif
	return *netbuffer.cursor++;
}

short Msg_ReadShort(void)
{
#ifdef _DEBUG
	if(Msg_Offset() >= netbuffer.length) 
		Con_Error("Packet read overflow!\n");
#endif
	netbuffer.cursor += 2;
	return *(short*) (netbuffer.cursor-2);
}

//==========================================================================
// Msg_ReadPackedShort
//	Only 15 bits can be used for the number because the high bit of the 
//	lower byte is used to determine whether the upper byte follows or not.
//==========================================================================
short Msg_ReadPackedShort(void)
{
	short pack = *netbuffer.cursor++;
	if(pack & 0x80) 
	{
		pack &= ~0x80;
		pack |= (*netbuffer.cursor++) << 7;
	}
	return pack;
}

int Msg_ReadLong(void)
{
#ifdef _DEBUG
	if(Msg_Offset() >= netbuffer.length) 
		Con_Error("Packet read overflow!\n");
#endif
	netbuffer.cursor += 4;
	return *(int*) (netbuffer.cursor-4);
}

void Msg_Read(void *dest, int len)
{
#ifdef _DEBUG
	if(Msg_Offset() >= netbuffer.length) 
		Con_Error("Packet read overflow!\n");
#endif
	memcpy(dest, netbuffer.cursor, len);
	netbuffer.cursor += len;
}

int Msg_Offset(void)
{
	return netbuffer.cursor - netbuffer.msg.data;
}

void Msg_SetOffset(int offset)
{
	netbuffer.cursor = netbuffer.msg.data + offset;
}

int Msg_MemoryLeft(void)
{
	return NETBUFFER_MAXDATA - (netbuffer.cursor - netbuffer.msg.data);
}

boolean Msg_End(void)
{
	return (netbuffer.cursor-netbuffer.msg.data >= netbuffer.length);
}