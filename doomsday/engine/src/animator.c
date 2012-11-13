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

#include <assert.h>
#include "dd_share.h"

void Animator_Init(animator_t* v, float val)
{
    assert(v);
    v->target = v->value = val;
    v->steps = 0;
}

void Animator_Set(animator_t* v, float val, int steps)
{
    assert(v);
    v->target = val;
    v->steps = steps;
    if(!v->steps)
        v->value = v->target;
}

void Animator_Think(animator_t* v)
{
    assert(v);
    if(v->steps <= 0)
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
    assert(v);
    Animator_Init(&v[0], x);
    Animator_Init(&v[1], y);
}

void AnimatorVector2_Set(animatorvector2_t v, float x, float y, int steps)
{
    assert(v);
    Animator_Set(&v[0], x, steps);
    Animator_Set(&v[1], y, steps);
}

void AnimatorVector2_Think(animatorvector2_t v)
{
    assert(v);
    Animator_Think(&v[0]);
    Animator_Think(&v[1]);
}

void AnimatorVector3_Init(animatorvector3_t v, float x, float y, float z)
{
    assert(v);
    AnimatorVector2_Init(v, x, y);
    Animator_Init(&v[2], z);
}

void AnimatorVector3_Set(animatorvector3_t v, float x, float y, float z, int steps)
{
    assert(v);
    AnimatorVector2_Set(v, x, y, steps);
    Animator_Set(&v[2], z, steps);
}

void AnimatorVector3_Think(animatorvector3_t v)
{
    assert(v);
    AnimatorVector2_Think(v);
    Animator_Think(&v[2]);
}

void AnimatorVector4_Init(animatorvector4_t v, float x, float y, float z, float w)
{
    assert(v);
    AnimatorVector3_Init(v, x, y, z);
    Animator_Init(&v[3], w);
}

void AnimatorVector4_Set(animatorvector4_t v, float x, float y, float z, float w, int steps)
{
    assert(v);
    AnimatorVector3_Set(v, x, y, z, steps);
    Animator_Set(&v[3], w, steps);
}

void AnimatorVector4_Think(animatorvector4_t v)
{
    assert(v);
    AnimatorVector3_Think(v);
    Animator_Think(&v[3]);
}
