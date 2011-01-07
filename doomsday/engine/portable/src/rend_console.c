/**\file rend_console.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Console rendering.
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
float   consoleLight = .14f;
float   consoleBackgroundAlpha = .75f;
byte    consoleShowFPS = false;
byte    consoleShadowText = true;
float   consoleMoveSpeed = .5f; // Speed of console opening/closing

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static float ConsoleY;          // Where the console bottom is currently?
static float ConsoleDestY;      // Where the console bottom should be?
static float ConsoleBlink;      // Cursor blink timer (35 Hz tics).
static boolean openingOrClosing;
static float consoleAlpha, consoleAlphaTarget;
static float fontFx, fontSy;    // Font x factor and y size.

static float funnyAng;

static char *consoleTitle = DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT;
static char secondaryTitleText[256];
static char statusText[256];

// CODE --------------------------------------------------------------------

void Rend_ConsoleRegister(void)
{
    C_CMD_FLAGS("bgturn", "i", BackgroundTurn, CMDF_NO_DEDICATED);

    C_VAR_FLOAT("con-alpha", &consoleBackgroundAlpha, 0, 0, 1);
    C_VAR_FLOAT("con-light", &consoleLight, 0, 0, 1);
    C_VAR_BYTE("con-fps", &consoleShowFPS, 0, 0, 1);
    C_VAR_BYTE("con-text-shadow", &consoleShadowText, 0, 0, 1);
    C_VAR_FLOAT("con-move-speed", &consoleMoveSpeed, 0, 0, 1);
}

void Rend_ConsoleInit(void)
{
    ConsoleY = 0;
    ConsoleOpenY = 90;
    ConsoleDestY = 0;
    openingOrClosing = false;
    consoleAlpha = 0;
    consoleAlphaTarget = 0;

    // Font size in VGA coordinates. (Everything is in VGA coords.)
    fontFx = 1;
    fontSy = 9;

    funnyAng = 0;

    memset(secondaryTitleText, 0, sizeof(secondaryTitleText));
    memset(statusText, 0, sizeof(statusText));
}

void Rend_ConsoleCursorResetBlink(void)
{
    ConsoleBlink = 0;
}

static float GetConsoleTitleBarHeight(void)
{
    int oldFont = FR_GetCurrent();
    int border = theWindow->width / 120;
    int height;

    FR_SetFont(glFontVariable[GLFS_BOLD]);
    height = FR_TextHeight("W") + border;
    FR_SetFont(oldFont);
    return height;
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
    glColor4f(r, g, b, alpha);
}

static void drawRuler(int x, int y, int lineWidth, int lineHeight, float alpha)
{
    int xoff = 3, yoff = lineHeight / 4, rh = lineHeight / 2;

    UI_GradientEx(x + xoff, y + yoff + (lineHeight - rh) / 2, lineWidth - 2 * xoff, rh,
                  rh / 3, UI_Color(UIC_SHADOW), UI_Color(UIC_BG_DARK), alpha / 2, alpha);
    UI_DrawRectEx(x + xoff, y + yoff + (lineHeight - rh) / 2, lineWidth - 2 * xoff, rh,
                  -rh / 3, false, UI_Color(UIC_BRD_HI), 0, 0, alpha / 3);
}

/**
 * Initializes the Doomsday console user interface. This is called when
 * engine startup is complete.
 *
 * \todo Doesn't belong here.
 */
void Con_InitUI(void)
{
    if(isDedicated)
        return;

    // Update the secondary title and the game status.
    if(!DD_IsNullGameInfo(DD_GameInfo()))
    {
        dd_snprintf(secondaryTitleText, sizeof(secondaryTitleText)-1, "%s %s", (char*) gx.GetVariable(DD_PLUGIN_NAME), (char*) gx.GetVariable(DD_PLUGIN_VERSION_SHORT));
        strncpy(statusText, Str_Text(GameInfo_IdentityKey(DD_GameInfo())), sizeof(statusText) - 1);
        return;
    }
    // No game currently loaded.
    memset(secondaryTitleText, 0, sizeof(secondaryTitleText));
    memset(statusText, 0, sizeof(statusText));
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
            glColor4f(0, 0, 0, alpha);
            Cfont.TextOut(buf, ssw - Cfont.Width(buf) - 2,
                          y / Cfont.sizeY + 1);
        }
        */
        glColor4f(CcolYellow[0], CcolYellow[1], CcolYellow[2], alpha);
        Cfont.TextOut(buf, ssw - Cfont.Width(buf) - 3, y / Cfont.sizeY);
    }
}
#endif

static __inline int consoleMinHeight(void)
{
    return fontSy + ((Cfont.height * Cfont.sizeY)/8) + GetConsoleTitleBarHeight() /
        theWindow->height * 200;
}

void Rend_ConsoleToggleFullscreen(void)
{
    float       y;
    int         minHeight;

    if(isDedicated)
        return;

    minHeight = consoleMinHeight();
    if(ConsoleDestY == minHeight)
        y = 100;
    else if(ConsoleDestY == 100)
        y = 200;
    else
        y = minHeight;

    ConsoleDestY = ConsoleOpenY = y;
}

void Rend_ConsoleOpen(int yes)
{
    if(isDedicated)
        return;

    if(yes)
    {
        consoleAlphaTarget = 1;
        ConsoleDestY = ConsoleOpenY;
        Rend_ConsoleCursorResetBlink();
    }
    else
    {
        consoleAlphaTarget = 0;
        ConsoleDestY = 0;
    }
}

void Rend_ConsoleMove(int numLines)
{
    if(isDedicated)
        return;

    if(numLines == 0)
        return;

    if(numLines < 0)
    {
        int         minHeight = consoleMinHeight();

        ConsoleOpenY -= fontSy * -numLines;
        if(ConsoleOpenY < minHeight)
            ConsoleOpenY = minHeight;
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
    float           step;

    if(isDedicated)
        return;

    step = time * 35;

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

    // Move the console alpha to the target.
    if(consoleAlphaTarget > consoleAlpha)
    {
        float diff = MAX_OF(consoleAlphaTarget - consoleAlpha, .0001f) * consoleMoveSpeed;
        consoleAlpha += diff * step;
        if(consoleAlpha > consoleAlphaTarget)
            consoleAlpha = consoleAlphaTarget;
    }
    else if(consoleAlphaTarget < consoleAlpha)
    {
        float diff = MAX_OF(consoleAlpha - consoleAlphaTarget, .0001f) * consoleMoveSpeed;
        consoleAlpha -= diff * step;
        if(consoleAlpha < consoleAlphaTarget)
            consoleAlpha = consoleAlphaTarget;
    }

    if(ConsoleY == ConsoleOpenY)
        openingOrClosing = false;

    funnyAng += step * consoleTurn / 10000;

    if(!Con_IsActive())
        return; // We have nothing further to do here.

    ConsoleBlink += step; // Cursor blink timer (0 = visible).
}

void Rend_ConsoleFPS(int x, int y)
{
    int w, h;
    char buf[160];

    if(isDedicated)
        return;

    if(!consoleShowFPS)
        return;

    // If the ui is active draw the counter a bit further down
    if(UI_IsActive())
        y += 20;

    sprintf(buf, "%.1f FPS", DD_GetFrameRate());
    w = FR_TextWidth(buf) + 16;
    h = FR_TextHeight(buf) + 16;
    x -= w;

    glEnable(GL_TEXTURE_2D);

    UI_GradientEx(x, y, w, h, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .8f);
    UI_DrawRectEx(x, y, w, h, 6, false, UI_Color(UIC_BRD_HI), UI_Color(UIC_BG_MEDIUM), .2f, -1);
    UI_SetColor(UI_Color(UIC_TEXT));
    UI_TextOutEx(buf, x + 8, y + h / 2, false, true, UI_Color(UIC_TITLE), 1);

    glDisable(GL_TEXTURE_2D);
}

static void drawConsoleTitleBar(float alpha)
{
    int width = 0, height, oldFont, border;

    if(alpha < .0001f)
        return;

    oldFont = FR_GetCurrent();
    border = theWindow->width / 120;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();

    glEnable(GL_TEXTURE_2D);

    //FR_SetFont(glFontVariable[GLFS_BOLD]);
    height = GetConsoleTitleBarHeight(); //FR_TextHeight("W") + border;
    FR_SetFont(glFontVariable[GLFS_BOLD]);
    UI_Gradient(0, 0, theWindow->width, height, UI_Color(UIC_BG_MEDIUM),
                UI_Color(UIC_BG_LIGHT), .8f * alpha, alpha);
    UI_Gradient(0, height, theWindow->width, border, UI_Color(UIC_SHADOW),
                UI_Color(UIC_BG_DARK), .8f * alpha, 0);
    UI_TextOutEx(consoleTitle, border, height / 2, false, true, UI_Color(UIC_TITLE),
                 alpha);
    if(secondaryTitleText[0])
    {
        width = FR_TextWidth(consoleTitle) + FR_TextWidth("  ");
        FR_SetFont(glFontVariable[GLFS_LIGHT]);
        UI_TextOutEx(secondaryTitleText, border + width, height / 2,
                     false, true, UI_Color(UIC_TEXT), .75f * alpha);
    }
    if(statusText[0])
    {
        width = FR_TextWidth(statusText);
        FR_SetFont(glFontVariable[GLFS_LIGHT]);
        UI_TextOutEx(statusText, theWindow->width - UI_BORDER - width, height / 2,
                     false, true, UI_Color(UIC_TEXT), .75f * alpha);
    }

    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    FR_SetFont(oldFont);
}

static void drawConsoleBackground(int x, int y, int w, int h, float gtosMulY,
    float closeFade)
{
    int bgX = 64, bgY = 64;

    // The console is composed of two parts: the main area background
    // and the border.
    glColor4f(consoleLight, consoleLight, consoleLight,
              closeFade * consoleBackgroundAlpha);

    // The background.
    if(!DD_IsNullGameInfo(DD_GameInfo()) && gx.ConsoleBackground)
        gx.ConsoleBackground(&bgX, &bgY);

    // Let's make it a bit more interesting.
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();

    glTranslatef(2 * sin(funnyAng / 4), 2 * cos(funnyAng / 4), 0);
    glRotatef(funnyAng * 3, 0, 0, 1);

    /**
     * Make sure the current texture will be tiled.
     * Do NOT do this here. We have no idea what the current texture may be.
     * Instead simply assume that it has been suitably configured for tiling.
     * \fixme Refactor the way console background is drawn (do it entirely
     * engine-side or game-side).
     */
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    GL_DrawRectTiled(x, y, w, h, bgX, bgY);

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
}

/**
 * NOTE: Slightly messy...
 */
static void drawConsole(float consoleAlpha)
{
#define XORIGIN             (0)
#define YORIGIN             (0)
#define PADDING             (2)

    extern uint bLineOff;

    static const cbline_t** lines = 0;
    static int bufferSize = 0;

    cbuffer_t* buffer = Con_ConsoleBuffer();
    uint cmdCursor = Con_CursorPosition();
    char* cmdLine = Con_CommandLine();
    float y, fontScaledY, gtosMulY = theWindow->height / 200.0f;
    char buff[CMDLINE_SIZE + 1];
    int textOffsetY = 0;
    uint reqLines;

    // Do we have a font?
    if(Cfont.drawText == NULL)
    {
        Cfont.flags = DDFONT_WHITE;
        Cfont.height = FR_SingleLineHeight("Con");
        Cfont.sizeX = 1;
        Cfont.sizeY = 1;
        Cfont.drawText = FR_ShadowTextOut;
        Cfont.getWidth = FR_TextWidth;
        Cfont.filterText = NULL;
    }

    FR_SetFont(glFontFixed);

    fontScaledY = Cfont.height * Cfont.sizeY;
    fontSy = fontScaledY / gtosMulY;
    textOffsetY = fontScaledY / 4;

    drawConsoleBackground(XORIGIN, YORIGIN + (int) (ConsoleY * gtosMulY + 4),
                          theWindow->width, -theWindow->height - 4,
                          gtosMulY, consoleAlpha);

    // The border.
    GL_DrawRect(XORIGIN, YORIGIN + (int) (ConsoleY * gtosMulY + 4),
                theWindow->width, 2, 0, 0, 0, consoleAlpha * consoleAlpha * .75f);

    // Subtle shadow?
    { float shadowAlpha = consoleAlpha * consoleAlpha * consoleBackgroundAlpha * .66f;
    if(shadowAlpha > .0001f)
    {
        glBegin(GL_QUADS);
            glColor4f(.1f, .1f, .1f, shadowAlpha);
            glVertex2f(XORIGIN, YORIGIN + (int) (ConsoleY * gtosMulY + 5));
            glVertex2f(XORIGIN + theWindow->width, YORIGIN + (int) (ConsoleY * gtosMulY + 5));
            glColor4f(0, 0, 0, 0);
            glVertex2f(XORIGIN + theWindow->width, YORIGIN + (int) (ConsoleY * gtosMulY + 13));
            glVertex2f(XORIGIN, YORIGIN + (int) (ConsoleY * gtosMulY + 13));
        glEnd();
    }}

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glScalef(Cfont.sizeX, Cfont.sizeY, 1);

    glColor4f(1, 1, 1, consoleAlpha);

    // The console history log is drawn from bottom to top.
    y = ConsoleY * gtosMulY - fontScaledY * 2 - textOffsetY;

    reqLines = MAX_OF(0, ceil(y / fontScaledY)+1);
    if(reqLines != 0)
    {
        uint count, totalLines = Con_BufferNumLines(buffer);
        int firstIdx;

        firstIdx = -((long)(reqLines + bLineOff));
        if(firstIdx < -((long)totalLines))
            firstIdx = -((long)totalLines);

        // Need to enlarge the buffer?
        if(reqLines > (uint) bufferSize)
        {
            lines = Z_Realloc((void*) lines, sizeof(cbline_t *) * (reqLines + 1), PU_APPSTATIC);
            bufferSize = reqLines;
        }

        count = Con_BufferGetLines2(buffer, reqLines, firstIdx, lines, BLF_OMIT_EMPTYLINE);
        if(count != 0)
        {
            glEnable(GL_TEXTURE_2D);

            { uint i;
            for(i = count; i-- > 0;)
            {
                const cbline_t* line = lines[i];

                if(line->flags & CBLF_RULER)
                {   // Draw a ruler here, and nothing else.
                    drawRuler(XORIGIN + PADDING, (YORIGIN + y) / Cfont.sizeY,
                               theWindow->width / Cfont.sizeX - PADDING*2, Cfont.height,
                               consoleAlpha);
                }
                else
                {
                    float xOffset;

                    memset(buff, 0, sizeof(buff));
                    strncpy(buff, line->text, 255);

                    if(line->flags & CBLF_CENTER)
                        xOffset = (theWindow->width / Cfont.sizeX - Cfont.getWidth(buff)) / 2;
                    else
                        xOffset = 0;

                    if(Cfont.filterText)
                        Cfont.filterText(buff);
                    /*else if(consoleShadowText)
                    {
                        // Draw a shadow.
                        glColor3f(0, 0, 0);
                        Cfont.drawText(buff, XORIGIN + PADDING + xOffset + 2, y / Cfont.sizeY + 2);
                    }*/

                    // Set the color.
                    if(Cfont.flags & DDFONT_WHITE)  // Can it be colored?
                        consoleSetColor(line->flags, consoleAlpha);
                    Cfont.drawText(buff, XORIGIN + PADDING + xOffset, YORIGIN + y / Cfont.sizeY);
                }

                // Move up.
                y -= fontScaledY;
            }}

            glDisable(GL_TEXTURE_2D);
        }
    }

    // The command line.
    strcpy(buff, ">");
    strncat(buff, cmdLine, 255);

    if(Cfont.filterText)
        Cfont.filterText(buff);
    /*if(consoleShadowText)
    {
        // Draw a shadow.
        glColor3f(0, 0, 0);
        Cfont.TextOut(buff, 4,
                      2 + (ConsoleY * gtosMulY - fontScaledY) / Cfont.sizeY);
    }*/
    if(Cfont.flags & DDFONT_WHITE)
        glColor4f(CcolYellow[0], CcolYellow[1], CcolYellow[2], consoleAlpha);
    else
        glColor4f(1, 1, 1, consoleAlpha);

    glEnable(GL_TEXTURE_2D);

    y = ConsoleY * gtosMulY - fontScaledY - textOffsetY;
    Cfont.drawText(buff, XORIGIN + PADDING, YORIGIN + y / Cfont.sizeY);

    glDisable(GL_TEXTURE_2D);

    // Draw the cursor in the appropriate place.
    if(!Con_IsLocked())
    {
        float halfInterlineHeight = (float)textOffsetY / 2;
        float width, height, xOffset, yOffset;
        char temp[CMDLINE_SIZE + 1];

        // Width of the current character.
        temp[0] = cmdLine[cmdCursor];
        temp[1] = 0;
        width = Cfont.getWidth(temp);
        if(width == 0)
            width = Cfont.getWidth(" ");

        // Where is the cursor?
        memset(temp, 0, sizeof(temp));
        strncpy(temp, buff, MIN_OF(250, cmdCursor) + 1);
        xOffset = Cfont.getWidth(temp);
        if(Con_InputMode())
        {
            height = fontScaledY;
            yOffset = halfInterlineHeight;
        }
        else
        {
            height = halfInterlineHeight;
            yOffset = fontScaledY;
        }

        GL_DrawRect(XORIGIN + PADDING + (int)xOffset, (int)((YORIGIN + y + yOffset) / Cfont.sizeY),
                    (int)width, (int)(height / Cfont.sizeY),
                    CcolYellow[0], CcolYellow[1], CcolYellow[2],
                    consoleAlpha * (((int) ConsoleBlink) & 0x10 ? .2f : .5f));
    }

    // Restore the original matrices.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

#undef PADDING
#undef YORIGIN
#undef XORIGIN
}

void Rend_Console(void)
{
    boolean consoleShow;

    if(isDedicated)
        return;

    consoleShow = (ConsoleY > 0 || openingOrClosing);
    if(!consoleShow && !consoleShowFPS)
        return;

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    if(consoleShow)
    {
        drawConsole(consoleAlpha);        drawConsoleTitleBar(consoleAlpha);    }
    if(consoleShowFPS && !UI_IsActive())
        Rend_ConsoleFPS(theWindow->width - 10, 10 + (ConsoleY > 0? consoleAlpha * GetConsoleTitleBarHeight() : 0));

    // Restore original matrix.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

D_CMD(BackgroundTurn)
{
    consoleTurn = atoi(argv[1]);
    if(!consoleTurn)
        funnyAng = 0;
    return true;
}