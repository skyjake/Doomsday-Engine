/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * dd_version.h: Version Information
 */

#ifndef __DOOMSDAY_VERSION_INFO_H__
#define __DOOMSDAY_VERSION_INFO_H__

/*	
 * Version number rules: (major).(minor).(revision)-(release)
 *
 * Major version will be 1 for now (few things short of a complete 
 * rewrite will increase the major version).
 *
 * Minor version increases with important feature releases.
 * NOTE: No extra zeros. Numbering goes from 1 to 9 and continues from
 * 10 like 'normal' numbers.
 *
 * Revision number increases with each small (maintenance) release.
 */

// Version constants. The Game module can use DOOMSDAY_VERSION to verify 
// that the engine is new enough. 
#define DOOMSDAY_VERSION		10800
#define DOOMSDAY_RELEASE_NAME	"0-rc1"
#define DOOMSDAY_VERSION_TEXT	"1.8."DOOMSDAY_RELEASE_NAME

#endif 
