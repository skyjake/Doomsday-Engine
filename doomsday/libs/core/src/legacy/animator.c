/**
 * @file animator.c
 * Moves a value gradually from a start value to a target value.
 *
 * The value transition is carried out in a fixed number of steps.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "de/legacy/animator.h"

void Animator_Init(animator_t *v, float val)
{
    DE_ASSERT(v);
    v->target = v->value = val;
    v->steps = 0;
}

void Animator_Set(animator_t *v, float val, int steps)
{
    DE_ASSERT(v);
    v->target = val;
    v->steps = steps;
    if (!v->steps)
        v->value = v->target;
}

void Animator_Think(animator_t *v)
{
    DE_ASSERT(v);
    if (v->steps <= 0)
    {
        v->steps = 0;
        v->value = v->target;
        return;
    }

    v->value += (v->target - v->value) / v->steps;
    v->steps--;
}

void AnimatorVector2_Init(animatorvector2_t v, float x, float y)
{
    DE_ASSERT(v);
    Animator_Init(&v[0], x);
    Animator_Init(&v[1], y);
}

void AnimatorVector2_Set(animatorvector2_t v, float x, float y, int steps)
{
    DE_ASSERT(v);
    Animator_Set(&v[0], x, steps);
    Animator_Set(&v[1], y, steps);
}

void AnimatorVector2_Think(animatorvector2_t v)
{
    DE_ASSERT(v);
    Animator_Think(&v[0]);
    Animator_Think(&v[1]);
}

void AnimatorVector3_Init(animatorvector3_t v, float x, float y, float z)
{
    DE_ASSERT(v);
    AnimatorVector2_Init(v, x, y);
    Animator_Init(&v[2], z);
}

void AnimatorVector3_Set(animatorvector3_t v, float x, float y, float z, int steps)
{
    DE_ASSERT(v);
    AnimatorVector2_Set(v, x, y, steps);
    Animator_Set(&v[2], z, steps);
}

void AnimatorVector3_Think(animatorvector3_t v)
{
    DE_ASSERT(v);
    AnimatorVector2_Think(v);
    Animator_Think(&v[2]);
}

void AnimatorVector4_Init(animatorvector4_t v, float x, float y, float z, float w)
{
    DE_ASSERT(v);
    AnimatorVector3_Init(v, x, y, z);
    Animator_Init(&v[3], w);
}

void AnimatorVector4_Set(animatorvector4_t v, float x, float y, float z, float w, int steps)
{
    DE_ASSERT(v);
    AnimatorVector3_Set(v, x, y, z, steps);
    Animator_Set(&v[3], w, steps);
}

void AnimatorVector4_Think(animatorvector4_t v)
{
    DE_ASSERT(v);
    AnimatorVector3_Think(v);
    Animator_Think(&v[3]);
}
