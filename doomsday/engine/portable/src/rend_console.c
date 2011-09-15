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
#include "de_render.h"
#include "de_ui.h"

#include "math.h"
#include "materialvariant.h"
#include "cbuffer.h"

#include "rend_console.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Rend_ConsoleUpdateBackground(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float ConsoleOpenY; // Where the console bottom is when open.
float consoleMoveSpeed = .5f; // Speed of console opening/closing.

float consoleBackgroundAlpha = .75f;
float consoleBackgroundLight = .14f;
char* consoleBackgroundMaterialName = "";
int consoleBackgroundTurn = 0; // The rotation variable.
float consoleBackgroundZoom = 1.0f;

byte consoleTextShadow = false;
byte consoleShowFPS = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;
static float ConsoleY; // Where the console bottom is currently?
static float ConsoleDestY; // Where the console bottom should be?
static float ConsoleBlink; // Cursor blink timer (35 Hz tics).
static boolean openingOrClosing;
static float consoleAlpha, consoleAlphaTarget;
static material_t* consoleBackgroundMaterial;

static float fontSy; // Font size Y.
static float funnyAng;

static const float CcolYellow[3] = { 1, .85f, .3f };
static const char* consoleTitle = DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT;

static char secondaryTitleText[256];
static char statusText[256];

// CODE --------------------------------------------------------------------

void Rend_ConsoleRegister(void)
{
    C_VAR_FLOAT("con-background-alpha", &consoleBackgroundAlpha, 0, 0, 1);
    C_VAR_FLOAT("con-background-light", &consoleBackgroundLight, 0, 0, 1);
    C_VAR_CHARPTR2("con-background-material", &consoleBackgroundMaterialName,
        0, 0, 0, Rend_ConsoleUpdateBackground);
    C_VAR_INT("con-background-turn", &consoleBackgroundTurn, CVF_NO_MIN|CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("con-background-zoom", &consoleBackgroundZoom, 0, 0.1f, 100.0f);
    C_VAR_BYTE("con-fps", &consoleShowFPS, 0, 0, 1);
    C_VAR_FLOAT("con-move-speed", &consoleMoveSpeed, 0, 0, 1);
    C_VAR_BYTE("con-text-shadow", &consoleTextShadow, 0, 0, 1);
}

static float calcConsoleTitleBarHeight(void)
{
    assert(inited);
    {
    int border = theWindow->width / 120, height;
    int oldFont = FR_Font();

    FR_SetFont(fontVariable[FS_BOLD]);
    height = FR_SingleLineHeight("Con") + border;
    FR_SetFont(oldFont);
    return height;
    }
}

static __inline int calcConsoleMinHeight(void)
{
    assert(inited);
    return fontSy * 1.5f + calcConsoleTitleBarHeight() / theWindow->height * SCREENHEIGHT;
}

void Rend_ConsoleInit(void)
{
    if(!inited)
    {   // First init.
        ConsoleY = 0;
        ConsoleOpenY = SCREENHEIGHT/2;
        ConsoleDestY = 0;
        ConsoleBlink = 0;
        openingOrClosing = false;
        consoleAlpha = 0;
        consoleAlphaTarget = 0;
    }

    consoleBackgroundMaterial = 0;
    funnyAng = 0;
    // Font size in VGA coordinates. (Everything is in VGA coords.)
    fontSy = 9;

    if(inited)
    {
        Rend_ConsoleUpdateTitle();
        Rend_ConsoleUpdateBackground();
    }
    else
    {   // First init.
        memset(secondaryTitleText, 0, sizeof(secondaryTitleText));
        memset(statusText, 0, sizeof(statusText));
    }
    inited = true;
}

void Rend_ConsoleCursorResetBlink(void)
{
    assert(inited);
    ConsoleBlink = 0;
}

// Calculate the average of the given color flags.
static void calcAvgColor(int fl, float rgb[3])
{
    assert(inited && rgb);
    {
    int count = 0;
    rgb[CR] = rgb[CG] = rgb[CB] = 0;

    if(fl & CBLF_BLACK)
    {
        ++count;
    }
    if(fl & CBLF_BLUE)
    {
        rgb[CB] += 1;
        ++count;
    }
    if(fl & CBLF_GREEN)
    {
        rgb[CG] += 1;
        ++count;
    }
    if(fl & CBLF_CYAN)
    {
        rgb[CG] += 1;
        rgb[CB] += 1;
        ++count;
    }
    if(fl & CBLF_RED)
    {
        rgb[CR] += 1;
        ++count;
    }
    if(fl & CBLF_MAGENTA)
    {
        rgb[CR] += 1;
        rgb[CB] += 1;
        ++count;
    }
    if(fl & CBLF_YELLOW)
    {
        rgb[CR] += CcolYellow[0];
        rgb[CG] += CcolYellow[1];
        rgb[CB] += CcolYellow[2];
        ++count;
    }
    if(fl & CBLF_WHITE)
    {
        rgb[CR] += 1;
        rgb[CG] += 1;
        rgb[CB] += 1;
        ++count;
    }
    // Calculate the average.
    if(count > 1)
    {
        rgb[CR] /= count;
        rgb[CG] /= count;
        rgb[CB] /= count;
    }
    if(fl & CBLF_LIGHT)
    {
        rgb[CR] += (1 - rgb[CR]) / 2;
        rgb[CG] += (1 - rgb[CG]) / 2;
        rgb[CB] += (1 - rgb[CB]) / 2;
    }
    }
}

static void drawRuler(int x, int y, int lineWidth, int lineHeight, float alpha)
{
    assert(inited);
    {
    int xoff = 3, yoff = lineHeight / 4, rh = MIN_OF(5, lineHeight / 2);

    UI_GradientEx(x + xoff, y + yoff + (lineHeight - rh) / 2, lineWidth - 2 * xoff, rh,
                  rh / 3, UI_Color(UIC_SHADOW), UI_Color(UIC_BG_DARK), alpha / 2, alpha);
    UI_DrawRectEx(x + xoff, y + yoff + (lineHeight - rh) / 2, lineWidth - 2 * xoff, rh,
                  -rh / 3, false, UI_Color(UIC_BRD_HI), 0, 0, alpha / 3);
    }
}

/**
 * Initializes the Doomsday console user interface. This is called when
 * engine startup is complete.
 */
void Rend_ConsoleUpdateTitle(void)
{
    if(isDedicated)
        return;

    assert(inited);

    // Update the secondary title and the game status.
    if(!DD_IsNullGameInfo(DD_GameInfo()))
    {
        dd_snprintf(secondaryTitleText, sizeof(secondaryTitleText)-1, "%s %s", (char*) gx.GetVariable(DD_PLUGIN_NAME), (char*) gx.GetVariable(DD_PLUGIN_VERSION_SHORT));
        strncpy(statusText, Str_Text(GameInfo_Title(DD_GameInfo())), sizeof(statusText) - 1);
        return;
    }
    // No game currently loaded.
    memset(secondaryTitleText, 0, sizeof(secondaryTitleText));
    memset(statusText, 0, sizeof(statusText));
}

void Rend_ConsoleUpdateBackground(void)
{
    assert(inited);
    consoleBackgroundMaterial = Materials_ToMaterial(
        Materials_IndexForName(consoleBackgroundMaterialName));
}

void Rend_ConsoleToggleFullscreen(void)
{
    float y;
    int minHeight;

    if(isDedicated)
        return;

    assert(inited);

    minHeight = calcConsoleMinHeight();
    if(ConsoleDestY == minHeight)
        y = SCREENHEIGHT/2;
    else if(ConsoleDestY == SCREENHEIGHT/2)
        y = SCREENHEIGHT;
    else
        y = minHeight;

    ConsoleDestY = ConsoleOpenY = y;
}

void Rend_ConsoleOpen(int yes)
{
    if(isDedicated)
        return;

    assert(inited);

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

    assert(inited);

    if(numLines == 0)
        return;

    if(numLines < 0)
    {
        int minHeight = calcConsoleMinHeight();

        ConsoleOpenY -= fontSy * -numLines;
        if(ConsoleOpenY < minHeight)
            ConsoleOpenY = minHeight;
    }
    else
    {
        ConsoleOpenY += fontSy * numLines;
        if(ConsoleOpenY > SCREENHEIGHT)
            ConsoleOpenY = SCREENHEIGHT;
    }

    ConsoleDestY = ConsoleOpenY;
}

void Rend_ConsoleTicker(timespan_t time)
{
    float step;

    if(isDedicated)
        return;

    assert(inited);

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

    if(!Con_IsActive())
        return; // We have nothing further to do here.

    if(consoleBackgroundTurn != 0)
        funnyAng += step * consoleBackgroundTurn / 10000;

    ConsoleBlink += step; // Cursor blink timer (0 = visible).
}

void Rend_ConsoleFPS(int x, int y)
{
    int w, h;
    char buf[160];

    if(isDedicated)
        return;

    assert(inited);

    if(!consoleShowFPS)
        return;

    // If the ui is active draw the counter a bit further down
    if(UI_IsActive())
        y += 20;

    sprintf(buf, "%.1f FPS", DD_GetFrameRate());
    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    w = FR_TextWidth(buf) + 16;
    h = FR_SingleLineHeight(buf)  + 16;

    glEnable(GL_TEXTURE_2D);

    UI_GradientEx(x-w, y, w, h, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .8f);
    UI_DrawRectEx(x-w, y, w, h, 6, false, UI_Color(UIC_BRD_HI), UI_Color(UIC_BG_MEDIUM), .2f, -1);
    UI_SetColor(UI_Color(UIC_TEXT));
    UI_TextOutEx2(buf, x - 8, y + h / 2, UI_Color(UIC_TITLE), 1, ALIGN_RIGHT, DTF_ONLY_SHADOW);

    glDisable(GL_TEXTURE_2D);
}

static void drawConsoleTitleBar(float alpha)
{
    assert(inited);
    {
    int width = 0, height, border;

    if(alpha < .0001f)
        return;

    border = theWindow->width / 120;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();

    glEnable(GL_TEXTURE_2D);

    height = calcConsoleTitleBarHeight();
    UI_Gradient(0, 0, theWindow->width, height, UI_Color(UIC_BG_MEDIUM),
                UI_Color(UIC_BG_LIGHT), .8f * alpha, alpha);
    UI_Gradient(0, height, theWindow->width, border, UI_Color(UIC_SHADOW),
                UI_Color(UIC_BG_DARK), .6f * alpha, 0);
    UI_Gradient(0, height, theWindow->width, border*2, UI_Color(UIC_BG_DARK),
                UI_Color(UIC_SHADOW), .2f * alpha, 0);
    FR_SetFont(fontVariable[FS_BOLD]);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    UI_TextOutEx2(consoleTitle, border, height / 2, UI_Color(UIC_TITLE), alpha, ALIGN_LEFT, DTF_ONLY_SHADOW);
    if(secondaryTitleText[0])
    {
        width = FR_TextWidth(consoleTitle) + FR_TextWidth("  ");
        FR_SetFont(fontVariable[FS_LIGHT]);
        UI_TextOutEx2(secondaryTitleText, border + width, height / 2, UI_Color(UIC_TEXT), .33f * alpha,
                      ALIGN_LEFT, DTF_ONLY_SHADOW);
    }
    if(statusText[0])
    {
        FR_SetFont(fontVariable[FS_LIGHT]);
        UI_TextOutEx2(statusText, theWindow->width - border, height / 2, UI_Color(UIC_TEXT), .75f * alpha,
                      ALIGN_RIGHT, DTF_ONLY_SHADOW);
    }

    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    }
}

static void drawConsoleBackground(int x, int y, int w, int h, float gtosMulY,
    float closeFade)
{
    assert(inited);
    {
    int bgX = 0, bgY = 0;

    if(consoleBackgroundMaterial)
    {
        material_snapshot_t ms;

        Materials_Prepare(&ms, consoleBackgroundMaterial, Con_IsActive(),
            Materials_VariantSpecificationForContext(MC_UI, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                0, 1, 0, false, false, false, false));
        GL_BindTexture(MSU(&ms, MTU_PRIMARY).tex.glName, MSU(&ms, MTU_PRIMARY).magMode);

        bgX = (int) (ms.width  * consoleBackgroundZoom);
        bgY = (int) (ms.height * consoleBackgroundZoom);

        glEnable(GL_TEXTURE_2D);
        if(consoleBackgroundTurn != 0)
        {
            glMatrixMode(GL_TEXTURE);
            glPushMatrix();
            glLoadIdentity();
            glTranslatef(2 * sin(funnyAng / 4), 2 * cos(funnyAng / 4), 0);
            glRotatef(funnyAng * 3, 0, 0, 1);
        }
    }

    glColor4f(consoleBackgroundLight, consoleBackgroundLight, consoleBackgroundLight, closeFade * consoleBackgroundAlpha);
    GL_DrawRectTiled(x, y, w, h, bgX, bgY);

    if(consoleBackgroundMaterial)
    {
        glDisable(GL_TEXTURE_2D);
        if(funnyAng != 0)
        {
            glMatrixMode(GL_TEXTURE);
            glPopMatrix();
        }
    }
    }
}

/**
 * Draw a 'side' text in the console. This is intended for extra
 * information about the current game mode.
 *
 * \note Currently unused.
 */
static void drawSideText(const char* text, int line, float alpha)
{
    assert(inited);
    {
    float gtosMulY = theWindow->height / 200.0f, fontScaledY, y, scale[2];
    char buf[300];
    int ssw;

    FR_SetFont(Con_Font());
    FR_LoadDefaultAttrib();
    Con_FontScale(&scale[0], &scale[1]);
    fontScaledY = FR_SingleLineHeight("Con") * scale[1];
    y = ConsoleY * gtosMulY - fontScaledY * (1 + line);

    if(y > -fontScaledY)
    {
        con_textfilter_t printFilter = Con_PrintFilter();

        // Scaled screen width.
        ssw = (int) (theWindow->width / scale[0]);

        if(printFilter)
        {
            strncpy(buf, text, sizeof(buf));
            printFilter(buf);
            text = buf;
        }

        FR_SetColorAndAlpha(CcolYellow[0], CcolYellow[1], CcolYellow[2], alpha * .75f);
        FR_DrawText3(text, ssw - 3, y / scale[1], ALIGN_TOPRIGHT, DTF_NO_TYPEIN|DTF_NO_GLITTER|(!consoleTextShadow?DTF_NO_SHADOW:0));
    }
    }
}

/**
 * NOTE: Slightly messy...
 */
static void drawConsole(float consoleAlpha)
{
#define XORIGIN             (0)
#define YORIGIN             (0)
#define PADDING             (2)
#define LOCALBUFFSIZE       (CMDLINE_SIZE +1/*prompt length*/ +1/*terminator*/)

    assert(inited);
    {
    static const cbline_t** lines = 0;
    static int bufferSize = 0;

    cbuffer_t* buffer = Con_HistoryBuffer();
    uint cmdCursor = Con_CommandLineCursorPosition();
    char* cmdLine = Con_CommandLine();
    float scale[2], y, fontScaledY, gtosMulY = theWindow->height / 200.0f;
    char buff[LOCALBUFFSIZE];
    font_t* cfont;
    int lineHeight, textOffsetY;
    con_textfilter_t printFilter = Con_PrintFilter();
    uint reqLines, maxLineLength;

    FR_SetFont(Con_Font());
    FR_LoadDefaultAttrib();
    FR_SetTracking(Con_FontTracking());
    FR_SetColorAndAlpha(1, 1, 1, consoleAlpha);

    cfont = Fonts_ToFont(FR_Font());
    lineHeight = FR_SingleLineHeight("Con");
    Con_FontScale(&scale[0], &scale[1]);
    fontScaledY = lineHeight * Con_FontLeading() * scale[1];
    fontSy = fontScaledY / gtosMulY;
    textOffsetY = PADDING + fontScaledY / 4;

    drawConsoleBackground(XORIGIN, YORIGIN + (int) (ConsoleY * gtosMulY),
                          theWindow->width, -theWindow->height,
                          gtosMulY, consoleAlpha);

    // The border.
    UI_Gradient(XORIGIN, YORIGIN + (int) ((ConsoleY - 10) * gtosMulY), theWindow->width,
                10 * gtosMulY, UI_Color(UIC_BG_DARK), UI_Color(UIC_BRD_HI), 0,
                consoleAlpha * consoleBackgroundAlpha * .06f);
    UI_Gradient(XORIGIN, YORIGIN + (int) (ConsoleY * gtosMulY), theWindow->width,
                2, UI_Color(UIC_BG_LIGHT), UI_Color(UIC_BG_LIGHT),
                consoleAlpha * consoleBackgroundAlpha, -1);
    UI_Gradient(XORIGIN, YORIGIN + (int) (ConsoleY * gtosMulY), theWindow->width,
                2 * gtosMulY, UI_Color(UIC_SHADOW), UI_Color(UIC_SHADOW),
                consoleAlpha * consoleBackgroundAlpha * .75f, 0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glScalef(scale[0], scale[1], 1);

    // The console history log is drawn from bottom to top.
    y = ConsoleY * gtosMulY - (lineHeight * scale[1] + fontScaledY) - textOffsetY;

    reqLines = MAX_OF(0, ceil(y / fontScaledY)+1);
    if(reqLines != 0)
    {
        uint count, totalLines = CBuffer_NumLines(buffer);
        int firstIdx;

        firstIdx = -((long)(reqLines + Con_HistoryOffset()));
        if(firstIdx < -((long)totalLines))
            firstIdx = -((long)totalLines);

        // Need to enlarge the buffer?
        if(reqLines > (uint) bufferSize)
        {
            lines = Z_Realloc((void*) lines, sizeof(cbline_t *) * (reqLines + 1), PU_APPSTATIC);
            bufferSize = reqLines;
        }

        count = CBuffer_GetLines2(buffer, reqLines, firstIdx, lines, BLF_OMIT_EMPTYLINE);
        if(count != 0)
        {
            glEnable(GL_TEXTURE_2D);

            { uint i;
            for(i = count; i-- > 0;)
            {
                const cbline_t* line = lines[i];

                if(line->flags & CBLF_RULER)
                {   // Draw a ruler here, and nothing else.
                    drawRuler(XORIGIN + PADDING, (YORIGIN + y) / scale[1],
                               theWindow->width / scale[0] - PADDING*2, lineHeight,
                               consoleAlpha);
                }
                else
                {
                    int alignFlags = 0;
                    short textFlags = DTF_NO_TYPEIN|DTF_NO_GLITTER|(!consoleTextShadow?DTF_NO_SHADOW:0);
                    float xOffset;

                    memset(buff, 0, sizeof(buff));
                    strncpy(buff, line->text, LOCALBUFFSIZE-1);

                    if(line->flags & CBLF_CENTER)
                    {
                        alignFlags |= ALIGN_TOP;
                        xOffset = (theWindow->width / scale[0]) / 2;
                    }
                    else
                    {
                        alignFlags |= ALIGN_TOPLEFT;
                        xOffset = 0;
                    }

                    if(printFilter)
                        printFilter(buff);

                    // Set the color.
                    if(Font_Flags(cfont) & FF_COLORIZE)
                    {
                        float rgb[3];
                        calcAvgColor(line->flags, rgb);
                        FR_SetColorv(rgb);
                    }
                    FR_DrawText3(buff, XORIGIN + PADDING + xOffset, YORIGIN + y / scale[1], alignFlags, textFlags);
                }

                // Move up.
                y -= fontScaledY;
            }}

            glDisable(GL_TEXTURE_2D);
        }
    }

    // The command line.
    { boolean abbrevLeft = 0, abbrevRight = 0;
    int offset = 0;
    uint cmdLineLength;

    y = ConsoleY * gtosMulY - (lineHeight * scale[1]) - textOffsetY;

    cmdLineLength = (uint)strlen(cmdLine);
    maxLineLength = CBuffer_MaxLineLength(buffer) - 1/*prompt length*/;

    if(cmdLineLength >= maxLineLength)
    {
        maxLineLength -= 5; /*abbrev vis length*/

        if((signed)cmdCursor - (signed)maxLineLength > 0 || cmdCursor > maxLineLength)
        {
            abbrevLeft = true;
            maxLineLength -= 5; /*abbrev vis length*/
        }

        offset = MAX_OF(0, (signed)cmdCursor - (signed)maxLineLength);
        abbrevRight = (offset + maxLineLength < cmdLineLength);
        if(!abbrevRight)
        {
            maxLineLength += 5; /*abbrev vis length*/
            offset = MAX_OF(0, (signed)cmdCursor - (signed)maxLineLength);
        }
    }

    dd_snprintf(buff, LOCALBUFFSIZE -1/*terminator*/, ">%s%.*s%s",
        abbrevLeft?  "{alpha=.5}[...]{alpha=1}" : "",
        maxLineLength, cmdLine + offset,
        abbrevRight? "{alpha=.5}[...]" : "");

    if(printFilter)
        printFilter(buff);

    glEnable(GL_TEXTURE_2D);
    if(Font_Flags(cfont) & FF_COLORIZE)
    {
        FR_SetColorAndAlpha(CcolYellow[0], CcolYellow[1], CcolYellow[2], consoleAlpha);
    }
    else
    {
        FR_SetColorAndAlpha(1, 1, 1, consoleAlpha);
    }

    FR_DrawText3(buff, XORIGIN + PADDING, YORIGIN + y / scale[1], ALIGN_TOPLEFT, DTF_NO_TYPEIN|DTF_NO_GLITTER|(!consoleTextShadow?DTF_NO_SHADOW:0));
    glDisable(GL_TEXTURE_2D);

    // Draw the cursor in the appropriate place.
    if(Con_IsActive() && !Con_IsLocked())
    {
        float width, height, halfInterlineHeight = (lineHeight * scale[1]) / 8.f;
        int xOffset, yOffset = 2 * scale[1];
        char temp[LOCALBUFFSIZE];

        // Where is the cursor?
        memset(temp, 0, sizeof(temp));
        strncpy(temp, buff, MIN_OF(LOCALBUFFSIZE -1/*prompt length*/ -1/*vis clamp*/, cmdCursor-offset + (abbrevLeft? 24/*abbrev length*/:0) + 1));
        xOffset = FR_TextWidth(temp);
        if(Con_InputMode())
        {
            height  = lineHeight * scale[1];
            yOffset += halfInterlineHeight;
        }
        else
        {
            height  = halfInterlineHeight;
            yOffset += lineHeight * scale[1];
        }

        // Dimensions of the current character.
        width = FR_CharWidth(cmdLine[cmdCursor] == '\0'? ' ' : cmdLine[cmdCursor]);

        glColor4f(CcolYellow[0], CcolYellow[1], CcolYellow[2],
                  consoleAlpha * (((int) ConsoleBlink) & 0x10 ? .2f : .5f));
        GL_DrawRect(XORIGIN + PADDING + xOffset, (int)((YORIGIN + y + yOffset) / scale[1]),
                    (int)width, MAX_OF(1, (int)(height / scale[1])));
    }

    // Restore the original matrices.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    }
    }
#undef LOCALBUFFSIZE
#undef PADDING
#undef YORIGIN
#undef XORIGIN
}

void Rend_Console(void)
{
    boolean consoleShow;

    if(isDedicated)
        return;

    assert(inited);

    consoleShow = (ConsoleY > 0);// || openingOrClosing);
    if(!consoleShow && !consoleShowFPS)
        return;

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    if(consoleShow)
    {
        drawConsole(consoleAlpha);
        drawConsoleTitleBar(consoleAlpha);
    }

    if(consoleShowFPS && !UI_IsActive())
        Rend_ConsoleFPS(theWindow->width - 10, 10 + (ConsoleY > 0? ROUND(consoleAlpha * calcConsoleTitleBarHeight()) : 0));

    // Restore original matrix.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
