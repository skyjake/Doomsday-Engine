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
 * rend_particle.h: Particle Effects
 */

#ifndef __DGL_PARTICLES_H__
#define __DGL_PARTICLES_H__

extern int		r_use_particles, r_max_particles;
extern float	r_particle_spawn_rate;
extern int		rend_particle_nearlimit;
extern float	rend_particle_diffuse;

void PG_InitTextures(void);
void PG_ShutdownTextures(void);
void PG_InitForLevel(void);
void PG_InitForNewFrame(void);
void PG_SectorIsVisible(sector_t *sector);
void PG_Render(void);

#endif
