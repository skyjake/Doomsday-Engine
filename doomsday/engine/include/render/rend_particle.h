/** @file rend_particle.h Particle Effect Rendering. @ingroup render
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_RENDER_PARTICLE_H
#define LIBDENG_RENDER_PARTICLE_H

#include "map/p_particle.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * Prepare for rendering a new view of the world.
 */
void Rend_ParticleInitForNewFrame(void);

/**
 * The given sector is visible. All PGs in it should be rendered.
 * Scans PG links.
 */
void Rend_ParticleMarkInSectorVisible(Sector* sector);

/**
 * Render all the visible particle generators.
 * We must render all particles ordered back->front, or otherwise
 * particles from one generator will obscure particles from another.
 * This would be especially bad with smoke trails.
 */
void Rend_RenderParticles(void);

/**
 * Debugging aid; Draw all active generators.
 */
void Rend_RenderGenerators(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_RENDER_PARTICLE_H
