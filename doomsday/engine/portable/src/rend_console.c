/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2007 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * rend_console.c: Console rendering.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_ui.h"

#include "math.h"

#ifdef TextOut
// Windows has its own TextOut.
#  undef TextOut
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(BackgroundTurn);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float   CcolYellow[3] = { 1, .85f, .3f };

float   ConsoleOpenY;           // Where the console bottom is when open.
int     consoleTurn;            // The rotation variable.
int     consoleLight = 14, consoleAlpha = 75;
byte    consoleShowFPS = false;
byte    consoleShadowText = true;
float   consoleMoveSpeed = .2f; // Speed of console opening/closing

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static float ConsoleY;          // Where the console bottom is currently?
static float ConsoleDestY;      // Where the console bottom should be?
static float ConsoleBlink;      // Cursor blink timer (35 Hz tics).
static boolean openingOrClosing = true;
static float fontFx, fontSy;    // Font x factor and y size.

static float funnyAng;

static char *consoleTitle = "Doomsday " DOOMSDAY_VERSION_TEXT;
static char secondaryTitleText[256];
static char statusText[256];

// CODE --------------------------------------------------------------------

void Rend_ConsoleRegister(void)
{
    C_CMD_FLAGS("bgturn", "i", BackgroundTurn, CMDF_NO_DEDICATED);

    C_VAR_INT("con-alpha", &consoleAlpha, 0, 0, 100);
    C_VAR_INT("con-light", &consoleLight, 0, 0, 100);
    C_VAR_BYTE("con-fps", &consoleShowFPS, 0, 0, 1);
    C_VAR_BYTE("con-text-shadow", &consoleShadowText, 0, 0, 1);
    C_VAR_FLOAT("con-move-speed", &consoleMoveSpeed, 0, 0, 1);
}

void Rend_ConsoleInit(void)
{
    ConsoleY = 0;
    ConsoleOpenY = 90;
    ConsoleDestY = 0;

    // Font size in VGA coordinates. (Everything is in VGA coords.)
    fontFx = 1;
    fontSy = 9;

    funnyAng = 0;
}

void Rend_ConsoleCursorResetBlink(void)
{
    ConsoleBlink = 0;
}

static void consoleSetColor(int fl, float alpha)
{
    float   r = 0, g = 0, b = 0;
    int     count = 0;

    // Calculate the average of the given colors.
    if(fl & CBLF_BLACK)
    {
        count++;
    }
    if(fl & CBLF_BLUE)
    {
        b += 1;
        count++;
    }
    if(fl & CBLF_GREEN)
    {
        g += 1;
        count++;
    }
    if(fl & CBLF_CYAN)
    {
        g += 1;
        b += 1;
        count++;
    }
    if(fl & CBLF_RED)
    {
        r += 1;
        count++;
    }
    if(fl & CBLF_MAGENTA)
    {
        r += 1;
        b += 1;
        count++;
    }
    if(fl & CBLF_YELLOW)
    {
        r += CcolYellow[0];
        g += CcolYellow[1];
        b += CcolYellow[2];
        count++;
    }
    if(fl & CBLF_WHITE)
    {
        r += 1;
        g += 1;
        b += 1;
        count++;
    }
    // Calculate the average.
    if(count)
    {
        r /= count;
        g /= count;
        b /= count;
    }
    if(fl & CBLF_LIGHT)
    {
        r += (1 - r) / 2;
        g += (1 - g) / 2;
        b += (1 - b) / 2;
    }
    gl.Color4f(r, g, b, alpha);
}

static void drawRuler2(int y, int lineHeight, float alpha, int scrWidth)
{
    int     xoff = 5;
    int     rh = 6;

    UI_GradientEx(xoff, y + (lineHeight - rh) / 2 + 1, scrWidth - 2 * xoff, rh,
                  rh / 2, UI_Color(UIC_SHADOW), UI_Color(UIC_BG_DARK), alpha / 3,
                  alpha);
    UI_DrawRectEx(xoff, y + (lineHeight - rh) / 2 + 1, scrWidth - 2 * xoff, rh,
                  rh / 2, false, UI_Color(UIC_TEXT), NULL, /*UI_Color(UIC_BG_DARK), UI_Color(UIC_BG_LIGHT), */
                  alpha, -1);
}

void Con_DrawRuler(int y, int lineHeight, float alpha)
{
    int         winWidth;

    if(!Sys_GetWindowDimensions(windowIDX, NULL, NULL, &winWidth, NULL))
    {
        Con_Message("Con_DrawRuler: Failed retrieving window dimensions.");
        return;
    }

    drawRuler2(y, lineHeight, alpha, winWidth);
}

/**
 * Initializes the Doomsday console user interface. This is called when
 * engine startup is complete.
 *
 * \todo Doesn't belong here.
 */
void Con_InitUI(void)
{
    // Update the secondary title and the game status.
    strncpy(secondaryTitleText, (char *) gx.GetVariable(DD_GAME_ID),
            sizeof(secondaryTitleText) - 1);
    strncpy(statusText, (char *) gx.GetVariable(DD_GAME_MODE),
            sizeof(statusText) - 1);
}

/**
 * Draw a 'side' text in the console. This is intended for extra
 * information about the current game mode.
 */
#if 0 // currently unused
static void drawSideText(const char *text, int line, float alpha)
{
    char    buf[300];
    float   gtosMulY = glScreenHeight / 200.0f;
    float   fontScaledY = Cfont.height * Cfont.sizeY;
    float   y = ConsoleY * gtosMulY - fontScaledY * (1 + line);
    int     ssw;

    if(y > -fontScaledY)
    {
        // The side text is a bit transparent.
        alpha *= .75f;

        // scaled screen width
        ssw = (int) (glScreenWidth / Cfont.sizeX);

        // Print the version.
        strncpy(buf, text, sizeof(buf));
        if(Cfont.Filter)
            Cfont.Filter(buf);

        /*
        if(consoleShadowText)
        {
            // Draw a shadow.
            gl.Color4f(0, 0, 0, alpha);
            Cfont.TextOut(buf, ssw - Cfont.Width(buf) - 2,
                          y / Cfont.sizeY + 1);
        }
        */
        gl.Color4f(CcolYellow[0], CcolYellow[1], CcolYellow[2], alpha);
        Cfont.TextOut(buf, ssw - Cfont.Width(buf) - 3, y / Cfont.sizeY);
    }
}
#endif

void Rend_ConsoleToggleFullscreen(void)
{
    ConsoleDestY = ConsoleOpenY = (ConsoleDestY == 200 ? 100 : 200);
}

void Rend_ConsoleOpen(int yes)
{
    openingOrClosing = true;
    if(yes)
    {
        ConsoleDestY = ConsoleOpenY;
        Rend_ConsoleCursorResetBlink();
    }
    else
    {
        ConsoleDestY = 0;
    }
}

void Rend_ConsoleMove(int numLines)
{
    if(numLines == 0)
        return;

    if(numLines < 0)
    {
        ConsoleOpenY -= fontSy * -numLines;
        if(ConsoleOpenY < fontSy)
            ConsoleOpenY = fontSy;
    }
    else
    {
        ConsoleOpenY += fontSy * numLines;
        if(ConsoleOpenY > 200)
            ConsoleOpenY = 200;
    }

    ConsoleDestY = ConsoleOpenY;
}

void Rend_ConsoleTicker(timespan_t time)
{
    float   step = time * 35;

    if(ConsoleY == 0)
        openingOrClosing = true;

    // Move the console to the destination Y.
    if(ConsoleDestY > ConsoleY)
    {
        float   diff = (ConsoleDestY - ConsoleY) * consoleMoveSpeed;

        if(diff < 1)
            diff = 1;
        ConsoleY += diff * step;
        if(ConsoleY > ConsoleDestY)
            ConsoleY = ConsoleDestY;
    }
    else if(ConsoleDestY < ConsoleY)
    {
        float   diff = (ConsoleY - ConsoleDestY) * consoleMoveSpeed;

        if(diff < 1)
            diff = 1;
        ConsoleY -= diff * step;
        if(ConsoleY < ConsoleDestY)
            ConsoleY = ConsoleDestY;
    }

    if(ConsoleY == ConsoleOpenY)
        openingOrClosing = false;

    funnyAng += step * consoleTurn / 10000;

    if(!Con_IsActive())
        return;                 // We have nothing further to do here.

    ConsoleBlink += step;       // Cursor blink timer (0 = visible).
}

void Rend_ConsoleFPS(int x, int y)
{
    int         w, h;
    char        buf[160];

    if(!consoleShowFPS)
        return;

    // If the ui is active draw the counter a bit further down
    if(UI_IsActive())
        y += 20;

    sprintf(buf, "%.1f FPS", DD_GetFrameRate());
    w = FR_TextWidth(buf) + 16;
    h = FR_TextHeight(buf) + 16;
    x -= w;
    UI_GradientEx(x, y, w, h, 6, UI_Color(UIC_BG_MEDIUM),
                  UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(x, y, w, h, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);
    UI_SetColor(UI_Color(UIC_TEXT));
    UI_TextOutEx(buf, x + 8, y + h / 2, false, true, UI_Color(UIC_TITLE), 1);
}

/**
 * NOTE: Slightly messy...
 */
void Rend_Console(void)
{
    extern uint bLineOff;

    int         i, k;               // Line count and buffer cursor.
    int         winWidth, winHeight;
    float       x, y;
    float       closeFade = 1;
    float       gtosMulY;
    char        buff[CMDLINE_SIZE + 1], temp[CMDLINE_SIZE + 1], *cmdLine;
    float       fontScaledY;
    int         bgX = 64, bgY = 64;
    int         textOffsetY = 0;
    uint        cmdCursor;
    cbuffer_t  *buffer;
    static cbline_t **lines = NULL;
    static int bufferSize = 0;
    int         reqLines;
    uint        count;

    if(ConsoleY <= 0)
        return;                 // We have nothing to do here.

    if(!Sys_GetWindowDimensions(windowIDX, NULL, NULL, &winWidth, &winHeight))
    {
        Con_Message("Rend_Console: Failed retrieving window dimensions.");
        return;
    }

    gtosMulY = winHeight / 200.0f;

    cmdLine = Con_GetCommandLine();
    cmdCursor = Con_CursorPosition();
    buffer = Con_GetConsoleBuffer();

    // Do we have a font?
    if(Cfont.TextOut == NULL)
    {
        Cfont.flags = DDFONT_WHITE;
        Cfont.height = FR_SingleLineHeight("Con");
        Cfont.sizeX = 1;
        Cfont.sizeY = 1;
        Cfont.TextOut = FR_ShadowTextOut;
        Cfont.Width = FR_TextWidth;
        Cfont.Filter = NULL;
    }

    FR_SetFont(glFontFixed);

    fontScaledY = Cfont.height * Cfont.sizeY;
    fontSy = fontScaledY / gtosMulY;
    textOffsetY = fontScaledY / 4;

    // Go into screen projection mode.
    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, winWidth, winHeight, -1, 1);

    BorderNeedRefresh = true;

    if(openingOrClosing)
    {
        closeFade = ConsoleY / (float) ConsoleOpenY;
    }

    // The console is composed of two parts: the main area background and the
    // border.
    gl.Color4f(consoleLight / 100.0f, consoleLight / 100.0f,
               consoleLight / 100.0f, closeFade * consoleAlpha / 100);

    // The background.
    if(gx.ConsoleBackground)
        gx.ConsoleBackground(&bgX, &bgY);

    // Let's make it a bit more interesting.
    gl.MatrixMode(DGL_TEXTURE);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Translatef(2 * sin(funnyAng / 4), 2 * cos(funnyAng / 4), 0);
    gl.Rotatef(funnyAng * 3, 0, 0, 1);
    GL_DrawRectTiled(0, (int) (ConsoleY * gtosMulY + 4), winWidth,
                     -winHeight - 4, bgX, bgY);
    gl.MatrixMode(DGL_TEXTURE);
    gl.PopMatrix();

    // The border.
    GL_DrawRect(0, (int) (ConsoleY * gtosMulY + 3), winWidth, 2, 0, 0, 0,
                closeFade);

    // Subtle shadow.
    gl.Begin(DGL_QUADS);
    gl.Color4f(.1f, .1f, .1f, closeFade * consoleAlpha / 150);
    gl.Vertex2f(0, (int) (ConsoleY * gtosMulY + 5));
    gl.Vertex2f(winWidth, (int) (ConsoleY * gtosMulY + 5));
    gl.Color4f(0, 0, 0, 0);
    gl.Vertex2f(winWidth, (int) (ConsoleY * gtosMulY + 13));
    gl.Vertex2f(0, (int) (ConsoleY * gtosMulY + 13));
    gl.End();

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
    gl.Scalef(Cfont.sizeX, Cfont.sizeY, 1);

    // The game & version number.
    //drawSideText(gx.Get(DD_GAME_ID), 2, closeFade);
    //drawSideText(gx.Get(DD_GAME_MODE), 1, closeFade);

    gl.Color4f(1, 1, 1, closeFade);

    // The text in the console buffer will be drawn from the bottom up (!).
    y = ConsoleY * gtosMulY - fontScaledY * 2 - textOffsetY;
    reqLines = ceil(y / fontScaledY);
    if(reqLines > 0)
    {
        // Need to enlarge the buffer?
        if(reqLines > bufferSize)
        {
            lines = Z_Realloc(lines, sizeof(cbline_t *) * (reqLines + 1),
                              PU_STATIC);
            bufferSize = reqLines;
        }

        count = Con_BufferGetLines(buffer, reqLines,
                                   -(reqLines + (int) bLineOff),
                                   lines);
        if(count > 0)
        {
            for(i = count - 1; i >= 0 && y > -fontScaledY; i--)
            {
                const cbline_t *line = lines[i];

                if(!line)
                    continue;

                if(line->flags & CBLF_RULER)
                {
                    // Draw a ruler here, and nothing else.
                    drawRuler2(y / Cfont.sizeY, Cfont.height, closeFade,
                               winWidth / Cfont.sizeX);
                }
                else
                {
                    if(!line->text)
                        continue;

                    memset(buff, 0, sizeof(buff));
                    strncpy(buff, line->text, 255);

                    x = line->flags & CBLF_CENTER ? (winWidth / Cfont.sizeX -
                                                     Cfont.Width(buff)) / 2 : 2;

                    if(Cfont.Filter)
                        Cfont.Filter(buff);
                    /*else if(consoleShadowText)
                    {
                        // Draw a shadow.
                        gl.Color3f(0, 0, 0);
                        Cfont.TextOut(buff, x + 2, y / Cfont.sizeY + 2);
                    }*/

                    // Set the color.
                    if(Cfont.flags & DDFONT_WHITE)  // Can it be colored?
                        consoleSetColor(line->flags, closeFade);
                    Cfont.TextOut(buff, x, y / Cfont.sizeY);
                }

                // Move up.
                y -= fontScaledY;
            }
        }
    }

    // The command line.
    strcpy(buff, ">");
    strncat(buff, cmdLine, 255);

    if(Cfont.Filter)
        Cfont.Filter(buff);
    /*if(consoleShadowText)
    {
        // Draw a shadow.
        gl.Color3f(0, 0, 0);
        Cfont.TextOut(buff, 4,
                      2 + (ConsoleY * gtosMulY - fontScaledY) / Cfont.sizeY);
    }*/
    if(Cfont.flags & DDFONT_WHITE)
        gl.Color4f(CcolYellow[0], CcolYellow[1], CcolYellow[2], closeFade);
    else
        gl.Color4f(1, 1, 1, closeFade);
    Cfont.TextOut(buff, 2, (ConsoleY * gtosMulY - fontScaledY - textOffsetY) /
                  Cfont.sizeY);

    // Width of the current char.
    temp[0] = cmdLine[cmdCursor];
    temp[1] = 0;
    k = Cfont.Width(temp);
    if(!k)
        k = Cfont.Width(" ");

    // What is the width?
    memset(temp, 0, sizeof(temp));
    strncpy(temp, buff, MIN_OF(250, cmdCursor) + 1);
    i = Cfont.Width(temp);

    // Draw the cursor in the appropriate place.
    if(!Con_IsLocked())
    {
        gl.Disable(DGL_TEXTURING);
        GL_DrawRect(2 + i, (ConsoleY * gtosMulY) / Cfont.sizeY,
                    k, -(Con_InputMode()? fontScaledY : textOffsetY),
                    CcolYellow[0], CcolYellow[1], CcolYellow[2],
                    closeFade * (((int) ConsoleBlink) & 0x10 ? .2f : .5f));
        gl.Enable(DGL_TEXTURING);
    }

    // Restore the original matrices.
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();

    // Draw the console title bar.
    {
    int width = 0;
    int height;
    int oldFont = FR_GetCurrent();
    int border = winWidth / 120;

    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    
    FR_SetFont(glFontVariable[GLFS_BOLD]);
    height = FR_TextHeight("W") + border;
    FR_SetFont(glFontVariable[GLFS_BOLD]);
    UI_Gradient(0, 0, winWidth, height, UI_Color(UIC_BG_MEDIUM),
                UI_Color(UIC_BG_LIGHT), .8f * closeFade, closeFade);
    UI_Gradient(0, height, winWidth, border, UI_Color(UIC_SHADOW),
                UI_Color(UIC_BG_DARK), closeFade, 0);
    UI_TextOutEx(consoleTitle, border, height / 2, false, true, UI_Color(UIC_TITLE),
                 closeFade);
    if(secondaryTitleText[0])
    {
        width = FR_TextWidth(consoleTitle) + FR_TextWidth("  ");
        FR_SetFont(glFontVariable[GLFS_LIGHT]);
        UI_TextOutEx(secondaryTitleText, border + width, height / 2,
                     false, true, UI_Color(UIC_TEXT), .75f * closeFade);
    }
    if(statusText[0])
    {
        width = FR_TextWidth(statusText);
        FR_SetFont(glFontVariable[GLFS_LIGHT]);
        UI_TextOutEx(statusText, winWidth - UI_BORDER - width, height / 2,
                     false, true, UI_Color(UIC_TEXT), .75f * closeFade);
    }

    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();

    FR_SetFont(oldFont);
    }

    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();
}

D_CMD(BackgroundTurn)
{
    consoleTurn = atoi(argv[1]);
    if(!consoleTurn)
        funnyAng = 0;
    return true;
}
