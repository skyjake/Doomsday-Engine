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
 * r_sky.h: Sky Management
 */

#ifndef __DOOMSDAY_REFRESH_SKY_H__
#define __DOOMSDAY_REFRESH_SKY_H__

#include "r_model.h"

typedef struct skymodel_s {
	ded_skymodel_t *def;
	modeldef_t *model;	
	int frame;
	int timer;
	int maxTimer;
	float yaw;
} skymodel_t;

extern skymodel_t	skyModels[NUM_SKY_MODELS];
extern boolean		skyModelsInited;
extern boolean		alwaysDrawSphere;

void R_SetupSkyModels(ded_mapinfo_t *info);
void R_PrecacheSky(void);
void R_SkyTicker(void);

#endif 
