/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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
 * gl_font.c: Font Renderer
 *
 * The font must be small enough to fit in one texture
 * (not a problem with *real* video cards! ;-)).
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "de_base.h"
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

static void* FR_ReadFormat0(DFILE *file, jfrfont_t *font);
static void* FR_ReadFormat2(DFILE *file, jfrfont_t *font);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int initFROk = 0;
static int numFonts = 0;
static jfrfont_t *fonts = NULL; // The list of fonts.
static int currentFontIndex;                // Index of the current font.
static char fontpath[256] = "";

// CODE --------------------------------------------------------------------

/**
 * Returns zero if there are no errors.
 */
int FR_Init(void)
{
    if(initFROk)
        return -1;              // No reinitializations...

    numFonts = 0;
    fonts = 0;                  // No fonts!
    currentFontIndex = -1;
    initFROk = 1;

    // Check the font path.
    if(ArgCheckWith("-fontdir", 1))
    {
        M_TranslatePath(ArgNext(), fontpath);
        Dir_ValidDir(fontpath);
        M_CheckPath(fontpath);
    }
    else
    {
        strcpy(fontpath, ddBasePath);
        strcat(fontpath, "data\\fonts\\");
    }
    return 0;
}

/**
 * Destroys the font with the index.
 */
static void FR_DestroyFontIdx(int idx)
{
    jfrfont_t *font = fonts + idx;

    gl.DeleteTextures(1, (DGLuint*) &font->texture);
    memmove(fonts + idx, fonts + idx + 1,
            sizeof(jfrfont_t) * (numFonts - idx - 1));
    numFonts--;
    fonts = M_Realloc(fonts, sizeof(jfrfont_t) * numFonts);
    if(currentFontIndex == idx)
        currentFontIndex = -1;
}

void FR_Shutdown(void)
{
    // Destroy all fonts.
    while(numFonts)
        FR_DestroyFontIdx(0);
    fonts = 0;
    currentFontIndex = -1;
    initFROk = 0;
}

static void OutByte(FILE *f, byte b)
{
    fwrite(&b, sizeof(b), 1, f);
}

static void OutShort(FILE *f, short s)
{
    fwrite(&s, sizeof(s), 1, f);
}

static byte InByte(DFILE *f)
{
    byte    b;

    F_Read(&b, sizeof(b), f);
    return b;
}

static unsigned short InShort(DFILE *f)
{
    unsigned short s;

    F_Read(&s, sizeof(s), f);
    return USHORT(s);
}

int FR_GetFontIdx(int id)
{
    int     i;

    for(i = 0; i < numFonts; ++i)
        if(fonts[i].id == id)
            return i;

    Con_Printf("FR_GetFontIdx: Unknown ID %i.\n", id);
    return -1;
}

void FR_DestroyFont(int id)
{
    int     idx = FR_GetFontIdx(id);

    FR_DestroyFontIdx(idx);
    if(currentFontIndex == idx)
        currentFontIndex = -1;
}

jfrfont_t *FR_GetFont(int id)
{
    int     idx = FR_GetFontIdx(id);

    if(idx == -1)
        return 0;
    return fonts + idx;
}

static int FR_GetMaxId(void)
{
    int     i, grid = 0;

    for(i = 0; i < numFonts; ++i)
        if(fonts[i].id > grid)
            grid = fonts[i].id;

    return grid;
}

static int findPow2(int num)
{
    int     cumul = 1;

    for(; num > cumul; cumul *= 2);

    return cumul;
}

#ifdef WIN32
static int FR_SaveFont(char *filename, jfrfont_t *font, unsigned int *image)
{
    FILE   *file = fopen(filename, "wb");
    int     i, c, bit, numPels;
    byte    mask;

    if(!file)
        return false;

    // Write header.
    OutByte(file, 0);           // Version.
    OutShort(file, font->texWidth);
    OutShort(file, font->texHeight);
    OutShort(file, MAX_CHARS);  // Number of characters.

    // Characters.
    for(i = 0; i < MAX_CHARS; ++i)
    {
        OutShort(file, font->chars[i].x);
        OutShort(file, font->chars[i].y);
        OutByte(file, font->chars[i].w);
        OutByte(file, font->chars[i].h);
    }

    // Write a zero to indicate the data is in bitmap format (0,1).
    OutByte(file, 0);
    numPels = font->texWidth * font->texHeight;
    for(c = i = 0; i < (numPels + 7) / 8; ++i)
    {
        for(mask = 0, bit = 7; bit >= 0; bit--, ++c)
            mask |= (c < numPels ? image[c] != 0 : false) << bit;
        OutByte(file, mask);
    }

    fclose(file);
    return true;
}
#endif

static int FR_NewFont(void)
{
    jfrfont_t *font;

    currentFontIndex = numFonts;
    fonts = M_Realloc(fonts, sizeof(jfrfont_t) * ++numFonts);
    font = fonts + currentFontIndex;
    memset(font, 0, sizeof(jfrfont_t));
    font->id = FR_GetMaxId() + 1;
    return currentFontIndex;
}

/**
 * Prepares a font from a file. If the given file is not found,
 * prepares the corresponding GDI font.
 */
int FR_PrepareFont(const char *name)
{
#ifdef WIN32
    struct {
        char   *name;
        int     gdires;
        char    winfontname[32]; // limit including terminator.
        int     pointsize;
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
    filename_t buf;
    DFILE  *file;
    int     version;
    jfrfont_t *font;
    void   *image;

    strcpy(buf, fontpath);
    strcat(buf, name);
    strcat(buf, ".dfn");
    if(ArgCheck("-gdifonts") || (file = F_Open(buf, "rb")) == NULL)
    {
        boolean retVal = false;
#ifdef WIN32
        HWND    hWnd = Sys_GetWindowHandle(windowIDX);
        HDC     hDC = NULL;
        int     i;

        if(hWnd)
            hDC = GetDC(hWnd);

        // No luck...
        for(i = 0; fontmapper[i].name; ++i)
            if(!stricmp(fontmapper[i].name, name))
            {
                if(verbose)
                {
                    Con_Printf("FR_PrepareFont: GDI font for \"%s\".\n",
                               fontmapper[i].name);
                }
                if(hWnd && hDC && fontmapper[i].winfontname)
                {
                    HFONT   uifont =
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

        if(hWnd && hDC)
            ReleaseDC(hWnd, hDC);
#endif
        // The font was not found.
        return retVal;
    }

    VERBOSE2(Con_Printf("FR_PrepareFont: %s\n", M_Pretty(buf)));

    version = InByte(file);

    VERBOSE2(Con_Printf("FR_PrepareFont: Version %i.\n", version));

    // Load the font from the file.
    FR_NewFont();
    font = fonts + currentFontIndex;
    strcpy(font->name, name);

    VERBOSE(Con_Printf("FR_PrepareFont: New font %i: %s.\n", currentFontIndex,
                       font->name));

    // Font glyph map.
    image = 0;

    switch(version)
    {
    case 0: // Original format.
        image = FR_ReadFormat0(file, font);
        break;

    case 2: // Enhanced format.
        image = FR_ReadFormat2(file, font);
        break;

    default:
        Con_Error("FR_PrepareFont: Format is unsupported.\n");
        break;
    }

    if(image)
    {
        VERBOSE2(Con_Printf("FR_PrepareFont: Creating GL texture.\n"));

        // Load in the texture.
        font->texture = GL_NewTextureWithParams2(DGL_RGBA,
                                                 font->texWidth, font->texHeight, image,
                                                 0, DGL_LINEAR, DGL_NEAREST,
                                                 -1 /*best anisotropy*/,
                                                 DGL_CLAMP, DGL_CLAMP);                                                 

        M_Free(image); image = 0;

        F_Close(file);
    }

    VERBOSE2(Con_Printf("FR_PrepareFont: Loaded %s.\n", name));
    return true;
}

static void* FR_ReadFormat0(DFILE *file, jfrfont_t*font)
{
    int     numChars = 0;
    int     i;
    int     format;
    int     numPels;
    int     c, bit, mask;
    int    *image;
    jfrchar_t *ch;

    font->hasEmbeddedShadow = false;

    font->marginWidth = 0;
    font->marginHeight = 0;

    // Load in the data.
    font->texWidth = InShort(file);
    font->texHeight = InShort(file);
    numChars = InShort(file);
    VERBOSE2(Con_Printf("FR_PrepareFont: Dimensions %i x %i, with %i chars.\n",
                        font->texWidth, font->texHeight, numChars));
    for(i = 0; i < numChars; ++i)
    {
        ch = &font->chars[i < MAX_CHARS ? i : MAX_CHARS - 1];
        ch->x = InShort(file);
        ch->y = InShort(file);
        ch->w = InByte(file);
        ch->h = InByte(file);
    }

    // The bitmap.
    format = InByte(file);
    if(format > 0)
    {
        Con_Error("FR_PrepareFont: Font %s uses unknown bitmap format %i.\n",
                  font->name, format);
    }

    numPels = font->texWidth * font->texHeight;
    image = M_Calloc(numPels * sizeof(int));
    for(c = i = 0; i < (numPels + 7) / 8; ++i)
    {
        mask = InByte(file);
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

static void* FR_ReadFormat2(DFILE *file, jfrfont_t *font)
{
    int     i;
    int     glyphCount = 0;
    byte    bitmapFormat = 0;
    int     numPels;
    uint   *image, *ptr;
    int     dataHeight;

    bitmapFormat = InByte(file);
    if(bitmapFormat != 1 && bitmapFormat != 0) // Luminance + Alpha.
    {
        Con_Error("FR_ReadFormat1: Bitmap format %i not implemented.\n",
                  bitmapFormat);
    }

    font->hasEmbeddedShadow = true;

    // Load in the data.
    font->texWidth = InShort(file);
    dataHeight = InShort(file);
    font->texHeight = findPow2(dataHeight);
    glyphCount = InShort(file);
    font->marginWidth = font->marginHeight = InShort(file);
    font->lineHeight = InShort(file);
    font->glyphHeight = InShort(file);
    font->ascent = InShort(file);
    font->descent = InShort(file);

    for(i = 0; i < glyphCount; ++i)
    {
        unsigned short code = InShort(file);
        unsigned short x = InShort(file);
        unsigned short y = InShort(file);
        unsigned short w = InShort(file);
        unsigned short h = InShort(file);

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
    image = ptr = M_Calloc(numPels * 4);
    if(bitmapFormat == 0)
    {
        for(i = 0; i < numPels; ++i)
        {
            byte red = InByte(file);
            byte green = InByte(file);
            byte blue = InByte(file);
            byte alpha = InByte(file);
            *ptr++ = ULONG(red | (green << 8) | (blue << 16) | (alpha << 24));
        }
    }
    else if(bitmapFormat == 1)
    {
        for(i = 0; i < numPels; ++i)
        {
            byte luminance = InByte(file);
            byte alpha = InByte(file);
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
    RECT    rect;
    jfrfont_t *font;
    int     i, x, y, maxh, bmpWidth = 256, bmpHeight = 0, imgWidth, imgHeight;
    HDC     hdc;
    HBITMAP hbmp;
    unsigned int *image;

    // Create a new font.
    FR_NewFont();
    font = fonts + currentFontIndex;

    // Now we'll create the actual data.
    hdc = CreateCompatibleDC(NULL);
    SetMapMode(hdc, MM_TEXT);
    SelectObject(hdc, hfont);
    // Let's first find out the sizes of all the characters.
    // Then we can decide how large a texture we need.
    for(i = 0, x = 0, y = 0, maxh = 0; i < 256; ++i)
    {
        jfrchar_t *fc = font->chars + i;
        SIZE    size;
        byte    ch[2];

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
    FillRect(hdc, &rect, GetStockObject(BLACK_BRUSH));
    // Print all the characters.
    for(i = 0, x = 0, y = 0, maxh = 0; i < 256; ++i)
    {
        jfrchar_t *fc = font->chars + i;
        byte    ch[2];

        ch[0] = i;
        ch[1] = 0;

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
    imgWidth = findPow2(bmpWidth);
    imgHeight = findPow2(bmpHeight);
/*
#if _DEBUG
Con_Printf( "font: %d x %d\n", imgWidth, imgHeight);
#endif
*/
    image = M_Malloc(4 * imgWidth * imgHeight);
    memset(image, 0, 4 * imgWidth * imgHeight);
    for(y = 0; y < bmpHeight; ++y)
        for(x = 0; x < bmpWidth; ++x)
            if(GetPixel(hdc, x, y))
            {
                image[x + y * imgWidth] = 0xffffffff;
                /*if(x+1 < imgWidth && y+1 < imgHeight)
                   image[x+1 + (y+1)*imgWidth] = 0xff000000; */
            }

/*
saveTGA24_rgba8888("ddfont.tga", bmpWidth, bmpHeight,
                   (unsigned char*)image);
 */

    font->texWidth = imgWidth;
    font->texHeight = imgHeight;

    // If necessary, write the font data to a file.
    if(ArgCheck("-dumpfont"))
    {
        char    buf[20];

        sprintf(buf, "font%i.dfn", font->id);
        FR_SaveFont(buf, font, image);
    }

    // Create the DGL texture.
    font->texture = GL_NewTextureWithParams2(DGL_RGBA, imgWidth, imgHeight, image,
                                             0, DGL_NEAREST, DGL_NEAREST,
                                             -1 /*best anisotropy*/,
                                             DGL_CLAMP, DGL_CLAMP);

    // We no longer need these.
    M_Free(image);
    DeleteObject(hbmp);
    DeleteDC(hdc);
    return 0;
}
#endif                          // WIN32

/**
 * Change the current font.
 */
void FR_SetFont(int id)
{
    int     idx = -1;

    idx = FR_GetFontIdx(id);
#if _DEBUG
    VERBOSE2(Con_Printf("FR_SetFont: Font id %i has index %i.\n", id, idx));
#endif
    if(idx == -1)
        return;                 // No such font.
    currentFontIndex = idx;
}

int FR_CharWidth(int ch)
{
    if(currentFontIndex == -1)
        return 0;

    return fonts[currentFontIndex].chars[ch].w -
        fonts[currentFontIndex].marginWidth * 2;
}

int FR_TextWidth(const char *text)
{
    int     i, width = 0, len = strlen(text);
    jfrfont_t *cf;

    if(currentFontIndex == -1)
        return 0;

    // Just add them together.
    for(cf = fonts + currentFontIndex, i = 0; i < len; ++i)
        width += cf->chars[(byte) text[i]].w - 2*cf->marginWidth;

    return width;
}

int FR_TextHeight(const char *text)
{
    int     i, height = 0, len;
    jfrfont_t *cf;

    if(currentFontIndex == -1 || !text)
        return 0;

    cf = fonts + currentFontIndex;

    // Find the greatest height.
    for(len = strlen(text), i = 0; i < len; ++i)
        height = MAX_OF(height, cf->chars[(byte) text[i]].h - 2*cf->marginHeight);

    return height;
}

int FR_SingleLineHeight(const char *text)
{
    jfrfont_t *cf;

    if(currentFontIndex == -1 || !text)
        return 0;

    cf = fonts + currentFontIndex;
    if(cf->ascent)
    {
        return cf->ascent;
    }

    return cf->chars[(byte)text[0]].h - 2*cf->marginHeight;
}

int FR_GlyphTopToAscent(const char *text)
{
    jfrfont_t *cf;

    if(currentFontIndex == -1 || !text)
        return 0;

    cf = fonts + currentFontIndex;
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
int FR_CustomShadowTextOut(const char *text, int x, int y, int shadowX, int shadowY,
                           float shadowAlpha)
{
    int     i, width = 0, len, step;
    jfrfont_t *cf;
    int     origColor[4];
    boolean drawShadow = (shadowX || shadowY);

    if(!text)
        return 0;

    len = strlen(text);

    // Check the font.
    if(currentFontIndex == -1)
        return 0;               // No selected font.
    cf = fonts + currentFontIndex;

    if(cf->hasEmbeddedShadow)
        drawShadow = false;

    if(drawShadow)
    {
        // The color of the text itself.
        gl.GetIntegerv(DGL_RGBA, origColor);
        gl.Color4ub(0, 0, 0, origColor[3] * shadowAlpha);
    }

    // Set the texture.
    gl.Bind(cf->texture);

    gl.MatrixMode(DGL_TEXTURE);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Scalef(1.f / cf->texWidth, 1.f / cf->texHeight, 1.f);

    // Print it.
    gl.Begin(DGL_QUADS);

    if(drawShadow)
    {
        int startX = x;
        int startY = y;

        x += shadowX;
        y += shadowY;

        for(i = 0; i < len; ++i)
        {
            // First draw the shadow.
            jfrchar_t *ch = cf->chars + (byte) text[i];

            // Upper left.
            gl.TexCoord2f(ch->x, ch->y);
            gl.Vertex2f(x, y);
            // Upper right.
            gl.TexCoord2f(ch->x + ch->w, ch->y);
            gl.Vertex2f(x + ch->w, y);
            // Lower right.
            gl.TexCoord2f(ch->x + ch->w, ch->y + ch->h);
            gl.Vertex2f(x + ch->w, y + ch->h);
            // Lower left.
            gl.TexCoord2f(ch->x, ch->y + ch->h);
            gl.Vertex2f(x, y + ch->h);
            // Move on.
            x += ch->w;
        }

        x = startX;
        y = startY;
    }

    if(drawShadow)
        gl.Color4ub(origColor[0], origColor[1], origColor[2], origColor[3]);

    x -= cf->marginWidth;
    y -= cf->marginHeight;

    for(i = 0; i < len; ++i)
    {
        jfrchar_t *ch = cf->chars + (byte) text[i];

        // Upper left.
        gl.TexCoord2f(ch->x, ch->y);
        gl.Vertex2f(x, y);
        // Upper right.
        gl.TexCoord2f(ch->x + ch->w, ch->y);
        gl.Vertex2f(x + ch->w, y);
        // Lower right.
        gl.TexCoord2f(ch->x + ch->w, ch->y + ch->h);
        gl.Vertex2f(x + ch->w, y + ch->h);
        // Lower left.
        gl.TexCoord2f(ch->x, ch->y + ch->h);
        gl.Vertex2f(x, y + ch->h);
        // Move on.
        step = ch->w - 2*cf->marginWidth;
        width += step;
        x += step;
    }
    gl.End();

    gl.MatrixMode(DGL_TEXTURE);
    gl.PopMatrix();
    return width;
}

int FR_TextOut(const char *text, int x, int y)
{
    return FR_CustomShadowTextOut(text, x, y, 0, 0, 0);
}

int FR_ShadowTextOut(const char *text, int x, int y)
{
    return FR_CustomShadowTextOut(text, x, y, 2, 2, .5f);
}
