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
 * dd_types.h: Type Definitions
 */

#ifndef __DOOMSDAY_TYPES_H__
#define __DOOMSDAY_TYPES_H__

struct directory_s;

typedef int					spritenum_t;
typedef unsigned int		uint;
typedef unsigned short		ushort;
typedef unsigned int		size_t;
typedef unsigned int		id_t;
typedef unsigned short		nodeindex_t;
typedef unsigned short		thid_t;
typedef unsigned char		byte;
typedef struct directory_s	directory_t;
typedef char				filename_t[256];

#ifdef __cplusplus
#	define boolean			int
#else // Plain C.
#	ifndef __BYTEBOOL__
#		define __BYTEBOOL__
#	endif
	typedef enum ddboolean_e { false, true } ddboolean_t;
#	define boolean			ddboolean_t
#endif

#define BAMS_BITS	16

#if BAMS_BITS == 32
	typedef unsigned long	binangle_t;
#elif BAMS_BITS == 16
	typedef unsigned short	binangle_t;
#else
	typedef unsigned char	binangle_t;
#endif

#define DDMAXCHAR	((char)0x7f)
#define DDMAXSHORT	((short)0x7fff)
#define DDMAXINT	((int)0x7fffffff)	// max pos 32-bit int
#define DDMAXLONG	((long)0x7fffffff)

#define DDMINCHAR	((char)0x80)
#define DDMINSHORT	((short)0x8000)
#define DDMININT 	((int)0x80000000)	// max negative 32-bit integer
#define DDMINLONG	((long)0x80000000)

#endif 