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
 * rend_bias.c: Light/Shadow Bias
 *
 * Calculating macro-scale lighting on the fly.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_render.h"
#include "de_misc.h"
#include "p_sight.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float viewfrontvec[3];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int useBias = false;
static int useSightCheck = true;
static int moveBiasLight = true;

// CODE --------------------------------------------------------------------

/*
 * Register console variables for Shadow Bias.
 */
void SB_Register(void)
{
    C_VAR_INT("rend-dev-bias", &useBias, CVF_NO_ARCHIVE, 0, 1,
              "1=Enable the experimental shadow bias test setup.");

    C_VAR_INT("rend-dev-bias-sight", &useSightCheck, CVF_NO_ARCHIVE, 0, 1,
              "1=Enable the use of line-of-sight checking with shadow bias.");

    C_VAR_INT("rend-dev-bias-move", &moveBiasLight, CVF_NO_ARCHIVE, 0, 1,
              "1=Keep moving the bias light.");
}

/*
 * Poly can be a either a wall or a plane (ceiling or a floor).
 */
void SB_RendPoly(rendpoly_t *poly, boolean isFloor)
{
    float pos[3];
    float normal[3];
    int i;

    if(!useBias)
        return;
    
    if(poly->numvertices == 2)
    {
        // It's a wall.
        normal[VY] = poly->vertices[0].pos[VX] -
            poly->vertices[1].pos[VX];
        normal[VX] = poly->vertices[1].pos[VY] -
            poly->vertices[0].pos[VY];
        normal[VZ] = 0;

        M_Normalize(normal);

        for(i = 0; i < 4; ++i)
        {
            pos[VX] = poly->vertices[i % 2].pos[VX];
            pos[VY] = poly->vertices[i % 2].pos[VY];
            pos[VZ] = (i >= 2 ? poly->bottom : poly->top);
            
            SB_Point((i >= 2 ? &poly->bottomcolor[i - 2] :
                      &poly->vertices[i].color), pos, normal);
        }
    }
    else
    {
        // It's a plane.
        normal[VX] = normal[VY] = 0;
        normal[VZ] = (isFloor? 1 : -1);

        for(i = 0; i < poly->numvertices; ++i)
        {
            pos[VX] = poly->vertices[i].pos[VX];
            pos[VY] = poly->vertices[i].pos[VY];
            pos[VZ] = poly->top;
            
            SB_Point(&poly->vertices[i].color, pos, normal);
        }
    }        
}

/*
 * Applies shadow bias to the given point.
 */
void SB_Point(gl_rgba_t *light, float *point, float *normal)
{
    static float source[3]; /* = { vx + viewfrontvec[VX]*300,
                               vz + viewfrontvec[VZ]*300,
                               vy + viewfrontvec[VY]*300 };*/
    float dot;
    float delta[3];
    float distance;
    float level;
    int i;

    if(moveBiasLight)
    {
        source[0] = vx + viewfrontvec[VX]*300;
        source[1] = vz + viewfrontvec[VZ]*300;
        source[2] = vy + viewfrontvec[VY]*300;
    }
    
    for(i = 0; i < 3; ++i)
    {
        delta[i] = source[i] - point[i];
        point[i] += delta[i] / 100;
    }
    
    if(useSightCheck && !P_CheckLineSight(source, point))
    {
        level = 0;
    }
    else
    {
        distance = M_Normalize(delta);
        dot = M_DotProduct(delta, normal);
        
        // The surface faces away from the light.
        if(dot <= 0) dot = 0;
        
        level = dot * 500 / distance;
    }

    if(level > 1) level = 1;
        
    for(i = 0; i < 3; ++i)
    {
        light->rgba[i] = 255 * level;
    }
}
