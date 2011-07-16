/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * rend_particle.h: Particle effect renderer.
 */

#ifndef LIBDENG_REND_PARTICLE_H
#define LIBDENG_REND_PARTICLE_H

extern byte useParticles;
extern int maxParticles;
extern float particleSpawnRate;

void Rend_ParticleRegister(void);

/**
 * Loads all system-level textures (i.e., those available by default)
 * for a minimal resource set needed for particle rendering.
 */
void Rend_ParticleLoadSystemTextures(void);
void Rend_ParticleReleaseSystemTextures(void);

/**
 * Load any custom particle textures. They are loaded from the
 * highres texture directory and are named "ParticleNN.(tga|png|pcx)".
 * The first is "Particle00". (based on Leesz' textured particles mod)
 */
void Rend_ParticleLoadExtraTextures(void);
void Rend_ParticleReleaseExtraTextures(void);

void Rend_ParticleInitForNewFrame(void);
void Rend_ParticleMarkInSectorVisible(sector_t* sector);

void Rend_RenderParticles(void);
// Debugging aid:
void Rend_RenderGenerators(void);
#endif /* LIBDENG_REND_PARTICLE_H */
