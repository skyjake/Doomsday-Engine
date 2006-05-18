/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * x_hair.c: Crosshairs. Drawing and config
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include "Common/x_hair.h"

#if __JDOOM__
#  include "doomdef.h"
#  include "jDoom/d_config.h"
#elif __JHERETIC__
#  include "jHeretic/Doomdef.h"
#  include "h_config.h"
#elif __JHEXEN__
#  include "h2def.h"
#  include "x_config.h"
#elif __JSTRIFE__
#  include "h2def.h"
#  include "jStrife/d_config.h"
#endif

// MACROS ------------------------------------------------------------------

#define MAX_XLINES  16

// TYPES -------------------------------------------------------------------

typedef struct {
    int     x, y;               // In window coordinates (*not* 320x200!).
} crosspoint_t;

typedef struct {
    crosspoint_t a, b;
} crossline_t;

typedef struct {
    int     numLines;
    crossline_t Lines[MAX_XLINES];
} cross_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdCrosshair);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#define XL(x1,y1,x2,y2) {{x1,y1},{x2,y2}}

cross_t crosshairs[NUM_XHAIRS] = {
    // + (open center)
    {4, {XL(-5, 0, -2, 0), XL(0, -5, 0, -2), XL(5, 0, 2, 0), XL(0, 5, 0, 2)}},
    // > <
    {4,
     {XL(-7, -5, -2, 0), XL(-7, 5, -2, 0), XL(7, -5, 2, 0), XL(7, 5, 2, 0)}},
    // square
    {4,
     {XL(-3, -3, -3, 3), XL(-3, 3, 3, 3), XL(3, 3, 3, -3), XL(3, -3, -3, -3)}},
    // square (open center)
    {8,
     {XL(-4, -4, -4, -2), XL(-4, 2, -4, 4), XL(-4, 4, -2, 4), XL(2, 4, 4, 4),
      XL(4, 4, 4, 2), XL(4, -2, 4, -4), XL(4, -4, 2, -4), XL(-2, -4, -4, -4)}},
    // diamond
    {4, {XL(0, -3, 3, 0), XL(3, 0, 0, 3), XL(0, 3, -3, 0), XL(-3, 0, 0, -3)}},
    // ^
    {2, {XL(-4, -4, 0, 0), XL(0, 0, 4, -4)}}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CVARs for the crosshair
cvar_t xhairCVars[] =
{
    {"view-cross-type", CVF_NO_MAX | CVF_PROTECTED, CVT_INT, &cfg.xhair, 0, 0,
        "The current crosshair."},

    {"view-cross-size", CVF_NO_MAX, CVT_INT, &cfg.xhairSize, 0, 0,
        "Crosshair size: 1=Normal."},

    {"view-cross-r", 0, CVT_BYTE, &cfg.xhairColor[0], 0, 255,
        "Crosshair color red component."},
    {"view-cross-g", 0, CVT_BYTE, &cfg.xhairColor[1], 0, 255,
        "Crosshair color green component."},
    {"view-cross-b", 0, CVT_BYTE, &cfg.xhairColor[2], 0, 255,
        "Crosshair color blue component."},
    {"view-cross-a", 0, CVT_BYTE, &cfg.xhairColor[3], 0, 255,
        "Crosshair color alpha component."},
    {NULL}
};

// Console commands for the crosshair
ccmd_t  xhairCCmds[] = {
    {"crosshair",      CCmdCrosshair,  "Crosshair setup.", 0 },
    {NULL}
};

// CODE --------------------------------------------------------------------

/*
 * Register CVARs and CCmds for the crosshair.
 */
void X_Register(void)
{
    int     i;

    for(i = 0; xhairCVars[i].name; i++)
        Con_AddVariable(xhairCVars + i);
    for(i = 0; xhairCCmds[i].name; i++)
        Con_AddCommand(xhairCCmds + i);
}

void X_Drawer(void)
{
    // Where to draw the xhair?
    int     centerX = Get(DD_SCREEN_WIDTH) / 2;
    int     centerY =
        (Get(DD_VIEWWINDOW_Y) + 2) * Get(DD_SCREEN_HEIGHT) / 200.0f +
        Get(DD_VIEWWINDOW_SCREEN_HEIGHT) / 2.0f;
    int     i;
    float   fact = (cfg.xhairSize + 1) / 2.0f;
    byte    xcolor[4] = { cfg.xhairColor[0], cfg.xhairColor[1],
        cfg.xhairColor[2], cfg.xhairColor[3]
    };
    cross_t *cross;

    // Is there a crosshair to draw?
    if(cfg.xhair < 1 || cfg.xhair > NUM_XHAIRS)
        return;

    gl.Disable(DGL_TEXTURING);

    // Push the current matrices.
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();

    gl.Ortho(0, 0, Get(DD_SCREEN_WIDTH), Get(DD_SCREEN_HEIGHT), -1, 1);

    cross = crosshairs + cfg.xhair - 1;

    gl.Color4ubv(xcolor);
    gl.Begin(DGL_LINES);
    for(i = 0; i < cross->numLines; i++)
    {
        crossline_t *xline = cross->Lines + i;

        gl.Vertex2f(fact * xline->a.x + centerX, fact * xline->a.y + centerY);
        gl.Vertex2f(fact * xline->b.x + centerX, fact * xline->b.y + centerY);
    }
    gl.End();

    gl.Enable(DGL_TEXTURING);

    // Pop back the old matrices.
    gl.PopMatrix();
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();
}

/*
 * Console command for configuring the crosshair.
 */
DEFCC(CCmdCrosshair)
{
    if(argc == 1)
    {
        Con_Printf("Usage:\n  crosshair (num)\n");
        Con_Printf("  crosshair size (size)\n");
        Con_Printf("  crosshair color (r) (g) (b)\n");
        Con_Printf("  crosshair color (r) (g) (b) (a)\n");
        Con_Printf("Num: 0=no crosshair, 1-%d: use crosshair 1...%d\n",
                   NUM_XHAIRS, NUM_XHAIRS);
        Con_Printf("Size: 1=normal\n");
        Con_Printf("R, G, B, A: 0-255\n");
        Con_Printf("Current values: xhair=%d, size=%d, color=(%d,%d,%d,%d)\n",
                   cfg.xhair, cfg.xhairSize, cfg.xhairColor[0],
                   cfg.xhairColor[1], cfg.xhairColor[2], cfg.xhairColor[3]);
        return true;
    }
    else if(argc == 2)          // Choose.
    {
        cfg.xhair = strtol(argv[1], NULL, 0);
        if(cfg.xhair > NUM_XHAIRS || cfg.xhair < 0)
        {
            cfg.xhair = 0;
            return false;
        }
        Con_Printf("Crosshair %d selected.\n", cfg.xhair);
    }
    else if(argc == 3)          // Size.
    {
        if(stricmp(argv[1], "size"))
            return false;
        cfg.xhairSize = strtol(argv[2], NULL, 0);
        if(cfg.xhairSize < 0)
            cfg.xhairSize = 0;
        Con_Printf("Crosshair size set to %d.\n", cfg.xhairSize);
    }
    else if(argc == 5 || argc == 6) // Color.
    {
        int     i, val;

        if(stricmp(argv[1], "color"))
            return false;
        for(i = 0; i < argc - 2; i++)
        {
            val = strtol(argv[2 + i], NULL, 0);
            // Clamp.
            if(val < 0)
                val = 0;
            if(val > 255)
                val = 255;
            cfg.xhairColor[i] = val;
        }
        Con_Printf("Crosshair color set to (%d, %d, %d, %d).\n",
                   cfg.xhairColor[0], cfg.xhairColor[1], cfg.xhairColor[2],
                   cfg.xhairColor[3]);
    }
    else
        return false;
    // Success!
    return true;
}
