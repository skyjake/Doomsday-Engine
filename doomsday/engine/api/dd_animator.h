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

#ifndef LIBDENG_ANIMATOR_H
#define LIBDENG_ANIMATOR_H

/**
 * Provides a way to gradually move between target values.
 *
 * @ingroup types
 */
typedef struct {
    float           value;
    float           target;
    int             steps;
} animator_t;

void            Animator_Init(animator_t* v, float val);
void            Animator_Set(animator_t* v, float val, int steps);
void            Animator_Think(animator_t* v);

/**
 * 2D vector animator.
 *
 * @ingroup types
 */
typedef animator_t animatorvector2_t[2];

void            AnimatorVector2_Init(animatorvector2_t v, float x, float y);
void            AnimatorVector2_Set(animatorvector2_t v, float x, float y, int steps);
void            AnimatorVector2_Think(animatorvector2_t v);

/**
 * 3D vector animator.
 *
 * @ingroup types.
 */
typedef animator_t animatorvector3_t[3];

void            AnimatorVector3_Init(animatorvector3_t v, float x, float y, float z);
void            AnimatorVector3_Set(animatorvector3_t v, float x, float y, float z, int steps);
void            AnimatorVector3_Think(animatorvector3_t v);

/**
 * 4D vector animator.
 *
 * @ingroup types.
 */
typedef animator_t animatorvector4_t[4];

void            AnimatorVector4_Init(animatorvector4_t v, float x, float y, float z, float w);
void            AnimatorVector4_Set(animatorvector4_t v, float x, float y, float z, float w, int steps);
void            AnimatorVector4_Think(animatorvector4_t v);

#endif /* LIBDENG_ANIMATOR_H */
