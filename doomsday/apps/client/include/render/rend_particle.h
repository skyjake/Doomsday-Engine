/** @file rend_particle.h  Particle effect rendering.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_CLIENT_RENDER_PARTICLE_H
#define DE_CLIENT_RENDER_PARTICLE_H

class Map;

// Maximum number of particle textures (not instances).
#define MAX_PTC_TEXTURES        300

// Maximum number of particle models (not instances).
#define MAX_PTC_MODELS          100

/// @ingroup render
enum ParticleType
{
    PTC_NONE,
    PTC_POINT,
    PTC_LINE,
    // New types can be added here.
    PTC_TEXTURE = 100,
    // ...followed by MAX_PTC_TEXTURES types.
    PTC_MODEL = 1000
};

DE_EXTERN_C de::dbyte useParticles;

void Rend_ParticleRegister();

/**
 * Loads all system-level textures (i.e., those available by default)
 * for a minimal resource set needed for particle rendering.
 */
void Rend_ParticleLoadSystemTextures();
void Rend_ParticleReleaseSystemTextures();

/**
 * Load any custom particle textures. They are loaded from the
 * highres texture directory and are named "ParticleNN.(tga|png|pcx)".
 * The first is "Particle00". (based on Leesz' textured particles mod)
 */
void Rend_ParticleLoadExtraTextures();
void Rend_ParticleReleaseExtraTextures();

/**
 * Render all the visible particle generators.
 * We must render all particles ordered back->front, or otherwise
 * particles from one generator will obscure particles from another.
 * This would be especially bad with smoke trails.
 */
void Rend_RenderParticles(Map &map);

#endif  // DE_CLIENT_RENDER_PARTICLE_H
