/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * con_start.c: Console Startup Screen
 *
 * Draws the GL startup screen & messages.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int bufferLines;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean startupScreen = false;
int     startupLogo;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char *titleText = "";
static char secondaryTitleText[256];
static char statusText[256];
static int fontHgt = 8;         // Height of the font.
static DGLuint bgflat;
char   *bitmap = NULL;

// CODE --------------------------------------------------------------------

/*
 * The startup screen mode is used during engine startup.  In startup
 * mode, the whole screen is used for console output.
 */
void Con_StartupInit(void)
{
    static boolean firstTime = true;

    if(novideo)
        return;

    GL_InitVarFont();
    fontHgt = FR_SingleLineHeight("Doomsday!");

    startupScreen = true;

    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);

    if(firstTime)
    {
        titleText = "Doomsday " DOOMSDAY_VERSION_TEXT " Startup";
        firstTime = false;
        bgflat = 0;
    }
    else
    {
        titleText = "Doomsday " DOOMSDAY_VERSION_TEXT;
    }

    // Load graphics.
    startupLogo = GL_LoadGraphics("Background", LGM_GRAYSCALE);
}

void Con_SetBgFlat(int lump)
{
    bgflat = GL_BindTexFlat(R_GetFlat(lump));
}

void Con_StartupDone(void)
{
    if(isDedicated)
        return;
    titleText = "Doomsday " DOOMSDAY_VERSION_TEXT;
    startupScreen = false;
    gl.DeleteTextures(1, (DGLuint*) &startupLogo);
    startupLogo = 0;
    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();
    GL_ShutdownVarFont();

    // Update the secondary title and the game status.
    strncpy(secondaryTitleText, (char *) gx.GetVariable(DD_GAME_ID),
            sizeof(secondaryTitleText) - 1);
    strncpy(statusText, (char *) gx.GetVariable(DD_GAME_MODE),
            sizeof(statusText) - 1);
}

/*
 * Background with the "The Doomsday Engine" text superimposed.
 *
 * @param alpha  Alpha level to use when drawing the background.
 */
void Con_DrawStartupBackground(float alpha)
{
    float   mul = (startupLogo ? 1.5f : 1.0f);
    ui_color_t *dark = UI_COL(UIC_BG_DARK), *light = UI_COL(UIC_BG_LIGHT);

    // Background gradient picture.
    gl.Bind(startupLogo);
    if(alpha < 1.0)
    {
        gl.Enable(DGL_BLENDING);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        gl.Disable(DGL_BLENDING);
    }
    gl.Begin(DGL_QUADS);
    // Top color.
    gl.Color4f(dark->red * mul, dark->green * mul, dark->blue * mul, alpha);
    gl.TexCoord2f(0, 0);
    gl.Vertex2f(0, 0);
    gl.TexCoord2f(1, 0);
    gl.Vertex2f(screenWidth, 0);
    // Bottom color.
    gl.Color4f(light->red * mul, light->green * mul, light->blue * mul, alpha);
    gl.TexCoord2f(1, 1);
    gl.Vertex2f(screenWidth, screenHeight);
    gl.TexCoord2f(0, 1);
    gl.Vertex2f(0, screenHeight);
    gl.End();
    gl.Enable(DGL_BLENDING);
}

/*
 * Draws the title bar of the console.
 *
 * @return  Title bar height.
 */
int Con_DrawTitle(float alpha)
{
    int width = 0;
    int height = UI_TITLE_HGT;

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
    gl.LoadIdentity();

    FR_SetFont(glFontVariable[GLFS_BOLD]);
    height = FR_TextHeight("W") + UI_BORDER;
    UI_DrawTitleEx(titleText, height, alpha);
    if(secondaryTitleText[0])
    {
        width = FR_TextWidth(titleText) + FR_TextWidth("  ");
        FR_SetFont(glFontVariable[GLFS_LIGHT]);
        UI_TextOutEx(secondaryTitleText, UI_BORDER + width, height / 2,
                     false, true, UI_COL(UIC_TEXT), .75f * alpha);
    }
    if(statusText[0])
    {
        width = FR_TextWidth(statusText);
        FR_SetFont(glFontVariable[GLFS_LIGHT]);
        UI_TextOutEx(statusText, screenWidth - UI_BORDER - width, height / 2,
                     false, true, UI_COL(UIC_TEXT), .75f * alpha);
    }

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();

    FR_SetFont(glFontFixed);
    return height;
}

/*
 * Draw the background and the current console output.
 */
void Con_DrawStartupScreen(int show)
{
    int     i, vislines, y, x, st;
    int     topy;

    // Print the messages in the console.
    if(!startupScreen || ui_active)
        return;

    //gl.MatrixMode(DGL_PROJECTION);
    //gl.LoadIdentity();
    //gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);

    Con_DrawStartupBackground(1.0);

    // Draw the title.
    topy = Con_DrawTitle(1.0);

    topy += UI_BORDER;
    vislines = (screenHeight - topy + fontHgt / 2) / fontHgt;
    y = topy;

    st = bufferLines - vislines;
    // Show the last line, too, if there's something.
    if(Con_GetBufferLine(bufferLines - 1)->len)
        st++;
    if(st < 0)
        st = 0;
    for(i = 0; i < vislines && st + i < bufferLines; i++)
    {
        cbline_t *line = Con_GetBufferLine(st + i);

        if(!line)
            break;
        if(line->flags & CBLF_RULER)
        {
            Con_DrawRuler(y, fontHgt, 1);
        }
        else
        {
            x = line->flags & CBLF_CENTER ? (screenWidth -
                                             FR_TextWidth(line->text)) / 2 : 3;
            //gl.Color3f(0, 0, 0);
            //FR_TextOut(line->text, x + 1, y + 1);
            gl.Color3f(1, 1, 1);
            FR_CustomShadowTextOut(line->text, x, y, 1, 1, 1);
        }
        y += fontHgt;
    }
    if(show)
    {
        // Update the progress bar, if one is active.
        Con_Progress(0, PBARF_NOBACKGROUND | PBARF_NOBLIT);
        gl.Show();
    }
}
