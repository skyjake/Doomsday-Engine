/* DE1: $Id$
 * Copyright (C) 2006 Jaakko Keränen <jaakko.keranen@iki.fi>
 *                    Daniel Swanson <danij@users.sourceforge.net>
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
 * p_dmu.h: Map Update API
 *
 * Engine-internal header for DMU.
 */

#ifndef __DOOMSDAY_MAP_UPDATE_H__
#define __DOOMSDAY_MAP_UPDATE_H__

void        P_InitMapUpdate(void);
void       *P_AllocDummy(int type, void* extraData);
void        P_FreeDummy(void* dummy);
int         P_DummyType(void* dummy);
boolean     P_IsDummy(void* dummy);
void       *P_DummyExtraData(void* dummy);

int         P_ToIndex(const void* ptr);

const char *DMU_Str(int prop);

#endif
