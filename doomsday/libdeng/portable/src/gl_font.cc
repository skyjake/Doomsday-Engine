/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * gl_font.c: Font Renderer
 *
 * The font must be small enough to fit in one texture
 * (not a problem with *real* video cards! ;-)).
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"

#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#ifndef WIN32
#  define SYSTEM_FONT           1
#  define SYSTEM_FIXED_FONT     2
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void* readFormat0(DFILE* file, jfrfont_t* font);
static void* readFormat2(DFILE* file, jfrfont_t* font);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int initFROk = 0;
static int numFonts = 0;
static jfrfont_t* fonts = NULL; // The list of fonts.
static int currentFontIndex; // Index of the current font.
static filename_t fontpath = "";

// CODE --------------------------------------------------------------------

static int getFontByName(const char* name)
{
    int                 i;
    size_t              len = strlen(name);

    for(i = 0; i < numFonts; ++i)
    {
        const jfrfont_t*    font = &fonts[i];

        if(!strnicmp(font->name, name, len))
            return i;
    }

    return -1;
}

static int getFontById(int id)
{
    int                 i;

    for(i = 0; i < numFonts; ++i)
    {
        const jfrfont_t*    font = &fonts[i];

        if(font->id == id)
            return i;
    }

    return -1;
}

/**
 * Destroys the font with the index.
 */
static void destroyFontIdx(int idx)
{
    jfrfont_t*          font = &fonts[idx];

    glDeleteTextures(1, (const GLuint*) &font->tex);
    memmove(&fonts[idx], &fonts[idx + 1],
            sizeof(jfrfont_t) * (numFonts - idx - 1));
    numFonts--;

    fonts = (jfrfont_t*) M_Realloc(fonts, sizeof(jfrfont_t) * numFonts);
    if(currentFontIndex == idx)
        currentFontIndex = -1;
}

static int getNewId(void)
{
    int                 i, max = 0;

    for(i = 0; i < numFonts; ++i)
    {
        jfrfont_t*          font = &fonts[i];

        if(font->id > max)
            max = font->id;
    }

    return max + 1;
}

/**
 * @return              @c 0, iff there are no errors.
 */
int FR_Init(void)
{
    if(initFROk)
        return -1; // No reinitializations...

    numFonts = 0;
    fonts = 0; // No fonts!
    currentFontIndex = -1;
    initFROk = 1;

    // Check the font path.
    if(ArgCheckWith("-fontdir", 1))
    {
        M_TranslatePath(fontpath, ArgNext(), FILENAME_T_MAXLEN);
        Dir_ValidDir(fontpath, FILENAME_T_MAXLEN);
        M_CheckPath(fontpath);
    }
    else
    {
        strncpy(fontpath, ddBasePath, FILENAME_T_MAXLEN);
        strncat(fontpath, "data\\fonts\\", FILENAME_T_MAXLEN);
    }
    return 0;
}

void FR_Shutdown(void)
{
    // Destroy all fonts.
    while(numFonts)
        destroyFontIdx(0);
    fonts = 0;
    currentFontIndex = -1;
    initFROk = 0;
}

int FR_GetFontIdx(int id)
{
    int                 idx;

    if((idx = getFontById(id)) == -1)
        Con_Printf("FR_GetFontIdx: Unknown ID %i.\n", id);

    return idx;
}

void FR_DestroyFont(int id)
{
    int                 idx;

    if((idx = getFontById(id)) != -1)
    {
        destroyFontIdx(idx);
        if(currentFontIndex == idx)
            currentFontIndex = -1;
    }
}

jfrfont_t* FR_GetFont(int id)
{
    int                 idx = getFontById(id);

    if(idx == -1)
        return 0;
    return fonts + idx;
}

#ifdef WIN32
static void outByte(FILE* f, byte b)
{
    fwrite(&b, sizeof(b), 1, f);
}

static void outShort(FILE* f, short s)
{
    fwrite(&s, sizeof(s), 1, f);
}

static int dumpFont(const char* filename, const jfrchar_t chars[256],
                    const uint32_t* image, int w, int h)
{
    FILE*               file = fopen(filename, "wb");
    int                 i, c, bit, numPels;
    byte                mask;

    if(!file)
        return false;

    // Write header.
    outByte(file, 0); // Version.
    outShort(file, w);
    outShort(file, h);
    outShort(file, MAX_CHARS); // Number of characters.

    // Characters.
    for(i = 0; i < MAX_CHARS; ++i)
    {
        outShort(file, chars[i].x);
        outShort(file, chars[i].y);
        outByte(file, chars[i].w);
        outByte(file, chars[i].h);
    }

    // Write a zero to indicate the data is in bitmap format (0,1).
    outByte(file, 0);
    numPels = w * h;
    for(c = i = 0; i < (numPels + 7) / 8; ++i)
    {
        for(mask = 0, bit = 7; bit >= 0; bit--, ++c)
            mask |= (c < numPels ? image[c] != 0 : false) << bit;
        outByte(file, mask);
    }

    fclose(file);
    return true;
}
#endif

static int createFont(void)
{
    jfrfont_t*          font;

    currentFontIndex = numFonts;
    fonts = (jfrfont_t*) M_Realloc(fonts, sizeof(jfrfont_t) * ++numFonts);
    font = &fonts[currentFontIndex];

    memset(font, 0, sizeof(jfrfont_t));
    font->id = getNewId();

    return currentFontIndex;
}

static byte inByte(DFILE* f)
{
    byte                b;

    F_Read(&b, sizeof(b), f);
    return b;
}

static unsigned short inShort(DFILE* f)
{
    unsigned short s;

    F_Read(&s, sizeof(s), f);
    return USHORT(s);
}

/**
 * Prepares a font from a file. If the given file is not found, prepares
 * the corresponding GDI font.
 */
int FR_PrepareFont(const char* name)
{
#ifdef WIN32
    struct {
        char*       name;
        int         gdires;
        char        winfontname[32]; // Limit including terminator.
        int         pointsize;
    } fontmapper[] = {
        {"Fixed", SYSTEM_FIXED_FONT},
        {"Fixed12", 0, "Fixedsys", 12},
        {"System", SYSTEM_FONT},
        {"System12", 0, "System", 12},
        {"Large", 0, "MS Sans Serif", 18},
        {"Small7", 0, "Small Fonts", 7},
        {"Small8", 0, "Small Fonts", 8},
        {"Small10", 0, "Small Fonts", 10},
        {NULL, 0}
    };
#endif
    filename_t          buf;
    DFILE*              file;
    int                 idx, version;
    jfrfont_t*          font;
    void*               image;

    // Is this a font we've already prepared?
    if((idx = getFontByName(name)) != -1)
    {
        /*font = &fonts[idx];
        if(font->tex)*/
        return true;
    }

    strcpy(buf, fontpath);
    strcat(buf, name);
    strcat(buf, ".dfn");
    if((file = F_Open(buf, "rb")) == NULL)
    {
        return false;
    }
/*    if(ArgCheck("-gdifonts") || (file = F_Open(buf, "rb")) == NULL)
    {
        boolean             retVal = false;
#ifdef WIN32
        HWND                hWnd = Sys_GetWindowHandle(windowIDX);
        HDC                 hDC = NULL;
        int                 i;

        if(hWnd)
            hDC = GetDC(hWnd);

        // No luck...
        for(i = 0; fontmapper[i].name; ++i)
        {
            if(!stricmp(fontmapper[i].name, name))
            {
                if(verbose)
                {
                    Con_Printf("FR_PrepareFont: GDI font for \"%s\".\n",
                               fontmapper[i].name);
                }
                if(hWnd && hDC && fontmapper[i].winfontname)
                {
                    HFONT               uifont =
                        CreateFont(-MulDiv(fontmapper[i].pointsize,
                                           GetDeviceCaps(hDC, LOGPIXELSY), 72),
                                   0, 0, 0, 0, FALSE,
                                   FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS,
                                   DEFAULT_QUALITY,
                                   VARIABLE_PITCH | FF_SWISS,
                                   (LPCTSTR) fontmapper[i].winfontname);

                    FR_PrepareGDIFont(uifont);
                    DeleteObject(uifont);
                }
                else
                {
                    FR_PrepareGDIFont(GetStockObject(fontmapper[i].gdires));
                }

                strcpy(fonts[currentFontIndex].name, name);
                retVal = true;
                break;
            }
        }

        if(hWnd && hDC)
            ReleaseDC(hWnd, hDC);
#endif
        // The font was not found.
        return retVal;
    }*/

    VERBOSE2(Con_Printf("FR_PrepareFont: %s\n", M_PrettyPath(buf)));

    version = inByte(file);

    VERBOSE2(Con_Printf("FR_PrepareFont: Version %i.\n", version));

    // Load the font from the file.
    createFont();
    font = &fonts[currentFontIndex];
    strncpy(font->name, name, 255);
    font->name[255] = '\0';

    VERBOSE(Con_Printf("FR_PrepareFont: New font %i: %s.\n", currentFontIndex,
                       font->name));

    // Font glyph map.
    image = 0;

    switch(version)
    {
    case 0: // Original format.
        image = readFormat0(file, font);
        break;

    case 2: // Enhanced format.
        image = readFormat2(file, font);
        break;

    default:
        Con_Error("FR_PrepareFont: Format is unsupported.\n");
        break;
    }

    if(image)
    {
        VERBOSE2(Con_Printf("FR_PrepareFont: Creating GL texture.\n"));

        // Load in the texture.
        font->tex = GL_NewTextureWithParams2(DGL_RGBA,
                                             font->texWidth, font->texHeight, image,
                                             0, GL_LINEAR, GL_NEAREST,
                                             -1 /*best anisotropy*/,
                                             GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        M_Free(image);
        image = 0;

        F_Close(file);
    }

    VERBOSE2(Con_Printf("FR_PrepareFont: Loaded %s.\n", name));
    return true;
}

static void* readFormat0(DFILE* file, jfrfont_t* font)
{
    int                 i, c, numChars = 0, format, numPels;
    uint32_t*           image;

    font->hasEmbeddedShadow = false;
    font->marginWidth = font->marginHeight = 0;

    // Load in the data.
    font->texWidth = inShort(file);
    font->texHeight = inShort(file);
    numChars = inShort(file);
    VERBOSE2(Con_Printf("readFormat0: Dimensions %i x %i, with %i chars.\n",
                        font->texWidth, font->texHeight, numChars));

    for(i = 0; i < numChars; ++i)
    {
        jfrchar_t*          ch = &font->chars[i < MAX_CHARS ? i : MAX_CHARS - 1];

        ch->x = inShort(file);
        ch->y = inShort(file);
        ch->w = inByte(file);
        ch->h = inByte(file);
    }

    // The bitmap.
    format = inByte(file);
    if(format > 0)
    {
        Con_Error("readFormat0: Font %s uses unknown bitmap format %i.\n",
                  font->name, format);
    }

    numPels = font->texWidth * font->texHeight;
    image = (uint32_t*) M_Calloc(numPels * sizeof(int));
    for(c = i = 0; i < (numPels + 7) / 8; ++i)
    {
        int                 bit, mask = inByte(file);

        for(bit = 7; bit >= 0; bit--, ++c)
        {
            if(c >= numPels)
                break;
            if(mask & (1 << bit))
                image[c] = ~0;
        }
    }

    return image;
}

static void* readFormat2(DFILE* file, jfrfont_t* font)
{
    int                 i, numPels, dataHeight, glyphCount = 0;
    byte                bitmapFormat = 0;
    uint32_t*           image, *ptr;

    bitmapFormat = inByte(file);
    if(bitmapFormat != 1 && bitmapFormat != 0) // Luminance + Alpha.
    {
        Con_Error("FR_ReadFormat1: Bitmap format %i not implemented.\n",
                  bitmapFormat);
    }

    font->hasEmbeddedShadow = true;

    // Load in the data.
    font->texWidth = inShort(file);
    dataHeight = inShort(file);
    font->texHeight = M_CeilPow2(dataHeight);
    glyphCount = inShort(file);
    font->marginWidth = font->marginHeight = inShort(file);
    font->lineHeight = inShort(file);
    font->glyphHeight = inShort(file);
    font->ascent = inShort(file);
    font->descent = inShort(file);

    for(i = 0; i < glyphCount; ++i)
    {
        ushort              code = inShort(file);
        ushort              x = inShort(file);
        ushort              y = inShort(file);
        ushort              w = inShort(file);
        ushort              h = inShort(file);

        if(code < MAX_CHARS)
        {
            font->chars[code].x = x;
            font->chars[code].y = y;
            font->chars[code].w = w;
            font->chars[code].h = h;
        }
    }

    // Read the bitmap.
    numPels = font->texWidth * font->texHeight;
    image = ptr = (uint32_t*) M_Calloc(numPels * 4);
    if(bitmapFormat == 0)
    {
        for(i = 0; i < numPels; ++i)
        {
            byte                red = inByte(file);
            byte                green = inByte(file);
            byte                blue = inByte(file);
            byte                alpha = inByte(file);

            *ptr++ = ULONG(red | (green << 8) | (blue << 16) | (alpha << 24));
        }
    }
    else if(bitmapFormat == 1)
    {
        for(i = 0; i < numPels; ++i)
        {
            byte luminance = inByte(file);
            byte alpha = inByte(file);
            *ptr++ = ULONG(luminance | (luminance << 8) | (luminance << 16) |
                      (alpha << 24));
        }
    }

    return image;
}

/**
 * Prepare a GDI font. Select it as the current font.
 */
#ifdef WIN32
int FR_PrepareGDIFont(HFONT hfont)
{
    RECT                rect;
    jfrfont_t*          font;
    int                 i, x, y, maxh, bmpWidth = 256, bmpHeight = 0,
                        imgWidth, imgHeight;
    HDC                 hdc;
    HBITMAP             hbmp;
    uint32_t*           image;

    // Create a new font.
    createFont();
    font = &fonts[currentFontIndex];

    // Now we'll create the actual data.
    hdc = CreateCompatibleDC(NULL);
    SetMapMode(hdc, MM_TEXT);
    SelectObject(hdc, hfont);

    // Let's first find out the sizes of all the characters.
    // Then we can decide how large a texture we need.
    for(i = 0, x = 0, y = 0, maxh = 0; i < 256; ++i)
    {
        jfrchar_t*          fc = font->chars + i;
        SIZE                size;
        byte                ch[2];

        ch[0] = i;
        ch[1] = 0;

        GetTextExtentPoint32(hdc, (LPCTSTR) ch, 1, &size);
        fc->w = size.cx;
        fc->h = size.cy;
        maxh = max(maxh, fc->h);
        x += fc->w + 1;
        if(x >= bmpWidth)
        {
            x = 0;
            y += maxh + 1;
            maxh = 0;
        }
    }

    bmpHeight = y + maxh;
    hbmp = CreateCompatibleBitmap(hdc, bmpWidth, bmpHeight);
    SelectObject(hdc, hbmp);
    SetBkMode(hdc, OPAQUE);
    SetBkColor(hdc, 0);
    SetTextColor(hdc, 0xffffff);
    rect.left = 0;
    rect.top = 0;
    rect.right = bmpWidth;
    rect.bottom = bmpHeight;
    FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));

    // Print all the characters.
    for(i = 0, x = 0, y = 0, maxh = 0; i < 256; ++i)
    {
        jfrchar_t*      fc = font->chars + i;
        char            ch[2];

        ch[0] = i;
        ch[1] = '\0';

        if(x + fc->w + 1 >= bmpWidth)
        {
            x = 0;
            y += maxh + 1;
            maxh = 0;
        }

        if(i)
            TextOut(hdc, x + 1, y + 1, ch, 1);

        fc->x = x + 1;
        fc->y = y + 1;
        maxh = max(maxh, fc->h);
        x += fc->w + 1;
    }

    // Now we can make a version that DGL can read.
    imgWidth = M_CeilPow2(bmpWidth);
    imgHeight = M_CeilPow2(bmpHeight);
/*
#if _DEBUG
Con_Printf( "font: %d x %d\n", imgWidth, imgHeight);
#endif
*/
    image = (uint32_t*) M_Malloc(4 * imgWidth * imgHeight);
    memset(image, 0, 4 * imgWidth * imgHeight);
    for(y = 0; y < bmpHeight; ++y)
        for(x = 0; x < bmpWidth; ++x)
            if(GetPixel(hdc, x, y))
            {
                image[x + y * imgWidth] = 0xffffffff;
                /*if(x+1 < imgWidth && y+1 < imgHeight)
                   image[x+1 + (y+1)*imgWidth] = 0xff000000; */
            }

    font->texWidth = imgWidth;
    font->texHeight = imgHeight;

    // If necessary, write the font data to a file.
    if(ArgCheck("-dumpfont"))
    {
        char                buf[20];

        sprintf(buf, "font%i.dfn", font->id);
        dumpFont(buf, font->chars, image, font->texWidth, font->texHeight);
    }

    // Create the DGL texture.
    font->tex = GL_NewTextureWithParams2(DGL_RGBA, imgWidth, imgHeight, image,
                                         0, GL_NEAREST, GL_NEAREST,
                                         -1 /*best anisotropy*/,
                                         GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    // We no longer need these.
    M_Free(image);
    DeleteObject(hbmp);
    DeleteDC(hdc);
    return 0;
}
#endif // WIN32

/**
 * Change the current font.
 */
void FR_SetFont(int id)
{
    int                 idx = -1;

    idx = FR_GetFontIdx(id);
    if(idx == -1)
        return; // No such font.
    currentFontIndex = idx;
}

int FR_CharWidth(int ch)
{
    jfrfont_t*          cf;

    if(currentFontIndex == -1)
        return 0;
    cf = &fonts[currentFontIndex];

    return cf->chars[ch].w - cf->marginWidth * 2;
}

int FR_TextWidth(const char* text)
{
    size_t              i, len;
    int                 width = 0;
    jfrfont_t*          cf;

    if(currentFontIndex == -1 || !text)
        return 0;

    // Just add them together.
    len = strlen(text);
    cf = &fonts[currentFontIndex];
    for(i = 0; i < len; ++i)
        width += cf->chars[(byte) text[i]].w - 2 * cf->marginWidth;

    return width;
}

int FR_TextHeight(const char* text)
{
    size_t              i, len;
    int                 height = 0;
    jfrfont_t*          cf;

    if(currentFontIndex == -1 || !text)
        return 0;

    // Find the greatest height.
    len = strlen(text);
    cf = &fonts[currentFontIndex];
    for(i = 0; i < len; ++i)
        height = MAX_OF(height, cf->chars[(byte) text[i]].h - 2 * cf->marginHeight);

    return height;
}

int FR_SingleLineHeight(const char* text)
{
    jfrfont_t*          cf;

    if(currentFontIndex == -1 || !text)
        return 0;

    cf = &fonts[currentFontIndex];
    if(cf->ascent)
    {
        return cf->ascent;
    }

    return cf->chars[(byte)text[0]].h - 2 * cf->marginHeight;
}

int FR_GlyphTopToAscent(const char* text)
{
    jfrfont_t*          cf;

    if(currentFontIndex == -1 || !text)
        return 0;

    cf = &fonts[currentFontIndex];
    if(!cf->lineHeight)
        return 0;

    return cf->lineHeight - cf->ascent;
}

int FR_GetCurrent(void)
{
    if(currentFontIndex == -1)
        return 0;

    return fonts[currentFontIndex].id;
}

/**
 * (x,y) is the upper left corner. Returns the length.
 */
int FR_CustomShadowTextOut(const char* text, int x, int y, int shadowX,
                           int shadowY, float shadowAlpha)
{
    int                 width = 0, step;
    size_t              i, len;
    jfrfont_t*          cf;
    float               origColor[4];
    boolean             drawShadow = (shadowX || shadowY);

    if(!text)
        return 0;

    len = strlen(text);

    // Check the font.
    if(currentFontIndex == -1)
        return 0; // No selected font.
    cf = &fonts[currentFontIndex];

    if(cf->hasEmbeddedShadow)
        drawShadow = false;

    if(drawShadow)
    {
        // The color of the text itself.
        glGetFloatv(GL_CURRENT_COLOR, origColor);
        glColor4f(0, 0, 0, origColor[3] * shadowAlpha);
    }

    // Set the texture.
    glBindTexture(GL_TEXTURE_2D, cf->tex);

    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glScalef(1.f / cf->texWidth, 1.f / cf->texHeight, 1.f);

    // Print it.
    glBegin(GL_QUADS);

    if(drawShadow)
    {
        int                 startX = x + shadowX, startY = y + shadowY;

        for(i = 0; i < len; ++i)
        {
            // First draw the shadow.
            const jfrchar_t*    ch = &cf->chars[(byte) text[i]];

            // Upper left.
            glTexCoord2f(ch->x, ch->y);
            glVertex2f(x, y);
            // Upper right.
            glTexCoord2f(ch->x + ch->w, ch->y);
            glVertex2f(x + ch->w, y);
            // Lower right.
            glTexCoord2f(ch->x + ch->w, ch->y + ch->h);
            glVertex2f(x + ch->w, y + ch->h);
            // Lower left.
            glTexCoord2f(ch->x, ch->y + ch->h);
            glVertex2f(x, y + ch->h);
            // Move on.
            x += ch->w;
        }

        x = startX;
        y = startY;
    }

    if(drawShadow)
        glColor4fv(origColor);

    x -= cf->marginWidth;
    y -= cf->marginHeight;

    for(i = 0; i < len; ++i)
    {
        const jfrchar_t*    ch = &cf->chars[(byte) text[i]];

        // Upper left.
        glTexCoord2f(ch->x, ch->y);
        glVertex2f(x, y);
        // Upper right.
        glTexCoord2f(ch->x + ch->w, ch->y);
        glVertex2f(x + ch->w, y);
        // Lower right.
        glTexCoord2f(ch->x + ch->w, ch->y + ch->h);
        glVertex2f(x + ch->w, y + ch->h);
        // Lower left.
        glTexCoord2f(ch->x, ch->y + ch->h);
        glVertex2f(x, y + ch->h);
        // Move on.
        step = ch->w - 2*cf->marginWidth;
        width += step;
        x += step;
    }
    glEnd();

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    return width;
}

int FR_TextOut(const char* text, int x, int y)
{
    return FR_CustomShadowTextOut(text, x, y, 0, 0, 0);
}

int FR_ShadowTextOut(const char* text, int x, int y)
{
    return FR_CustomShadowTextOut(text, x, y, 2, 2, .5f);
}
